#ifndef SHARED_BUFFERS_HPP
#define SHARED_BUFFERS_HPP

#include <mutex>
#include <condition_variable>
#include <queue>

/* Define os Buffers, Mutexes, Filas e Variáveis de Condição que permitem
a comunicação segura entre as múltiplas threads do sistema. Garante a
exclusão mútua nas zonas críticas, evitando condições de corrida.
*/

inline std::mutex mtx_console;

// Buffer de Comando e Navegação
// Armazena os dados de troca entre os comandos do usuário e o controle PID
// do robo. Protegida por MUTEX para evitar Condição de Corrida.
struct NavBuffer {
    double j_sp_velocidade = 0.0;       // Velocidade alvo solicitada (m/s)
    double o_aceleracao = 0.0;          // Esforço do controle calculado pelo PID
    bool e_automatico = false;          // Estado de operação (Manual ou Auto)

    bool sistema_iniciado = false;      // O robô inicia parado
    bool c_automatico = false;          // Comando de modo Automático 
    bool c_man = false;                 // Comando de modo Manual 
    bool c_para = false;                // Comando de parada de emergência
    double velocidade_joystick = 0.0;   // Velocidade enviada pelos botões esq/dir

    bool inspecao_concluida = false;    // Flag de Inspeção Concluída

    std::mutex mtx;                     // O cadeado do PID
};

// Armazena uma leitura única de sensor. Empacota os dados antes de enviar para 
// a fila (queue).
struct Medicao {
    long long timestamp = 0;            // Tempo da leitura (ms)
    int i_encoder = 0;                  // Odometria: Posição atual no eixo X
    float i_lidar = 0.0;                // LIDAR: Distância até o teto
    double nivel_confianca = 0;         // Qualidade da medição (0 a 100)
};

// Buffer dos Sensores e Câmera
// Armazena os dados sensoriais e sinalização de eventos críticos 
// A thread de Sensores "Produz" e a thread do Coletor "Consome"
struct SensorBuffer {
    // ----- Fila de Dados (Produtor-Consumidor) -----
    std::queue<Medicao> fila_medicoes; 
    std::mutex mtx_fila;                // Cadeado para a inserção e remição da fila
    std::mutex mtx_leituras;            // Cadeado para a leitura dos sensores

    std::condition_variable cv_coletor; // Variável de condição para o Coletor de Dados.

    // ----- Sistema de Alarme - Interrupção (Câmera) -----
    int e_inspecao = 0;        // Flag que indica se o teto tem falhas: 0 = OK, 1 = Saliência, -1 = Buraco
    std::mutex mtx_camera;              // Cadeado para proteger a alteração da FLAG

    bool o_liga_camera = false;         // Variável de Condição para a Câmera
    std::condition_variable cv_camera;  // O alarme da câmera

    float ultima_leitura_lidar = 10.0f; // Última leitura LIDAR, nasce como altura ideal do teto
    double velocidade_real_medida = 0.0;// Variável compartilhada com o PID e MQTT
    int ultima_leitura_encoder = 0;    // última leitura Encedor, nasce como posição 0

    // ----- Flags de Sincronismo MQTT -----
    bool python_conectado = false;      // Controla o Handshake de largada inicial com a GUI
    bool simulo_concluida = false;      // Sinaliza o fim de curso (Parada geral aos 1000m)
};

#endif 