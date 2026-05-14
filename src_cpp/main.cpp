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

// Thread responsável por ler os comandos e definir o Setpoint
// Agora recebe "sensor" para verificar a necessidade de slowDown
void t_comando_navegacao(NavBuffer& nav, SensorBuffer& sensor) {
    NavigationManager nav_manager; 

    boost::asio::io_context io_com_nav;
    boost::asio::steady_timer timer_com_nav(io_com_nav, boost::asio::chrono::milliseconds(80)); 

    std::function<void(const boost::system::error_code&)> loop_comando;

    loop_comando = ([&](const boost::system::error_code& erro){
        if(erro) return;

        // ----- Mock de entradas (Simulando interface/MQTT) -----
        bool c_automatico = true; // Simulando modo automático ativado
        bool c_man = false;
        bool c_para = false;
        double velocidade_joystick = 0.0; 

        // --- Verificação de Inspeção Ativa (SlowDown) ---
        bool em_inspecao = false;
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_camera);
            em_inspecao = sensor.e_inspecao; // Lê a flag de falha do LIDAR
        }

        // Processamento da lógica de navegação
        nav_manager.updateMode(c_automatico, c_man);
        
        // Aplica entradas e o slowDown se houver falha
        nav_manager.processInputs(velocidade_joystick, c_para, em_inspecao);

        // ----- Zona Crítica: Escrita no Buffer de Navegação -----
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            nav.e_automatico = (nav_manager.getMode() == NavMode::AUTOMATIC);
            nav.j_sp_velocidade = static_cast<int>(nav_manager.getTargetSpeed());
        }

        timer_com_nav.expires_at(timer_com_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_com_nav.async_wait(loop_comando);
    });

    timer_com_nav.async_wait(loop_comando);
    io_com_nav.run();
}

// Thread responsável pelo controle PID
void t_controle_navegacao(NavBuffer& nav) {
    // Parâmetros do PID calibrados para 80ms
    PIDController pid(1.0, 0.1, 0.05, 0.08);

    boost::asio::io_context io_PID_nav;
    boost::asio::steady_timer timer_PID_nav(io_PID_nav, boost::asio::chrono::milliseconds(80));

    std::function<void(const boost::system::error_code&)> loop_PID;

    loop_PID = ([&](const boost::system::error_code& erro){
        if(erro) return;

        int setpoint_atual = 0;
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            setpoint_atual = nav.j_sp_velocidade;
        }

        // Mock da velocidade atual (seria lida dos sensores reais)
        double velocidade_atual_robo = 0.0; 

        // Cálculo do PID
        double saida_aceleracao = pid.compute(static_cast<double>(setpoint_atual), velocidade_atual_robo);

        std::cout << "[PID] Setpoint: " << setpoint_atual << " | Saída: " << saida_aceleracao << "\n";
        
        {
            std::lock_guard<std::mutex> lock(nav.mtx);
            nav.o_aceleracao = static_cast<int>(saida_aceleracao);
        }

        timer_PID_nav.expires_at(timer_PID_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_PID_nav.async_wait(loop_PID);
    });

    timer_PID_nav.async_wait(loop_PID);
    io_PID_nav.run();
}

int main() {
    std::cout << "--- Iniciando Sistema de Inspeção de Túnel (ATR) ---\n";

    NavBuffer nav;
    SensorBuffer sensor;

    // Inicialização das threads
    // Note que th_comando agora recebe nav e sensor
    std::thread th_comando(t_comando_navegacao, std::ref(nav), std::ref(sensor));
    std::thread th_controle(t_controle_navegacao, std::ref(nav));
    
    std::thread th_distancia(t_calculo_distancia, std::ref(sensor));
    std::thread th_teto(t_reconstrucao_teto, std::ref(sensor));
    std::thread th_camera(t_inspecao_camera, std::ref(sensor));
    std::thread th_coletor(t_coletor_dados, std::ref(sensor));

    // Junção das threads (Loop infinito)
    th_comando.join();
    th_controle.join();
    th_distancia.join();
    th_teto.join();
    th_camera.join();
    th_coletor.join();

    return 0;
}