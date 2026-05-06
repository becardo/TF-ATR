#include "shared_buffers.hpp"
#include "Lidar.hpp"
#include "Odometria.hpp"
#include <iostream>
#include <thread>
#include <chrono>

/* 
Contém as threads responsáveis por ler a odometria, mapear o teto com LIDAR, 
acionar a câmera em caso de falha e salvar os dados em disco (HD). 
*/

// Odometria: Produtor 
void t_calculo_distancia(SensorBuffer& sensor) {
    Odometria odo; // Instancia da classe Odometria 

    while (true) { 
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

        // Ciclo da odometria (20ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); 
    }
}

// LIDAR / Reconstrução do teto: Produtor
void t_reconstrucao_teto(SensorBuffer& sensor) {
    LidarFilter filtro(5); // Instancia da classe LIDAR
    float limite_falha = 30.0; // Ajuste conforme necessário

    while (true) { 
        int leitura_bruta = 40; // Simulação de leitura normal
        float media = filtro.calcular(leitura_bruta);

        // Se achou um buraco, aciona o Alarme da Câmera
        if (media > limite_falha) {
            sensor.mtx_camera.lock();
            sensor.e_inspecao = true;      
            sensor.o_liga_camera = true;    // Manda o comando pra ligar a camera
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
        sensor.cv_camera.wait(lock_camera, [&sensor] { return sensor.e_inspecao; });

        /* Inserir lógica "tirar foto" aqui*/

        std::cout << "[CAMERA] Falha detectada! Tirando foto...\n";

        // Reseta o gatilho para a câmera voltar a dormir
        sensor.e_inspecao = false;
        sensor.o_liga_camera = false;

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