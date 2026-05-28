#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <chrono>
#include <mutex>
#include <thread>
#include "MQTTClient.h"

#include "shared_buffers.hpp" 

#define ENDERECO    "tcp://localhost:1883"
#define CLIENTID    "Backend_CPP_Oficial"

extern SensorBuffer sensor; 
extern NavBuffer nav;

volatile bool python_reconhecido = false;

// ====================================================================
// CALLBACK: Processamento de dados recebidos da rede MQTT
// ====================================================================
int ao_receber_mensagem(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    std::string topico(topicName);
    std::string payload((char*)message->payload, message->payloadlen);

    // 1. CHECAGEM DE STATUS E SINCRONISMO (HANDSHAKE E FIM DE CURSO)
    if (topico == "tunel/sistema/status") {
        if (payload == "PYTHON_READY") {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            sensor.python_conectado = true;
            python_reconhecido = true;
            std::cout << "[SINCRONISMO] Sinal 'PYTHON_READY' capturado!\n";
        }
        else if (payload == "SIMULACAO_CONCLUIDA") {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            sensor.simulo_concluida = true;
            std::cout << "[FIM DE CURSO] Sinal 'SIMULACAO_CONCLUIDA' recebido do Python!\n";
        }
    }
    // 2. RECEPÇÃO DE COMANDOS VINDOS DA INTERFACE GRÁFICA (GUI)
    else if (topico == "tunel/controle/modo") {
        std::lock_guard<std::mutex> lock(nav.mtx);
        nav.e_automatico = (payload == "AUTO");
        std::cout << "[GUI-IN] Alteração de Modo: " << payload << "\n";
    }
    else if (topico == "tunel/controle/sp_velocidade") {
        std::lock_guard<std::mutex> lock(nav.mtx);
        nav.j_sp_velocidade = std::stof(payload);
        std::cout << "[GUI-IN] Novo Setpoint de Velocidade: " << payload << " m/s\n";
    }
    // 3. RECEPÇÃO DE DADOS DINÂMICOS DA FÍSICA
    else if (topico == "tunel/sensor/encoder") {
        int leitura_encoder = std::stoi(payload);
        
        {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            sensor.ultimo_encoder_recebido = leitura_encoder;
        }
        
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_fila);
            Medicao nova_medicao;
            nova_medicao.i_encoder = leitura_encoder;
            nova_medicao.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            nova_medicao.i_lidar = sensor.ultima_leitura_lidar;
            nova_medicao.nivel_confianca = 100;
            sensor.fila_medicoes.push(nova_medicao);
        }
        sensor.cv_coletor.notify_one(); 
    } 
    else if (topico == "tunel/sensor/lidar") {
        float leitura_lidar = std::stof(payload);
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            sensor.ultima_leitura_lidar = leitura_lidar;
        }
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// ====================================================================
// THREAD PRINCIPAL DE COMUNICAÇÃO MQTT
// ====================================================================
void t_comunicacao_mqtt(){
    
    MQTTClient client;
    MQTTClient_create(&client, ENDERECO, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, ao_receber_mensagem, NULL);

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        std::cout << "[ERRO] Falha ao conectar C++ no Mosquitto!\n";
        return; 
    }
    
    std::cout << "[SISTEMA] Thread MQTT Conectada. Inicializando Handshake com a Interface...\n";

    // Inscrição nos tópicos de Sensores, Sincronismo e Controles da GUI
    MQTTClient_subscribe(client, "tunel/sistema/status", 1);
    MQTTClient_subscribe(client, "tunel/sensor/encoder", 1);
    MQTTClient_subscribe(client, "tunel/sensor/lidar", 1);
    MQTTClient_subscribe(client, "tunel/controle/modo", 1);
    MQTTClient_subscribe(client, "tunel/controle/sp_velocidade", 1);
    MQTTClient_subscribe(client, "tunel/controle/direcao", 1);

    // ---- BLOCO DE HANDSHAKE ----
    while (!python_reconhecido) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    MQTTClient_message msg_handshake = MQTTClient_message_initializer;
    std::string payload_ready = "CPP_READY";
    msg_handshake.payload = (void*)payload_ready.c_str();
    msg_handshake.payloadlen = payload_ready.length();
    msg_handshake.qos = 1; msg_handshake.retained = 1;
    MQTTClient_publishMessage(client, "tunel/sistema/status", &msg_handshake, NULL);
    std::cout << "[OK] Handshake Concluído! Enviado 'CPP_READY'.\n";

    boost::asio::io_context io;
    boost::asio::steady_timer temporizador(io);
    
    auto proximo_ciclo = std::chrono::steady_clock::now();

    while(true) {
        bool encerrar_sistema = false;
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            encerrar_sistema = sensor.simulo_concluida;
        }

        if (encerrar_sistema) {
            std::cout << "[SISTEMA-MQTT] Encerrando publicações cíclicas.\n";
            break; 
        }

        proximo_ciclo += std::chrono::milliseconds(50);

        double controle_pid_aceleracao;
        int controle_status_alarme;
        double velocidade_atual_calculada;

        // Captura segura das variáveis de controle e odometria
        {
            std::lock_guard<std::mutex> lock_nav(nav.mtx);
            controle_pid_aceleracao = nav.o_aceleracao;
        }
        {
            std::lock_guard<std::mutex> lock_cam(sensor.mtx_camera);
            controle_status_alarme = sensor.e_inspecao ? 1 : 0;
        }
        {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            velocidade_atual_calculada = sensor.velocidade_real_medida;
        }

        // --- PUBLICAÇÕES PARA A REDE ---
        // Publica a aceleração para o simulador do Python rodar a física
        std::string payload_accel = std::to_string(controle_pid_aceleracao);
        MQTTClient_message msg1 = MQTTClient_message_initializer;
        msg1.payload = (void*)payload_accel.c_str();
        msg1.payloadlen = payload_accel.length();
        msg1.qos = 1; msg1.retained = 0;
        MQTTClient_publishMessage(client, "tunel/cmd/aceleracao", &msg1, NULL);

        // Publica o status de inspeção para a GUI atualizar o painel de alarmes
        std::string payload_insp = std::to_string(controle_status_alarme);
        MQTTClient_message msg2 = MQTTClient_message_initializer;
        msg2.payload = (void*)payload_insp.c_str();
        msg2.payloadlen = payload_insp.length();
        msg2.qos = 1; msg2.retained = 0;
        MQTTClient_publishMessage(client, "tunel/status/inspecao", &msg2, NULL);

        // CORREÇÃO: Publica a velocidade derivada pela odometria C++ para a GUI sair do zero
        std::string payload_vel = std::to_string(velocidade_atual_calculada);
        MQTTClient_message msg3 = MQTTClient_message_initializer;
        msg3.payload = (void*)payload_vel.c_str();
        msg3.payloadlen = payload_vel.length();
        msg3.qos = 1; msg3.retained = 0;
        MQTTClient_publishMessage(client, "tunel/sensor/velocidade", &msg3, NULL);

        temporizador.expires_at(proximo_ciclo);
        temporizador.wait();
    }

    // Liberação e desconexão limpa de portas
    MQTTClient_unsubscribe(client, "tunel/sensor/encoder");
    MQTTClient_unsubscribe(client, "tunel/sensor/lidar");
    MQTTClient_unsubscribe(client, "tunel/controle/modo");
    MQTTClient_unsubscribe(client, "tunel/controle/sp_velocidade");
    MQTTClient_unsubscribe(client, "tunel/controle/direcao");
    MQTTClient_unsubscribe(client, "tunel/sistema/status");
    
    MQTTClient_disconnect(client, 5000);
    MQTTClient_destroy(&client);
}