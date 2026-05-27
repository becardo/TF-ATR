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

// Protótipo da nova thread de rede desenvolvida pela equipe
void t_comunicacao_mqtt();

NavBuffer nav;
SensorBuffer sensor;

// Thread responsável por ler os comandos (joystick/botoes) e definir o Setpoint
void t_comando_navegacao(NavBuffer& nav, SensorBuffer& sensor) {
    NavigationManager nav_manager; 

    boost::asio::io_context io_com_nav;
    boost::asio::steady_timer timer_com_nav(io_com_nav, boost::asio::chrono::milliseconds(80)); 

    std::function<void(const boost::system::error_code&)> loop_comando;

    loop_comando = ([&](const boost::system::error_code& erro){
        if(erro) return;

        // ----- Mock de entradas -----
        bool c_automatico = false; 
        bool c_man = true;         
        bool c_para = false;       

        double velocidade_joystick = 5.0; 

        bool em_inspecao = false;
        {
            // Bloqueia o mutex da câmera apenas para ler a flag de alarme
            std::lock_guard<std::mutex> lock(sensor.mtx_camera);
            em_inspecao = sensor.e_inspecao;
        }

        // Processamento da lógica de estado da navegação
        nav_manager.updateMode(c_automatico, c_man);
        nav_manager.processInputs(velocidade_joystick, c_para, em_inspecao);

        // ----- Zona Crítica: Escrita no Buffer Compartilhado -----
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            nav.e_automatico = (nav_manager.getMode() == NavMode::AUTOMATIC);
            nav.j_sp_velocidade = nav_manager.getTargetSpeed();
        }

        timer_com_nav.expires_at(timer_com_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_com_nav.async_wait(loop_comando);
    });

    timer_com_nav.async_wait(loop_comando);
    io_com_nav.run();
}

// Thread responsável pelo controle PID em malha fechada
void t_controle_navegacao(NavBuffer& nav, SensorBuffer& sensor) {
    PIDController pid(1.0, 0.1, 0.05, 0.08);

    boost::asio::io_context io_PID_nav;
    boost::asio::steady_timer timer_PID_nav(io_PID_nav, boost::asio::chrono::milliseconds(80));

    std::function<void(const boost::system::error_code&)> loop_PID;

    loop_PID = ([&](const boost::system::error_code& erro){
        if(erro) return;

        float setpoint_atual = 0.0;
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            setpoint_atual = nav.j_sp_velocidade;
        }

        double velocidade_atual_robo = 0.0;
        int posicao_atual_encoder = 0;
        {
            // Tranca a porta para ler a telemetria atualizada pelo MQTT com segurança
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            velocidade_atual_robo = sensor.velocidade_real_medida;
            posicao_atual_encoder = sensor.ultimo_encoder_recebido;
        }

        double saida_aceleracao = pid.compute(static_cast<double>(setpoint_atual), velocidade_atual_robo);

        // CORREÇÃO: Trava de segurança para fim de curso do túnel estendido de 1000 metros
        if (posicao_atual_encoder >= 100) {
            saida_aceleracao = 0.0;
            pid.reset();
        }

        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[PID] Setpoint: " << setpoint_atual << " m/s | Aceleracao calculada: " << saida_aceleracao << " m/s²\n";
        }
        
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
    std::thread th_coletor(t_coletor_dados, std::ref(sensor));
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