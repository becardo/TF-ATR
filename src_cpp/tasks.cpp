#include <iostream>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>

#include "tasks.hpp"
#include "shared_buffers.hpp"
#include "Lidar.hpp"
#include "Odometria.hpp"

/* 
Contém as threads responsáveis por ler a odometria, mapear o teto com LIDAR, 
acionar a câmera em caso de falha e salvar os dados em disco (HD). 
*/

// Odometria: Produtor 
void t_calculo_distancia(SensorBuffer& sensor) {
    Odometria odo; // Instancia da classe Odometria 

    boost::asio::io_context io_odo;
    boost::asio::steady_timer timer_odo(io_odo, boost::asio::chrono::milliseconds(20));

    std::function<void(const boost::system::error_code&)> loop_odo;

    loop_odo = ([&](const boost::system::error_code& erro) { 
        if(erro) return;

        // Simulação: alternando sinal do encoder para testar a contagem
        static bool i_encoder = false;
        i_encoder = !i_encoder; 

        int dist_x = odo.atualizar(i_encoder);

        Medicao m;
        m.posicao_x = dist_x;
        m.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        // ----- Zona Crítica: Guardar a leitura na fila -----
        sensor.mtx_fila.lock();
        sensor.fila_medicoes.push(m);
        sensor.mtx_fila.unlock();

        timer_odo.expires_at(timer_odo.expiry() + boost::asio::chrono::milliseconds(20));
        timer_odo.async_wait(loop_odo);
    });

    timer_odo.async_wait(loop_odo);
    io_odo.run();
}

// LIDAR / Reconstrução do teto: Produtor
void t_reconstrucao_teto(SensorBuffer& sensor) {
    LidarFilter filtro(5); // Instancia da classe LIDAR

    int valor_simulado = 40; // Futuro lidar.ler_distancia()
    for(int i = 0; i < 5; i++) {
        filtro.calcular(valor_simulado);
    }

    float altura_ideal = 30.0; // Ajustar conforme necessário
    float margem_erro = 1.0; // Variações toleráveis na altura: tentativa de aproximar mais o problema da realidade.

    boost::asio::io_context io_lidar;
    boost::asio::steady_timer timer_lidar(io_lidar, boost::asio::chrono::milliseconds(100));

    std::function<void(const boost::system::error_code&)> loop_lidar;

    loop_lidar = ([&](const boost::system::error_code& erro){ 
        if(erro) return;

        float media = filtro.calcular(valor_simulado);

        bool falha_detectada = false;

        // Se for MAIOR que o teto+margem: Buraco detectado
        if (media > (altura_ideal + margem_erro)){
            std::cout << "[LIDAR] FALHA 1: BURACO...(dist: " << media << ")\n";
            falha_detectada = true;
        }
        // Se for MENOR que teto-margem: Saliência detectada
        else if(media < (altura_ideal - margem_erro)){
            std::cout << "[LIDAR] FALHA 2: SALIÊNCIA...(dist: " << media << ")\n";
            falha_detectada = true;
        }

        // Se achou uma falha, aciona o Alarme da Câmera
        if (falha_detectada) {
            std::unique_lock<std::mutex> lock_camera(sensor.mtx_camera);
            sensor.e_inspecao = true;      
            sensor.o_liga_camera = true;  // Manda o comando pra ligar a camera
            sensor.mtx_camera.unlock();
            
            // Toca o alarme para acordar a thread da Câmera
            sensor.cv_camera.notify_one(); 
        }

        timer_lidar.expires_at(timer_lidar.expiry() + boost::asio::chrono::milliseconds(100));
        timer_lidar.async_wait(loop_lidar);
    });

    timer_lidar.async_wait(loop_lidar);
    io_lidar.run();
}

// Câmera
void t_inspecao_camera(SensorBuffer& sensor) {
    while (true) { 
        // O uso de 'unique_lock' permite que a thread durma.
        std::unique_lock<std::mutex> lock_camera(sensor.mtx_camera);
        
        // A Câmera dorme. Solução com Predicado.
        sensor.cv_camera.wait(lock_camera, [&sensor] { 
            return sensor.e_inspecao; 
        });

        // Reseta o gatilho para a câmera voltar a dormir
        sensor.e_inspecao = false;
        sensor.o_liga_camera = false;

        // Libera o look antes de realizar as outras tarefas
        lock_camera.unlock();

        /* Inserir lógica "tirar foto" aqui*/

        std::cout << "[CAMERA] Falha detectada! Tirando foto...\n";
    }
}

// Coletor de Dados: Consumidor 
void t_coletor_dados(SensorBuffer& sensor) {
    /*Instanciar a classe do coletor de dados aqui*/

    while (true) { 

        std::unique_lock<std::mutex> lock_fila(sensor.mtx_fila);

        // Acorda sempre que precisar. O predicado protege contra o despertar "fora de hora".
        sensor.cv_coletor.wait(lock_fila, [&sensor] { 
            return !sensor.fila_medicoes.empty(); 
        });
        
        // Esvazia a fila sempre que acorda:
        while (!sensor.fila_medicoes.empty()) {
            // Pega o primeiro da fila e apaga
            Medicao m = sensor.fila_medicoes.front();
            sensor.fila_medicoes.pop();

            // Destranca para Odometria calcular distância
            lock_fila.unlock(); 
            /* Inserir lógica de salvar 'm' no arquivo TVT/CSV */
            std::cout << "[COLETOR] Salvando medição...\n";
            lock_fila.lock(); // Tranca novamente para a próxima rodada pegar o próximo dado com segurança.

        } 

    }
}
