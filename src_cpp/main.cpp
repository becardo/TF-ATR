#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <fstream>
#include <iomanip>
#include <string>
#include <functional>
#include <condition_variable>

// Inclusão dos Buffers e Blocos de Execução
#include "tasks.hpp"
#include "shared_buffers.hpp"
#include "NavigationManager.hpp"
#include "PIDController.hpp"
#include "Coletor.hpp"
#include "Camera.hpp"

// Thread de comunicação MQTT
void t_comunicacao_mqtt();

NavBuffer nav;
SensorBuffer sensor;

// Thread responsável por ler os comandos do usuário (joystick/botoes) e definir o Setpoint
void t_comando_navegacao(NavBuffer& nav, SensorBuffer& sensor) {
    NavigationManager nav_manager; 

    boost::asio::io_context io_com_nav;
    boost::asio::steady_timer timer_com_nav(io_com_nav, boost::asio::chrono::milliseconds(80)); 

    std::function<void(const boost::system::error_code&)> loop_comando;

    loop_comando = ([&](const boost::system::error_code& erro){
        if(erro) return;

        // Leitura do status da inspeção (câmera)
        bool em_inspecao = false;
        {
            // Bloqueia o mutex da câmera apenas para ler a flag de alarme
            std::lock_guard<std::mutex> lock(sensor.mtx_camera);
            em_inspecao = sensor.e_inspecao;
        }

        // Mocks de entrada, vindos do MQTT
        bool iniciado = false;
        bool c_automatico = false; 
        bool c_man = false;         
        bool c_para = false;       
        double velocidade_joystick = 0.0; 

        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            iniciado = nav.sistema_iniciado;
            c_automatico = nav.e_automatico;
            c_man = !nav.e_automatico;
            c_para = nav.c_para;
            velocidade_joystick = nav.velocidade_joystick;
        }

        // Intercepçao: enquando não for iniciada, robô não se mexe
        if (!iniciado) {
            velocidade_joystick = 0.0;
            c_para = true; // Mantém o freio puxado
        }

        // Leitura da Telemetria 
        float teto_atual = 0.0;

        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            teto_atual = sensor.ultima_leitura_lidar;
        }

        // Fim da inspeção normal (Túnel 1 - final aberto)
        if (teto_atual > 15.0 && iniciado && !nav.inspecao_concluida) {
            std::cout << "\n [MISSÃO] Fim do túnel detectado.\n";
            nav.inspecao_concluida = true;
        }

        // Finalização dos motores, inspeção finalizada
        if (!iniciado || nav.inspecao_concluida) {
            velocidade_joystick = 0.0;
            c_para = true; 
        }

        // Passa para o NavigationManager processar
        // Atualiza o modo (Manual/Automática) e calcula a nova velocidade
        nav_manager.updateMode(c_automatico, c_man);
        nav_manager.processInputs(velocidade_joystick, c_para, em_inspecao);

        int posicao_atual = 0;
        {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            posicao_atual = sensor.ultima_leitura_encoder;
        }

        // Zona crítica: Escrita no Buffer compartilhado
        {
            std::lock_guard<std::mutex> lock(nav.mtx);

            double setpoint_calculado = nav_manager.getTargetSpeed();

            if (posicao_atual <= 0 && setpoint_calculado < 0.0) {
                nav.j_sp_velocidade = 0.0; 
            } 
            else {
                nav.j_sp_velocidade = setpoint_calculado;
            }
        }

        timer_com_nav.expires_at(timer_com_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_com_nav.async_wait(loop_comando);
    });

    timer_com_nav.async_wait(loop_comando);
    io_com_nav.run();
}

// Thread responsável pelo controle PID em malha fechada
// Lê o setpoint desejado, comparar com a velocidade real e calcula
// o esforço de aceleração necessário
void t_controle_navegacao(NavBuffer& nav, SensorBuffer& sensor) {
    PIDController pid(1.0, 0.1, 0.05, 0.08);
    // Kp=1.0, Ki=1.0, Kd=0.05, dt=0.08

    boost::asio::io_context io_PID_nav;
    boost::asio::steady_timer timer_PID_nav(io_PID_nav, boost::asio::chrono::milliseconds(80));

    std::function<void(const boost::system::error_code&)> loop_PID;

    loop_PID = ([&](const boost::system::error_code& erro){
        if(erro) return;

        // leitura do setpoint definido por t_comando_navegacao
        float setpoint_atual = 0.0;
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            setpoint_atual = nav.j_sp_velocidade;
        }

        // Leitura da física real
        double velocidade_atual_robo = 0.0;
        int posicao_atual_encoder = 0;
        {
            // Tranca a porta para ler a telemetria atualizada pelo MQTT com segurança
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            velocidade_atual_robo = sensor.velocidade_real_medida;
            posicao_atual_encoder = sensor.ultima_leitura_encoder;
        }

        // Cálculo do PID
        if (setpoint_atual == 0.0) {
            pid.reset();
        }

        double saida_aceleracao = pid.compute(static_cast<double>(setpoint_atual), velocidade_atual_robo);

        // Trava de segurança para fim de curso do túnel estendido de 250 metros
        if (posicao_atual_encoder >= 250) {
            saida_aceleracao = 0.0;
            pid.reset(); // Zera a memória integral do PID
        }

        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[PID] Setpoint: " << setpoint_atual << " m/s | Aceleracao calculada: " << saida_aceleracao << " m/s²\n";
        }
        
        // Escrita da aceleração no Buffer para a Thread MQTT enviar ao Python
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            nav.o_aceleracao = saida_aceleracao;
        }

        timer_PID_nav.expires_at(timer_PID_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_PID_nav.async_wait(loop_PID);
    });

    timer_PID_nav.async_wait(loop_PID);
    io_PID_nav.run();
}



int main() {
    std::cout << "Iniciando Sistema de Inspecao do Tunel (Módulos Integrados via MQTT)...\n";

    // Instanciação assíncrona paralela das 7 threads de processamento do sistema
    std::thread th_comando(t_comando_navegacao, std::ref(nav), std::ref(sensor));
    std::thread th_controle(t_controle_navegacao, std::ref(nav), std::ref(sensor));
    std::thread th_distancia(t_calculo_distancia, std::ref(nav), std::ref(sensor));
    std::thread th_teto(t_reconstrucao_teto, std::ref(sensor));
    std::thread th_camera(t_inspecao_camera, std::ref(sensor));
    std::thread th_coletor(t_coletor_dados, std::ref(sensor), std::ref(nav));
    std::thread thread_mqtt(t_comunicacao_mqtt); 

    // Sincronismo estruturado de encerramento determinístico
    th_comando.join();
    th_controle.join();
    th_distancia.join();
    th_teto.join();
    th_camera.join();
    th_coletor.join();
    
    thread_mqtt.join(); 

    return 0;
}