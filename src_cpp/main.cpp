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


void t_inspecao_camera(SensorBuffer& sensor) {
    {
        std::unique_lock<std::mutex> lk(sensor.mtx_camera);
        sensor.cv_camera.wait(lk, [&]{ return sensor.o_liga_camera; });
    }

    while (true) {
        const int size = 150;
        std::vector<std::vector<double>> A(size, std::vector<double>(size, 1.1));
        std::vector<std::vector<double>> B(size, std::vector<double>(size, 2.2));
        std::vector<std::vector<double>> C(size, std::vector<double>(size, 0.0));

        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                for (int k = 0; k < size; ++k) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(sensor.mtx_camera);
            sensor.e_inspecao = (C[0][0] > 1000.0); 
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Thread responsável por ler os comandos (joystick/botoes) e definir o Setpoint
// Roda ciclicamente a cada 80ms
void t_comando_navegacao(NavBuffer& nav) {
    NavigationManager nav_manager; 

    boost::asio::io_context io_com_nav; // Maestro para gerenciar os eventos da thread do Comando de Navegação.
    boost::asio::steady_timer timer_com_nav(io_com_nav, boost::asio::chrono::milliseconds(80)); 

    // Função apara chamar o loop a ser executado
    std::function<void(const boost::system::error_code&)> loop_comando;

    // Função que o timer executa quando "toca"
    loop_comando = ([&](const boost::system::error_code& erro){

        if(erro) return;

        // ----- Mock de entradas -----
        // (Serão substítuidos posteriormente por leiturais reais, via interface/MQTT a ser desenvolvida)
        bool c_automatico = false; // Comando para passar para o modo automático
        bool c_man = true;         // Comando para passar para o modo manual
        bool c_para = false;       // Comando para parar o robô

        double velocidade_joystick = 30.0; // Simulação do sinal vindo do joystick para o Setpoint

        // Processamento da lógica de estado da navegação
        nav_manager.updateMode(c_automatico, c_man);
        nav_manager.processInputs(velocidade_joystick, c_para);

        // ----- Zona Crítica: Escrita no Buffer Compartilhado -----
        nav.mtx.lock(); // Tranca o cadeado antes de mexer na memória
        nav.e_automatico = (nav_manager.getMode() == NavMode::AUTOMATIC);
        nav.j_sp_velocidade = static_cast<int>(nav_manager.getTargetSpeed());
        nav.mtx.unlock(); // Destranca o cadeado após alterar a memória

        // Loop de repetição do timer: sempre que o tempo expirar, somam-se 80ms 
        // e o timer é disparado novamente.
        timer_com_nav.expires_at(timer_com_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_com_nav.async_wait(loop_comando);
    });

    // Gatilho de início da contagem: 80ms
    timer_com_nav.async_wait(loop_comando);
    io_com_nav.run();

}

// Thread responsável pelo controle PID
// Roda ciclicamente a cada 80ms 
void t_controle_navegacao(NavBuffer& nav) {
    // Kp, Ki, Kd iniciais e tempo de amostragem de 80ms (0.08)
    PIDController pid(1.0, 0.1, 0.05, 0.08);

    boost::asio::io_context io_PID_nav;
    boost::asio::steady_timer timer_PID_nav(io_PID_nav, boost::asio::chrono::milliseconds(80));

    std::function<void(const boost::system::error_code&)> loop_PID;

    loop_PID = ([&](const boost::system::error_code& erro){

        if(erro) return;

        int setpoint_atual = 0;

        // --- Zona Crítica: Leitura do Setpoint ---
        nav.mtx.lock(); // Tranca para ler a velocidade que o robo deve alcançar
        setpoint_atual = nav.j_sp_velocidade;
        nav.mtx.unlock(); // Destranca após a leitura

        // Mock da velocidade atual 
        double velocidade_atual_robo = 0.0; 

        // Calcula o PID fora da Zona Crítica, para não travar o sistema durante o cálculo
        double saida_aceleracao = pid.compute(static_cast<double>(setpoint_atual), velocidade_atual_robo);

        std::cout << "[PID] Setpoint: " << setpoint_atual << " | Aceleracao calculada: " << saida_aceleracao << "\n";
        
        // ----- Zona Crítica: Escrita da Aceleração -----
        nav.mtx.lock(); // Tranca para escrever o resultado
        nav.o_aceleracao = static_cast<int>(saida_aceleracao);
        nav.mtx.unlock(); // Destranca

        timer_PID_nav.expires_at(timer_PID_nav.expiry() + boost::asio::chrono::milliseconds(80));
        timer_PID_nav.async_wait(loop_PID);
    });

    timer_PID_nav.async_wait(loop_PID);
    io_PID_nav.run();
}

void t_coletor_dados(SensorBuffer& sensor) {
    // Configuração do arquivo de saída
    std::ofstream log_file("telemetria_inspecao.csv", std::ios::app);
    
    if (!log_file.is_open()) {
        std::cerr << "[ERRO] Falha ao abrir o arquivo de log!" << std::endl;
        return;
    }

    // Cabeçalho do CSV (opcional, se o arquivo for novo)
    // log_file << "Timestamp,Posicao_X,Posicao_Y_Teto,Nivel_Confianca\n";

    boost::asio::io_context io_log;
    // Periodicidade de 100ms para o log (ajustável conforme necessidade)
    boost::asio::steady_timer timer_log(io_log, boost::asio::chrono::milliseconds(100));

    std::function<void(const boost::system::error_code&)> loop_log;

    loop_log = ([&](const boost::system::error_code& erro) {
        if (erro) return;

        double x, y, conf;
        long long ts = std::chrono::system_clock::now().time_since_epoch().count();

        // ----- Zona Crítica: Leitura dos dados dos Buffers -----
        {
            std::lock_guard<std::mutex> lock(sensor.mtx);
            x = sensor.posicao_x;
            y = sensor.posicao_y_teto;
            conf = sensor.nivel_confianca;
        }

        // Gravação otimizada: Escrevemos no buffer do stream
        // O uso de '\n' em vez de std::endl evita o flush forçado a cada linha, economizando I/O
        log_file << ts << "," 
                 << std::fixed << std::setprecision(3) << x << "," 
                 << y << "," 
                 << std::setprecision(2) << conf << "\n";

        // Feedback no console para debug (opcional, pode ser removido para performance)
        // std::cout << "[LOG] Dados gravados em disco.\n";

        timer_log.expires_at(timer_log.expiry() + boost::asio::chrono::milliseconds(100));
        timer_log.async_wait(loop_log);
    });

    timer_log.async_wait(loop_log);
    io_log.run();

    if (log_file.is_open()) {
        log_file.close();
    }
}

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
