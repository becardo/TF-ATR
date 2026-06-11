#include "Coletor.hpp"
#include <mutex>
#include <condition_variable>
#include <cmath> 
#include <memory>
#include <fstream>
#include <iostream>

#include "shared_buffers.hpp"

// Thread responsável por consumir os dados da fila, analisar a confiança e salvar no disco
void t_coletor_dados(SensorBuffer& sensor, NavBuffer& nav) {
    
    // Transforma o logger em um ponteiro dinâmico para ser possível reinicia-lo
    auto logger = std::make_unique<ColetorCSV>("telemetria_inspecao.csv"); 

    if (!logger->estaAberto()) {
        std::cout << "[ERRO] Não foi possível abrir o arquivo de telemetria.\n";
        return;
    }

    // Variáveis para guardar a última posição e calcular a distância
    double ultimo_x = 0.0;
    double ultimo_y = 0.0;

    // ultimo estado do sistema
    bool sistema_estava_iniciado = false;

    while (true) { 

        // Detecta o início de uma nova inspeção
        {
            std::lock_guard<std::mutex> lock_nav(nav.mtx);
            if (nav.sistema_iniciado && !sistema_estava_iniciado) {

                std::cout << "[COLETOR] Nova inspeção detectada. Reiniciando CSV.\n";

                // Fecha e reseta o arquivo atual
                logger.reset();
                std::ofstream resetador("telemetria_inspecao.csv", std::ios::trunc);
                resetador.close();

                // Recria o arquivo
                logger = std::make_unique<ColetorCSV>("telemetria_inspecao.csv");

                // Reinicia memória de confiança
                ultimo_x = 0.0;
                ultimo_y = 0.0;
            }

            sistema_estava_iniciado = nav.sistema_iniciado;
        }
        std::unique_lock<std::mutex> lock_fila(sensor.mtx_fila);

        sensor.cv_coletor.wait(lock_fila, [&sensor] { 
            return !sensor.fila_medicoes.empty(); 
        });
        
        while (!sensor.fila_medicoes.empty()) {
            Medicao dado = sensor.fila_medicoes.front();
            sensor.fila_medicoes.pop();

            // Inicia nova inspeção: arquivo CSV é renovado

            // --- Análise de Confiança ---
            
            // Calcula a distância entre o ponto atual e o último ponto
            double distancia = std::sqrt(std::pow(dado.i_encoder - ultimo_x, 2) + 
                                         std::pow(dado.i_lidar - ultimo_y, 2));

            // Lógica de Confiança: Se a distância aumenta, a confiança cai.
            // O fator "5.0" é um multiplicador de sensibilidade
            double confianca_calculada = 100.0 - (distancia * 5.0);
            
            // Travas de segurança para não ter confiança negativa ou acima de 100%
            if (confianca_calculada < 0.0) confianca_calculada = 0.0;
            if (confianca_calculada > 100.0) confianca_calculada = 100.0;

            dado.nivel_confianca = confianca_calculada;
            ultimo_x = dado.i_encoder;
            ultimo_y = dado.i_lidar;

            // Destranca para gravar no HD 
            lock_fila.unlock(); 
            logger->gravar(dado);
            lock_fila.lock(); 
        } 
    }
}