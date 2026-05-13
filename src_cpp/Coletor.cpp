#include "Coletor.hpp"
#include <mutex>
#include <condition_variable>
#include <cmath> // Necessário para pow() e sqrt()

// Thread responsável por consumir os dados da fila, analisar a confiança e salvar no disco
void t_coletor_dados(SensorBuffer& sensor) {
    
    ColetorCSV logger("telemetria_inspecao.csv");

    if (!logger.estaAberto()) {
        return;
    }

    // Variáveis para guardar a última posição e calcular a distância
    double ultimo_x = 0.0;
    double ultimo_y = 0.0;

    while (true) { 
        std::unique_lock<std::mutex> lock_fila(sensor.mtx_fila);

        sensor.cv_coletor.wait(lock_fila, [&sensor] { 
            return !sensor.fila_medicoes.empty(); 
        });
        
        while (!sensor.fila_medicoes.empty()) {
            Medicao dado = sensor.fila_medicoes.front();
            sensor.fila_medicoes.pop();

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
            logger.gravar(dado);
            lock_fila.lock(); 
        } 
    }
}