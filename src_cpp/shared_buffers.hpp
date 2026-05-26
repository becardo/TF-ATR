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

inline std::mutex mtx_console;

// Buffer de Comando e Navegação
// Armazena os dados de troca entre os comandos do usuário e o controle PID
// do robo. Protegida por MUTEX para evitar Condição de Corrida.
struct NavBuffer {
    double j_sp_velocidade = 0.0; // Velocidade alvo solicitada (m/s)
    double o_aceleracao = 0.0;          // Esforço do controle calculado pelo PID
    bool e_automatico = false;// Estado de operação (Manual ou Auto)
    std::mutex mtx;              // O cadeado do PID
};

// Armazena uma leitura única de sensor. Empacota os dados antes de enviar para 
// a fila (queue).
struct Medicao {
    long long timestamp = 0; // Tempo da leitura (ms)
    int i_encoder = 0;       // Odometria: Posição atual no eixo X
    float i_lidar = 0.0;       // LIDAR: Distância até o teto
    double nivel_confianca = 0; // Qualidade da medição (0 a 100)
};

// Buffer dos Sensores e Câmera
// Armazena os dados sensoriais e sinalização de eventos críticos 
// A thread de Sensores "Produz" e a thread do Coletor "Consome"
struct SensorBuffer {
    // ----- Fila de Dados (Produtor-Consumidor) -----
    std::queue<Medicao> fila_medicoes; 
    std::mutex mtx_fila; // Cadeado para a inserção e remição da fila
    std::mutex mtx_leituras; 

    std::condition_variable cv_coletor; // Variável de condição para o Coletor de Dados.

    // ----- Sistema de Alarme - Interrupção (Câmera) -----
    bool e_inspecao = false; // FLAG que indica se o teto tem buracos
    std::mutex mtx_camera;        // Cadeado para proteger a alteração da FLAG

    bool o_liga_camera = false; // Variável de Condição para a Câmera 
    std::condition_variable cv_camera; // O alarme da câmera

    float ultima_leitura_lidar = 10.0;
    double velocidade_real_medida = 0.0; // Variável para compartilhar com o PID
    int ultimo_encoder_recebido = 0;
};

#endif