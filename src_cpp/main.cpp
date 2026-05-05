#include <iostream>
#include <thread>
#include <chrono>

// Inclusão dos Buffers e Blocos de Execução
#include "shared_buffers.hpp"
#include "NavigationManager.hpp"
#include "PIDController.hpp"

// Thread responsável por ler os comandos (joystick/botoes) e definir o Setpoint
// Roda ciclicamente a cada 80ms
void t_comando_navegacao(NavBuffer& nav) {
    NavigationManager nav_manager; 

    while (true) {
        // ----- Mock de entradas -----
        // (Serão substítuidos posteriormente por leiturais reais, via interface/MQTT a ser desenvolvida)
        bool botao_auto = false;
        bool botao_manual = true;
        double velocidade_joystick = 30.0; 
        bool botao_parar = false;

        // Processamento da lógica de estado da navegação
        nav_manager.updateMode(botao_auto, botao_manual);
        nav_manager.processInputs(velocidade_joystick, botao_parar);

        // ----- Zona Crítica: Escrita no Buffer Compartilhado -----
        nav.mtx.lock(); // Tranca o cadeado antes de mexer na memória
        nav.modo_automatico = (nav_manager.getMode() == NavMode::AUTOMATIC);
        nav.setpoint_velocidade = static_cast<int>(nav_manager.getTargetSpeed());
        nav.mtx.unlock(); // Destranca o cadeado após alterar a memória

        // Aguarda o fim do ciclo de 80ms
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

// Thread responsável pelo controle PID
// Roda ciclicamente a cada 80ms 
void t_controle_navegacao(NavBuffer& nav) {
    // Kp, Ki, Kd iniciais e tempo de amostragem de 80ms (0.08)
    PIDController pid(1.0, 0.1, 0.05, 0.08);

    while (true) {
        int setpoint_atual = 0;

        // --- Zona Crítica: Leitura do Setpoint ---
        nav.mtx.lock(); // Tranca para ler a velocidade que o robo deve alcançar
        setpoint_atual = nav.setpoint_velocidade;
        nav.mtx.unlock(); // Destranca após a leitura

        // Mock da velocidade atual 
        double velocidade_atual_robo = 0.0; 

        // Calcula o PID fora da Zona Crítica, para não travar o sistema durante o cálculo
        double saida_aceleracao = pid.compute(static_cast<double>(setpoint_atual), velocidade_atual_robo);

        //std::cout << "[PID] Setpoint: " << setpoint_atual << " | Aceleracao calculada: " << saida_aceleracao << "\n";
        
        // ----- Zona Crítica: Escrita da Aceleração -----
        nav.mtx.lock(); // Tranca para escrever o resultado
        nav.aceleracao = static_cast<int>(saida_aceleracao);
        nav.mtx.unlock(); // Destranca

        // Aguarda o fim do ciclo de 80ms
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

// Assinatura dos sensores
extern void t_calculo_distancia(SensorBuffer& sensor);
extern void t_reconstrucao_teto(SensorBuffer& sensor);
extern void t_inspecao_camera(SensorBuffer& sensor);
extern void t_coletor_dados(SensorBuffer& sensor);

int main() {
    std::cout << "Iniciando Sistema de Inspecao do Tunel...\n";

    NavBuffer nav;
    SensorBuffer sensor;

    // Dispara as 6 threads passando a referência da memória
    std::thread th_comando(t_comando_navegacao, std::ref(nav));
    std::thread th_controle(t_controle_navegacao, std::ref(nav));
    std::thread th_distancia(t_calculo_distancia, std::ref(sensor));
    std::thread th_teto(t_reconstrucao_teto, std::ref(sensor));
    std::thread th_camera(t_inspecao_camera, std::ref(sensor));
    std::thread th_coletor(t_coletor_dados, std::ref(sensor));

    // Aguarda execução infinita
    th_comando.join();
    th_controle.join();
    th_distancia.join();
    th_teto.join();
    th_camera.join();
    th_coletor.join();

    return 0;
}