#include "shared_buffers.hpp"
#include <iostream>
#include <thread>
#include <chrono>

/* 
Contém as threads responsáveis por ler a odometria, mapear o teto com LIDAR, 
acionar a câmera em caso de falha e salvar os dados em disco (HD). 
*/

// Odometria: Produtor 
void t_calculo_distancia(SensorBuffer& sensor) {
    /* Instanciar a classe da Odometria aqui*/

    while (true) { 
        /*Incluir chamada da função da odometria aqui*/

        Medicao nova_medicao;
        nova_medicao.posicao_x = 10; // Exemplo

        // ----- Zona Crítica: Guardar a leitura na fila -----
        sensor.mtx_fila.lock();
        sensor.fila_medicoes.push(nova_medicao);
        sensor.mtx_fila.unlock();

        // Ciclo da odometria (20ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); 
    }
}

// LIDAR / Reconstrução do teto: Produtor
void t_reconstrucao_teto(SensorBuffer& sensor) {
    /*Instanciar a classe do LIDAR aqui*/

    while (true) { 
        /*Incluir chamada da função do LIDAR aqui*/


        bool achou_buraco = false; // True se o LIDAR detectar uma falha

        // Se achou um buraco, aciona o Alarme da Câmera
        if (achou_buraco) {
            sensor.mtx_camera.lock();
            sensor.falha_detectada = true;
            sensor.mtx_camera.unlock();
            
            // Toca o alarme para acordar a thread da Câmera
            sensor.cv_camera.notify_one(); 
        }

        // Ciclo do LIDAR (100ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    }
}

// Câmera
void t_inspecao_camera(SensorBuffer& sensor) {
    while (true) { 
        // O uso de 'unique_lock' permite que a thread durma.
        std::unique_lock<std::mutex> lock_camera(sensor.mtx_camera);
        
        // A Câmera dorme. Só acorda quando o LIDAR der o 'notify_one()' E a falha_detectada for true.
        sensor.cv_camera.wait(lock_camera, [&sensor] { return sensor.falha_detectada; });

        /* Inserir lógica "tirar foto" aqui*/

        std::cout << "[CAMERA] Falha detectada! Tirando foto...\n";

        // Reseta o gatilho para a câmera voltar a dormir
        sensor.falha_detectada = false;

    }
}

// Coletor de Dados: Consumidor 
void t_coletor_dados(SensorBuffer& sensor) {
    /*Instanciar a classe do coletor de dados aqui*/

    while (true) { 
        // ----- Zona Crítica: Tira o dado mais antigo da fila -----
        sensor.mtx_fila.lock();
        
        if (!sensor.fila_medicoes.empty()) {
            // Pega o primeiro da fila e apaga
            Medicao m = sensor.fila_medicoes.front();
            sensor.fila_medicoes.pop();
            sensor.mtx_fila.unlock(); 

            /* Inserir lógica de salvar 'm' no arquivo TVT/CSV */

        } else {
            sensor.mtx_fila.unlock(); // Se a fila estiver vazia, só destranca
        }

        // Ciclo de salvamento no HD (500ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    }
}