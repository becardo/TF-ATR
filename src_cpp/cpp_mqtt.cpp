#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <chrono>
#include <mutex>
#include "MQTTClient.h"

#include "shared_buffers.hpp" 

#define ENDERECO    "tcp://localhost:1883"
#define CLIENTID    "Backend_CPP_Oficial"

// 'extern' avisa ao compilador que essas instancias já foram criadas no main.cpp
extern SensorBuffer sensor; 
extern NavBuffer nav;

// ====================================================================
// CALLBACK: O que o C++ faz quando o Python envia os sensores
// Acionado automaticamente pelo Mosquitto sempre que o Python publica alguma 
// informação nova.
// ====================================================================
int ao_receber_mensagem(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    std::string topico(topicName);
    std::string payload((char*)message->payload, message->payloadlen);

    // Utilização de blocos if-else para descobrir qual sensor está fornecendo o dado, já que cada
    // dado tem um tratamento diferente.

    if (topico == "tunel/sensor/encoder") {
        // Tratamento da Odometria (encoder)
        int leitura_encoder = std::stoi(payload); // std::stoi tranforma o texto recebido em 'int'
        
        {
            std::lock_guard<std::mutex> lock_leituras(sensor.mtx_leituras);
            sensor.ultimo_encoder_recebido = leitura_encoder;
        }
        
        // lock_guard para a Fila
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_fila);
            Medicao nova_medicao;
            nova_medicao.i_encoder = leitura_encoder;
            sensor.fila_medicoes.push(nova_medicao);
        }
        // Acorda a thread do Coletor avisando que tem dado novo na fila
        sensor.cv_coletor.notify_one(); 
        
        std::cout << "[MQTT-IN] Encoder: " << leitura_encoder << " m\n";
    } 
    else if (topico == "tunel/sensor/lidar") {
        // Tratamento Sensor de Distância (Lidar)
        float leitura_lidar = std::stof(payload); // std::stof converte o texto para ponto flutuante
        
        // lock_guard para as Leituras
        {
            std::lock_guard<std::mutex> lock(sensor.mtx_leituras);
            sensor.ultima_leitura_lidar = leitura_lidar;
        }
        
        std::cout << "[MQTT-IN] LIDAR: " << leitura_lidar << " m\n";
    }

    // Libera a memória alocada pela biblioteca do Paho MQTT
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
    
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        // Verificação de Inicialização da Rede, detectar erros de conexão
        std::cout << "[ERRO] Falha ao conectar C++ no Mosquitto!\n";
        return; // É bom colocar um return aqui para ele não tentar assinar tópicos se falhar
    }
    
    std::cout << "[SISTEMA] Thread MQTT Iniciada e escutando a rede...\n";

    // Assina os tópicos dos sensores que vêm do Python
    MQTTClient_subscribe(client, "tunel/sensor/encoder", 1);
    MQTTClient_subscribe(client, "tunel/sensor/lidar", 1);

    boost::asio::io_context io;
    boost::asio::steady_timer temporizador(io);
    
    auto proximo_ciclo = std::chrono::steady_clock::now();

    while(true) {
        proximo_ciclo += std::chrono::milliseconds(50);

        double controle_pid_aceleracao;
        int controle_status_alarme;

        // RAII para a Navegação (PID)
        {
            std::lock_guard<std::mutex> lock_nav(nav.mtx);
            controle_pid_aceleracao = nav.o_aceleracao;
        }
        
        // RAII para os Sensores (Câmera)
        {
            std::lock_guard<std::mutex> lock_cam(sensor.mtx_camera);
            controle_status_alarme = sensor.e_inspecao ? 1 : 0;
        }

        // Publica no MQTT (Fora dos cadeados, para não travar o robô enquanto envia a mensagem)
        std::string payload_accel = std::to_string(controle_pid_aceleracao);
        MQTTClient_message msg1 = MQTTClient_message_initializer;
        msg1.payload = (void*)payload_accel.c_str();
        msg1.payloadlen = payload_accel.length();
        msg1.qos = 1; msg1.retained = 0;
        MQTTClient_publishMessage(client, "tunel/cmd/aceleracao", &msg1, NULL);

        std::string payload_insp = std::to_string(controle_status_alarme);
        MQTTClient_message msg2 = MQTTClient_message_initializer;
        msg2.payload = (void*)payload_insp.c_str();
        msg2.payloadlen = payload_insp.length();
        msg2.qos = 1; msg2.retained = 0;
        MQTTClient_publishMessage(client, "tunel/status/inspecao", &msg2, NULL);

        temporizador.expires_at(proximo_ciclo);
        temporizador.wait();
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}