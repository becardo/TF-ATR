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

// Odometria: Calcula velocidade com base no Encoder do MQTT
void t_calculo_distancia(NavBuffer& nav, SensorBuffer& sensor) {
    Odometria odo; 
    int distancia_anterior = 0;
    double dt = 0.02; 

    boost::asio::io_context io_odo;
    boost::asio::steady_timer timer_odo(io_odo, boost::asio::chrono::milliseconds(20));

    std::function<void(const boost::system::error_code&)> loop_odo;

    loop_odo = ([&](const boost::system::error_code& erro) { 
        if(erro) return;

        int dist_atual = distancia_anterior;

        // Captura da última posição do encoder descarregada pelo MQTT
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            dist_atual = sensor.ultima_leitura_encoder;
        }

        // Cálculo derivativo da velocidade real (Delta X / Delta T) em metros
        double velocidade_medida = (dist_atual - distancia_anterior) / dt;

        // Salva a velocidade calculada para a thread do PID ler
        /* {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            sensor.velocidade_real_medida = velocidade_medida;
        } */
        
        // Atualiza a memória de rastreamento para o próximo ciclo de 20ms
        distancia_anterior = dist_atual;

        timer_odo.expires_at(timer_odo.expiry() + boost::asio::chrono::milliseconds(20));
        timer_odo.async_wait(loop_odo);
    });

    timer_odo.async_wait(loop_odo);
    io_odo.run();
}

// LIDAR / Reconstrução do teto: Processa média móvel e dispara Alarme 
void t_reconstrucao_teto(SensorBuffer& sensor) {
    LidarFilter filtro(5);

    // Inicializa histórico do filtro
    for (int i = 0; i < 5; i++) {
        filtro.calcular(10.0f);
    }

    bool alarme_ativo = false;

    boost::asio::io_context io_lidar;
    boost::asio::steady_timer timer_lidar(
        io_lidar,
        boost::asio::chrono::milliseconds(100)
    );

    std::function<void(const boost::system::error_code&)> loop_lidar;

    loop_lidar = ([&](const boost::system::error_code& erro) {
        if (erro) return;

        float leitura_lidar_real = 10.0f;

        // Leitura do último valor recebido
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            leitura_lidar_real = sensor.ultima_leitura_lidar;
        }

        // Média móvel
        float media = filtro.calcular(leitura_lidar_real);

        const float altura_ideal = 10.0f;
        const float margem_erro = 0.5f;

        bool falha_detectada =
            (media > altura_ideal + margem_erro) ||
            (media < altura_ideal - margem_erro);

        // Detectou falha
        if (falha_detectada) {

            // Só registra e dispara a câmera na primeira vez
            if (!alarme_ativo) {

                {
                    std::lock_guard<std::mutex> lock_tela(mtx_console);

                    if (media > altura_ideal + margem_erro) {
                        std::cout
                            << "[LIDAR] FALHA: BURACO detectado! (Media: "
                            << media << "m)\n";
                    } else {
                        std::cout
                            << "[LIDAR] FALHA: SALIENCIA detectada! (Media: "
                            << media << "m)\n";
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(sensor.mtx_camera);

                    sensor.e_inspecao = true;
                    sensor.o_liga_camera = true;
                }

                sensor.cv_camera.notify_one();

                alarme_ativo = true;
            }
        }
        else {

            // Se antes havia falha e agora voltou ao normal,
            // então o robô já passou pela região defeituosa.
            if (alarme_ativo) {

                {
                    std::lock_guard<std::mutex> lock_tela(mtx_console);
                    std::cout
                        << "[LIDAR] Regiao inspecionada concluida. "
                        << "Alarme desligado.\n";
                }

                {
                    std::lock_guard<std::mutex> lock(sensor.mtx_camera);

                    sensor.e_inspecao = false;
                    sensor.o_liga_camera = false;
                }

                alarme_ativo = false;
            }
        }

        timer_lidar.expires_at(
            timer_lidar.expiry() +
            boost::asio::chrono::milliseconds(100)
        );

        timer_lidar.async_wait(loop_lidar);
    });

    timer_lidar.async_wait(loop_lidar);
    io_lidar.run();
}