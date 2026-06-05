#include "Camera.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

// Função otimizada para garantir carga real de CPU
void processamento_pesado_ia() {
    double resultado = 0.0;
    // Número de iterações para chegar perto dos 30-40ms 
    for (int i = 0; i < 700000; ++i) {
        resultado += std::sin(i) * std::cos(i) * std::tan(i);
    }
    
    // Impede que o compilador ignore o loop (Dead Code Elimination)
    if(resultado == 0.00000123) std::cout << resultado; 
}

void t_inspecao_camera(SensorBuffer& sensor) {
    while (true) {
        // Bloqueio inicial para esperar o sinal
        std::unique_lock<std::mutex> lock(sensor.mtx_camera);

        // Aguarda sinalização do LIDAR (o_liga_camera)
        sensor.cv_camera.wait(lock, [&sensor] { 
            return sensor.o_liga_camera; 
        });

        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[CAMERA] Falha detectada! Iniciando processamento de IA...\n";
        }

        // Libera o LOCK durante o processamento pesado
        lock.unlock();
        
        auto start = std::chrono::high_resolution_clock::now();
        processamento_pesado_ia();
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        lock.lock();
        
        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[CAMERA] Inspecao concluida em " << elapsed.count() << " ms.\n";
        }

        sensor.o_liga_camera = false;        
        // O lock é liberado automaticamente ao voltar para o wait ou fim do loop
    }
}