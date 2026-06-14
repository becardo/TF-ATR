// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MÓDULO DE COMUNICAÇÃO MQTT

// Responsável por:

// 1) Receber dados dos sensores simulados pelo Python
// 2) Receber comandos enviados pela Interface Gráfica (GUI)
// 3) Publicar comandos de controle para o simulador
// 4) Publicar estados e alarmes para a GUI

// Estrutura de comunicação:

//      Python <-> Broker MQTT <-> Backend C++ <-> GUI

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <condition_variable>
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <chrono>
#include <mutex>
#include <thread>
#include "MQTTClient.h"

#include "shared_buffers.hpp" 

#define ENDERECO    "tcp://localhost:1883" // Endereço do broker MQTT (Mosquitto)
#define CLIENTID    "Backend_CPP_Oficial"  // Identificador do cliente MQTT

// Estruturas glogais compartilhadas entre threads do sistema
extern SensorBuffer sensor; 
extern NavBuffer nav;

static double memoria_spinbox = 1.0;

// Variáveis de sincronismo. Backend C++ aguarda até que o simulador Python diga "PYTHON_READY"
std::mutex mtx_handshake;
std::condition_variable cv_handshake;
bool python_reconhecido = false;

// CALLBACK - Processamento de dados recebidos da rede MQTT
// - Receber dados de sensores
// - Receber comandos da GUI
// - Atualizar buffers compartilhados
// - Sinalizar threads consumidoras
// - Realizar sincronismo do sistema
int ao_receber_mensagem(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // Converte os dados recebidos da biblioteca MQTT para objetos std::string
    std::string topico(topicName);
    std::string payload((char*)message->payload, message->payloadlen);

    // Checagem de sincronismo
    if (topico == "tunel/sistema/status") {
        if (payload == "PYTHON_READY") { // Broker conectado
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            sensor.python_conectado = true; // Marca o simulador como disponível

            {
                std::lock_guard<std::mutex> lock_hs(mtx_handshake);
                python_reconhecido = true;
            }
            cv_handshake.notify_one(); // Acorda a thread

            std::cout << "[SINCRONISMO] Sinal 'PYTHON_READY' capturado.\n";
        }
        else if (payload == "SIMULACAO_CONCLUIDA") {
            {
                std::lock_guard<std::mutex> lock_sensor(sensor.mtx_leituras);
                sensor.ultima_leitura_encoder = 0;
                sensor.ultima_leitura_lidar = 0.0;
                sensor.velocidade_real_medida = 0.0;
            }

            {
                std::lock_guard<std::mutex> lock_nav(nav.mtx);
                nav.c_para = true;
                nav.velocidade_joystick = 0.0;
                nav.e_automatico = false;
                nav.sistema_iniciado = false;
            }
            std::cout << "[FIM DE CURSO] Sinal 'SIMULACAO_CONCLUIDA' recebido do Python.\n";
        }
    }
    // Seleção de modo Manual/Automático, vindo da GUI
    else if (topico == "tunel/controle/modo") {
        std::lock_guard<std::mutex> lock(nav.mtx);
        if (payload == "AUTO") {
            nav.c_automatico = true;
            nav.c_man = false;
        } else {
            nav.c_automatico = false;
            nav.c_man = true;
        }
    }
    // Dados físicos da posição do robô, recebe leituras do encoder
    else if (topico == "tunel/sensor/encoder") {
        try {
            // Tolerância a falhas de rede: Evita a quebra do processo C++ ao tentar converter payloads malformados.
            // Se a conversão falhar, a exceção é capturada, o pacote sujo é descartado, e o robô mantém sua estabilidade.
            int leitura_encoder = std::stoi(payload);
            
            {
                std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
                sensor.ultima_leitura_encoder = leitura_encoder; // Atualização da última leitura
            }
            
            { // criação da medição
                std::lock_guard<std::mutex> lock(sensor.mtx_fila);

                Medicao nova_medicao;  // Estrutura de um instante completo de observação
                                    // - posição encoder
                                    // - leitura lidar
                                    // - timestamp
                                    // - nível de confiança

                nova_medicao.i_encoder = leitura_encoder; // Valor absoluto do encoder
                nova_medicao.timestamp = std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count(); // Momento em que a medição foi recebida
                nova_medicao.i_lidar = sensor.ultima_leitura_lidar; // Associa último valor LIDAR conhecido
                nova_medicao.nivel_confianca = 100;
                sensor.fila_medicoes.push(nova_medicao); // Insere a medição na fila compartilhada
            }

            // Avisa a thread do coletor que tem uma nova medição para ser gravada
            sensor.cv_coletor.notify_one();
        }
        catch (const std::exception& e) {
            std::cerr << "[ERRO MQTT] Lixo de rede descartado no Encoder: '" << payload << "'\n";
        } 
    } 
    // Dados físicos da leitura do teto
    else if (topico == "tunel/sensor/lidar") {
        try {
            // Tolerância a falhas de rede: Evita a quebra do processo C++ ao tentar converter payloads malformados.
            // Se a conversão falhar, a exceção é capturada, o pacote sujo é descartado, e o robô mantém sua estabilidade.
            float leitura_lidar = std::stof(payload);
            {
                std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
                sensor.ultima_leitura_lidar = leitura_lidar;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[ERRO MQTT] Lixo de rede descartado no LIDAR: '" << payload << "\n";
        }
    }
    // Comandos botões: modo Manual (DIREITA/ESQUERDA) e geral (PARAR/CONTINUAR)
    else if (topico == "tunel/controle/direcao") {
        std::lock_guard<std::mutex> lock(nav.mtx);
        if (payload == "DIREITA") {
            nav.c_para = false;
            nav.velocidade_joystick = memoria_spinbox; // Garante ir pra frente
        } 
        else if (payload == "ESQUERDA") {
            nav.c_para = false;
            nav.velocidade_joystick = -memoria_spinbox; // Dá ré
        } 
        else if (payload == "PARAR") {
            nav.c_para = true; 
            std::cout << "[MQTT-IN] EMERGÊNCIA acionada.\n";
        }
        else if (payload == "CONTINUAR") {
            nav.c_para = false; 
            std::cout << "[MQTT-IN] RETOMANDO: Emergência desativada.\n";
        }
        else if (payload == "SIMULACAO_CONCLUIDA") {
            std::lock_guard<std::mutex> lock(nav.mtx);
            nav.c_para = true;
            nav.velocidade_joystick = 0.0;
            nav.e_automatico = false;

            sensor.ultima_leitura_lidar = 0.0;
            sensor.ultima_leitura_encoder = 0;
            sensor.velocidade_real_medida = 0.0;

            std::cout << "[MQTT-IN] STATUS: SIMULAÇÃO CONCLUÍDA!\n";
        }
    }
    // Iniciar a Inspeção
    else if (topico == "tunel/cmd/iniciar") { // Botão iniciar
        std::lock_guard<std::mutex> lock_nav(nav.mtx);
        if (payload == "1") {
            nav.sistema_iniciado = true;
            nav.c_para = false;
            std::cout << "[MQTT-IN] Comando INICIAR recebido!\n";
        }
    }
    // Velocidade medida
    else if (topico == "tunel/sensor/velocidade") {
        try {
            // Tolerância a falhas de rede: Evita a quebra do processo C++ ao tentar converter payloads malformados.
            // Se a conversão falhar, a exceção é capturada, o pacote sujo é descartado, e o robô mantém sua estabilidade.
            double vel = std::stod(payload);

            // Recebe a velocidade calculada pelo simulador físico
            // Utilizada para realimentação do controlador e exibição na interface
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            sensor.velocidade_real_medida = vel;
        }
        catch (const std::exception& e) {
            std::cerr << "[ERRO MQTT] Lixo de rede descartado na Velocidade: '" << payload << "\n";
        }
    }
    // Captura a ordem de mudança de modo vinda do botão "INICIAR"
    else if (topico == "tunel/controle/modo" || topico == "tunel/cmd/modo") {
        std::lock_guard<std::mutex> lock(nav.mtx);
        if (payload == "AUTO" || payload == "1") {
            nav.e_automatico = true;
            std::cout << "[MQTT-IN] Ordem recebida: Modo AUTOMATICO ativado.\n";
        } else {
            nav.e_automatico = false;
            std::cout << "[MQTT-IN] Ordem recebida: Modo MANUAL ativado.\n";
        }
    }

    // Liberação de recursos alocados
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// Thread de comunicação MQTT:
// - conectar ao broker;
// - inscrever-se nos tópicos;
// - realizar handshake;
// - publicar aceleração, alarme e velocidade;
// - receber mensagens
void t_comunicacao_mqtt(){
    
    MQTTClient client;

    // Criação da instancia local do cliente MQTT associada ao broker configurado
    MQTTClient_create(&client, 
        ENDERECO, 
        CLIENTID, 
        MQTTCLIENT_PERSISTENCE_NONE, 
        NULL
    );

    // Registra a função que será chamada automaticamente
    // sempre que uma mensagem MQTT chegar
    MQTTClient_setCallbacks(client, 
        NULL, 
        NULL, 
        ao_receber_mensagem, 
        NULL
    );

    // Estrutura para os parâmetros de conexão
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    conn_opts.keepAliveInterval = 60; // Mensagens periódicas a cada 60s
    conn_opts.cleansession = 1; // Solicita uma sessão limpa
    
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        std::cout << "[ERRO] Falha ao conectar C++ no Mosquitto!\n";
        return; 
    }
    
    std::cout << "[SISTEMA] Thread MQTT Conectada. Inicializando Handshake com a Interface...\n";

    // Inscrição nos tópicos de Sensores, Sincronismo e Controles da GUI
    // QoS nível 1: "Pelo menos uma vez". A mensagem tem garantia absoluta de que vai chegar.
    MQTTClient_subscribe(client, "tunel/sensor/camera", 1);
    MQTTClient_subscribe(client, "tunel/sistema/status", 1);
    MQTTClient_subscribe(client, "tunel/sensor/encoder", 1);
    MQTTClient_subscribe(client, "tunel/sensor/lidar", 1);
    MQTTClient_subscribe(client, "tunel/controle/modo", 1);
    MQTTClient_subscribe(client, "tunel/controle/direcao", 1);
    MQTTClient_subscribe(client, "tunel/cmd/iniciar", 1);
    MQTTClient_subscribe(client, "tunel/sensor/velocidade", 1);
    MQTTClient_subscribe(client, "tunel/cmd/modo", 1);
    MQTTClient_subscribe(client, "tunel/controle/modo", 1);

    {
        std::unique_lock<std::mutex> lock_hs(mtx_handshake);
        cv_handshake.wait(lock_hs, [] { return python_reconhecido; });
    }

    MQTTClient_message msg_handshake = MQTTClient_message_initializer;

    std::string payload_ready = "CPP_READY"; // Resposta do Backend após reconhecer o Python
    msg_handshake.payload = (void*)payload_ready.c_str();
    msg_handshake.payloadlen = payload_ready.length();
    msg_handshake.qos = 1; 
    msg_handshake.retained = 1;
    MQTTClient_publishMessage(client, "tunel/sistema/status", &msg_handshake, NULL);
    std::cout << "[OK] Handshake Concluído! Enviado 'CPP_READY'.\n";

    boost::asio::io_context io;
    boost::asio::steady_timer temporizador(io);
    
    auto proximo_ciclo = std::chrono::steady_clock::now();

    // Variável para detectar a "borda de subida" do alarme
    int estado_anterior_alarme = 0;

    while(true) {
        proximo_ciclo += std::chrono::milliseconds(50);

        double controle_pid_aceleracao;
        int controle_status_alarme;
        double velocidade_atual_calculada;
        bool estado_atual_alarme;

        // Captura das variáveis de controle e odometria
        {
            std::lock_guard<std::mutex> lock_nav(nav.mtx);
            controle_pid_aceleracao = nav.o_aceleracao;
        }

        {
            std::lock_guard<std::mutex> lock_cam(sensor.mtx_camera);
            // Converte o estado booleano da inspeção
            // para um valor inteiro adequado para
            // transmissão MQTT
            controle_status_alarme = sensor.e_inspecao;
        }

        {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            velocidade_atual_calculada = sensor.velocidade_real_medida;
        }

        {
            std::lock_guard<std::mutex> lock_cam(sensor.mtx_camera);
            estado_atual_alarme = sensor.e_inspecao;
        }

        // Publicações

        // Publica a ação da câmera
        if (estado_atual_alarme != 0 && estado_anterior_alarme == 0) {
            std::string payload_cam = "CLICK";
            MQTTClient_message msg_camera = MQTTClient_message_initializer;
            msg_camera.payload = (void*)payload_cam.c_str();
            msg_camera.payloadlen = payload_cam.length();
            msg_camera.qos = 1; 
            msg_camera.retained = 0;
            
            MQTTClient_publishMessage(
                client, 
                "tunel/sensor/camera", 
                &msg_camera, 
                NULL
            );
            
            // Log impossível de ignorar no terminal C++
            std::cout << "[MQTT-OUT] Pulso de FLASH enviado para o Python.\n";
        }
        // Atualiza a memória para o próximo ciclo
        estado_anterior_alarme = estado_atual_alarme;

        // Publica a aceleração para o simulador do Python rodar a física
        std::string payload_accel = std::to_string(controle_pid_aceleracao);
        MQTTClient_message msg_aceleracao = MQTTClient_message_initializer;
        msg_aceleracao.payload = (void*)payload_accel.c_str();
        msg_aceleracao.payloadlen = payload_accel.length();
        msg_aceleracao.qos = 1; 
        msg_aceleracao.retained = 0;
        MQTTClient_publishMessage(
            client, 
            "tunel/cmd/aceleracao", 
            &msg_aceleracao, 
            NULL
        );

        // Publica o alarme de inspeção para a GUI atualizar o painel de alarmes
        std::string payload_insp = std::to_string(controle_status_alarme);
        MQTTClient_message msg_alarme = MQTTClient_message_initializer;
        msg_alarme.payload = (void*)payload_insp.c_str();
        msg_alarme.payloadlen = payload_insp.length();
        msg_alarme.qos = 1; 
        msg_alarme.retained = 0;
        MQTTClient_publishMessage(
            client, 
            "tunel/status/inspecao", 
            &msg_alarme, 
            NULL
        );

        // Publica a velocidade derivada pela odometria C++ para a GUI sair do zero
        std::string payload_vel = std::to_string(velocidade_atual_calculada);
        MQTTClient_message msg_velocidade = MQTTClient_message_initializer;
        msg_velocidade.payload = (void*)payload_vel.c_str();
        msg_velocidade.payloadlen = payload_vel.length();
        msg_velocidade.qos = 1; 
        msg_velocidade.retained = 0;
        MQTTClient_publishMessage(
            client, 
            "tunel/sensor/velocidade", 
            &msg_velocidade, 
            NULL
        );

        temporizador.expires_at(proximo_ciclo);
        temporizador.wait();
    }

    // Liberação e desconexão limpa de portas
    MQTTClient_unsubscribe(client, "tunel/sensor/camera");
    MQTTClient_unsubscribe(client, "tunel/sensor/encoder");
    MQTTClient_unsubscribe(client, "tunel/sensor/lidar");
    MQTTClient_unsubscribe(client, "tunel/controle/modo");
    MQTTClient_unsubscribe(client, "tunel/controle/direcao");
    MQTTClient_unsubscribe(client, "tunel/sistema/status");
    MQTTClient_unsubscribe(client, "tunel/cmd/iniciar");
    MQTTClient_unsubscribe(client, "tunel/cmd/modo");
    MQTTClient_unsubscribe(client, "tunel/controle/modo");
    
    MQTTClient_disconnect(client, 5000);
    MQTTClient_destroy(&client);
}