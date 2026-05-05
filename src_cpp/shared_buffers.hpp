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
    int setpoint_velocidade = 0; // Velocidade alvo solicitada (cm/s)
    int aceleracao = 0;          // Esforço do controle calculado pelo PID
    bool modo_automatico = false;// Estado de operação (Manual ou Auto)
    std::mutex mtx;              // O cadeado do PID
};

// Armazena uma leitura única de sensor. Empacota os dados antes de enviar para 
// a fila (queue).
struct Medicao {
    long long timestamp; // Tempo da leitura (ms)
    int posicao_x;       // Odometria: Posição atual no eixo X
    int leitura_y;       // LIDAR: Distância até o teto
    int nivel_confianca; // Qualidade da medição (0 a 100)
};

// Buffer dos Sensores e Câmera
// Armazena os dados sensoriais e sinalização de eventos críticos 
struct SensorBuffer {
    // ----- Fila de Dados (Produtor-Consumidor) -----
    // A thread de Sensores "Produz" e a thread do Coletor "Consome"
    std::queue<Medicao> fila_medicoes; 
    std::mutex mtx_fila; // Cadeado para a inserção e remição da fila

    // ----- Sistema de Alarme - Interrupção (Câmera) -----
    bool falha_detectada = false; // FLAG que indica se o teto tem buracos
    std::mutex mtx_camera;        // Cadeado para proteger a alteração da FLAG

    // Variável de Condição (Alarme): mantém a thread da câmera dormindo 
    // até receber o sinal 'notify_one()' do sensor do teto.
    std::condition_variable cv_camera; // O alarme da câmera
};

#endif