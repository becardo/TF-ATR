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

// --- Odometria: Calcula velocidade com base no Encoder do MQTT ---
void t_calculo_distancia(NavBuffer& nav, SensorBuffer& sensor) {
    Odometria odo; 
    int distancia_anterior = 0;
    double dt = 0.02; // 20ms rígidos do timer

    boost::asio::io_context io_odo;
    boost::asio::steady_timer timer_odo(io_odo, boost::asio::chrono::milliseconds(20));

    std::function<void(const boost::system::error_code&)> loop_odo;

    loop_odo = ([&](const boost::system::error_code& erro) { 
        if(erro) return;

        int dist_atual = distancia_anterior;

        // ZONA CRÍTICA: Captura segura da última posição do encoder descarregada pelo MQTT
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            dist_atual = sensor.ultimo_encoder_recebido;
        }

        // Cálculo derivativo da velocidade real (Delta X / Delta T) em metros
        double velocidade_medida = (dist_atual - distancia_anterior) / dt;

        // Salva a velocidade calculada para a thread do PID ler
        {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            sensor.velocidade_real_medida = velocidade_medida;
        }
        
        // Atualiza a memória de rastreamento para o próximo ciclo de 20ms
        distancia_anterior = dist_atual;

        timer_odo.expires_at(timer_odo.expiry() + boost::asio::chrono::milliseconds(20));
        timer_odo.async_wait(loop_odo);
    });

    timer_odo.async_wait(loop_odo);
    io_odo.run();
}

// --- LIDAR / Reconstrução do teto: Processa média móvel e dispara Alarme ---
void t_reconstrucao_teto(SensorBuffer& sensor) {
    LidarFilter filtro(5); 

    // Inicializa o histórico do filtro circular com a altura ideal
    for(int i = 0; i < 5; i++) { filtro.calcular(10.0f); }

    boost::asio::io_context io_lidar;
    boost::asio::steady_timer timer_lidar(io_lidar, boost::asio::chrono::milliseconds(100));

    std::function<void(const boost::system::error_code&)> loop_lidar;

    loop_lidar = ([&](const boost::system::error_code& erro){ 
        if(erro) return;

        float leitura_lidar_real = 10.0f;

        // CORREÇÃO: Captura o dado bruto da rede ANTES de passar pelo filtro
        {
            std::lock_guard<std::mutex> lock_memoria(sensor.mtx_leituras);
            leitura_lidar_real = sensor.ultima_leitura_lidar;
        }

        // Processa o dado no Buffer Circular de Média Móvel para eliminar ruído gaussiano
        float media = filtro.calcular(leitura_lidar_real);

        bool falha_detectada = false;
        float altura_ideal = 10.0f;
        float margem_erro = 0.5f;

        // Lógica de Detecção com logs protegidos para o console
        if (media > (altura_ideal + margem_erro)){
            {
                std::lock_guard<std::mutex> lock_tela(mtx_console);
                std::cout << "[LIDAR] FALHA: BURACO detectado! (Média: " << media << "m)\n";
            }
            falha_detectada = true;
        }
        else if(media < (altura_ideal - margem_erro)){
            {
                std::lock_guard<std::mutex> lock_tela(mtx_console);
                std::cout << "[LIDAR] FALHA: SALIENCIA detectada! (Média: " << media << "m)\n";
            }
            falha_detectada = true;
        }

        // Disparo reativo do alarme por software para a Câmera e Slowdown
        if (falha_detectada) {
            {
                std::lock_guard<std::mutex> lock_camera(sensor.mtx_camera);
                sensor.e_inspecao = true;     
                sensor.o_liga_camera = true;  
            } 
            sensor.cv_camera.notify_one(); 
        }

        timer_lidar.expires_at(timer_lidar.expiry() + boost::asio::chrono::milliseconds(100));
        timer_lidar.async_wait(loop_lidar);
    });

    timer_lidar.async_wait(loop_lidar);
    io_lidar.run();
}