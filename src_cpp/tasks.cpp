#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include <fstream>
#include <string>
#include <vector>
#include <functional>

#include "tasks.hpp"
#include "shared_buffers.hpp"
#include "Lidar.hpp"
#include "Odometria.hpp"

/* Contém as threads responsáveis por ler a odometria e mapear o teto com LIDAR
*/

// --- Odometria: Produtor de dados de distância ---
void t_calculo_distancia(NavBuffer& nav, SensorBuffer& sensor) {
    Odometria odo; 

    double velocidade_fisica = 0.0;
    double posicao_fisica = 0.0;
    int ultimo_metro_inteiro = 0;
    int distancia_anterior = 0;
    double dt = 0.02; // 20ms do timer

    bool pulso_fisico_encoder = false;

    boost::asio::io_context io_odo;
    boost::asio::steady_timer timer_odo(io_odo, boost::asio::chrono::milliseconds(20));

    std::function<void(const boost::system::error_code&)> loop_odo;

    loop_odo = ([&](const boost::system::error_code& erro) { 
        if(erro) return;

        // Freando o robô no mundo real - Integração
        double aceleracao = nav.o_aceleracao * 0.1; 

        velocidade_fisica += aceleracao * dt;
        if(velocidade_fisica < 0.0) velocidade_fisica = 0.0; // Impede o robô de dar ré
        
        posicao_fisica += velocidade_fisica * dt;

        // Encoder só vira quando acumula 1 metro
        if ((int)posicao_fisica > ultimo_metro_inteiro) {
            pulso_fisico_encoder = !pulso_fisico_encoder;
            ultimo_metro_inteiro = (int)posicao_fisica;
        }

        int dist_x = odo.atualizar(pulso_fisico_encoder);

        // Derivação para encontrar a velocidade
        double variacao_distancia = dist_x - distancia_anterior;
        double velocidade_medida = variacao_distancia / dt;

        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[ODOMETRIA]: Distancia percorrida: " << dist_x << " m\n";
        }
        
        // Atualiza a memória para o próximo ciclo de 20ms
        distancia_anterior = dist_x;

        // Guarda a velocidade medida no buffer para a thread do PID ler
        sensor.velocidade_real_medida = velocidade_medida;

        Medicao m;
        m.i_encoder = dist_x;
        // Timestamp simples para o coletor
        m.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m.i_lidar = sensor.ultima_leitura_lidar;
        m.nivel_confianca = 100; // Valor padrão inicial

        {
            // Tranca a porta para escrever a velocidade e ler o Lidar com segurança
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            sensor.velocidade_real_medida = velocidade_medida;
            m.i_lidar = sensor.ultima_leitura_lidar; 
        }

        // ----- Zona Crítica: Guardar na fila para o Coletor -----
        {
            std::lock_guard<std::mutex> lock_fila(sensor.mtx_fila);
            sensor.fila_medicoes.push(m);
        }

        sensor.cv_coletor.notify_one();

        timer_odo.expires_at(timer_odo.expiry() + boost::asio::chrono::milliseconds(20));
        timer_odo.async_wait(loop_odo);
    });

    timer_odo.async_wait(loop_odo);
    io_odo.run();
}

// --- LIDAR / Reconstrução do teto: Identifica falhas e aciona a câmera ---
void t_reconstrucao_teto(SensorBuffer& sensor) {
    LidarFilter filtro(5); 

    // Inicializa o filtro com valor neutro
    for(int i = 0; i < 5; i++) { filtro.calcular(10.0f); }

    std::vector<float> valores_teste;
    std::ifstream arquivo_teto("simulacao_superficie.csv");
    std::string linha;

    // Tenta carregar o arquivo CSV
    if (arquivo_teto.is_open()) {
        while (std::getline(arquivo_teto, linha)) {
            if(linha.empty()) continue;
            try {
                valores_teste.push_back(std::stof(linha));
            } catch (...) { }
        }
        arquivo_teto.close();
        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[SISTEMA] Arquivo simulacao_superficie.csv carregado com " << valores_teste.size() << " leituras.\n";
        }
        
    }

    // Proteção: Se o arquivo falhou ou está vazio, garante que o programa não dê crash
    if (valores_teste.empty()) {
        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cerr << "[AVISO] Arquivo de simulacao vazio ou nao encontrado. Usando teto plano.\n";
        }
        
        valores_teste.push_back(10.0f);
    }

    int indice_teste = 0;
    boost::asio::io_context io_lidar;
    boost::asio::steady_timer timer_lidar(io_lidar, boost::asio::chrono::milliseconds(100));

    std::function<void(const boost::system::error_code&)> loop_lidar;

    loop_lidar = ([&](const boost::system::error_code& erro){ 
        if(erro) return;

        // Leitura do valor atual do vetor (circular)
        float valor_atual = valores_teste[indice_teste];
        indice_teste = (indice_teste + 1) % valores_teste.size();

        float media = filtro.calcular(valor_atual);

        // Proteção: Tranca a porta para atualizar o SensorBuffer
        {
            std::lock_guard<std::mutex> lock_memoria(sensor.mtx_leituras);
            sensor.ultima_leitura_lidar = media;
        }

        // Proteção: Imprime a medida atual para o usuário ver
        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[LIDAR] Medida atual: " << media << " m\n";
        }

        bool falha_detectada = false;
        float altura_ideal = 10.0f;
        float margem_erro = 0.5f;

        // Lógica de Detecção
        if (media > (altura_ideal + margem_erro)){
            {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[LIDAR] FALHA: BURACO detectado! (Dist/Altura: " << media << "m)\n";
            }
            
            falha_detectada = true;
        }
        else if(media < (altura_ideal - margem_erro)){
            {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[LIDAR] FALHA: SALIENCIA detectada! (Dist/Altura: " << media << "m)\n";
            }
            
            falha_detectada = true;
        }

        // Se achou uma falha, aciona o Alarme e sinaliza o Slowdown
        if (falha_detectada) {
            {
                std::lock_guard<std::mutex> lock_camera(sensor.mtx_camera);
                sensor.e_inspecao = true;     // Ativa o Slowdown no NavigationManager
                sensor.o_liga_camera = true;  // Acorda a thread da Câmera
            } 
            sensor.cv_camera.notify_one(); 
        }

        timer_lidar.expires_at(timer_lidar.expiry() + boost::asio::chrono::milliseconds(100));
        timer_lidar.async_wait(loop_lidar);
    });

    timer_lidar.async_wait(loop_lidar);
    io_lidar.run();
}