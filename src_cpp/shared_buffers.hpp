#ifndef SHARED_BUFFERS_HPP
#define SHARED_BUFFERS_HPP

#include <mutex>
#include <condition_variable>
#include <queue>

/* 
Define os Buffers, Mutexes, Filas e Variáveis de Condição que permitem
a comunicação segura entre as múltiplas threads do sistema. Garante a
exclusão mútua nas zonas críticas, evitando condições de corrida.
*/

// Buffer de Comando e Navegação
// Armazena os dados de troca entre os comandos do usuário e o controle PID
// do robo. Protegida por MUTEX para evitar Condição de Corrida.
struct NavBuffer {
    int j_sp_velocidade = 0; // Velocidade alvo solicitada (cm/s)
    int o_aceleracao = 0;          // Esforço do controle calculado pelo PID
    bool e_automatico = false;// Estado de operação (Manual ou Auto)
    std::mutex mtx;              // O cadeado do PID
};

// Armazena uma leitura única de sensor. Empacota os dados antes de enviar para 
// a fila (queue).
struct Medicao {
    long long timestamp = 0; // Tempo da leitura (ms)
    int i_encoder = 0;       // Odometria: Posição atual no eixo X
    int i_lidar = 0;       // LIDAR: Distância até o teto
    double nivel_confianca = 0; // Qualidade da medição (0 a 100)
};

// Buffer dos Sensores e Câmera
// Armazena os dados sensoriais e sinalização de eventos críticos 
struct SensorBuffer {
    // ----- Fila de Dados (Produtor-Consumidor) -----
    // A thread de Sensores "Produz" e a thread do Coletor "Consome"
    std::queue<Medicao> fila_medicoes; 
    std::mutex mtx_fila; // Cadeado para a inserção e remição da fila

    // Variável de condição para o Coletor de Dados.
    std::condition_variable cv_coletor;

    // ----- Sistema de Alarme - Interrupção (Câmera) -----
    bool e_inspecao = false; // FLAG que indica se o teto tem buracos
    std::mutex mtx_camera;        // Cadeado para proteger a alteração da FLAG

    // Variável de Condição para a Câmera 
    bool o_liga_camera = false;
    std::condition_variable cv_camera; // O alarme da câmera

    int ultima_leitura_lidar = 30;
};

#endif