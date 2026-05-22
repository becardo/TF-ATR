#include "Camera.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

// Função otimizada para garantir carga real de CPU
void processamento_pesado_ia() {
    double resultado = 0.0;
    // Ajustei o número de iterações para chegar perto dos 30-40ms em CPUs modernas
    // Se ficar rápido demais no seu PC, aumente para 2000000
    for (int i = 0; i < 700000; ++i) {
        resultado += std::sin(i) * std::cos(i) * std::tan(i);
    }
    
    // Impede que o compilador ignore o loop (Dead Code Elimination)
    if(resultado == 0.00000123) std::cout << resultado; 
}

void t_inspecao_camera(SensorBuffer& sensor) {
    while (true) {
        // 1. Bloqueio inicial para esperar o sinal
        std::unique_lock<std::mutex> lock(sensor.mtx_camera);

        // 2. Aguarda sinalização do LIDAR (o_liga_camera)
        // O robô já estará em Slowdown (Setpoint 1) antes da câmera acordar
        sensor.cv_camera.wait(lock, [&sensor] { 
            return sensor.o_liga_camera; 
        });

        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[CAMERA] Falha detectada! Iniciando processamento de IA...\n";
        }

        // 3. LIBERA O LOCK durante o processamento pesado
        // Isso é CRITICAL em ATR para permitir que a thread de comando 
        // continue lendo o buffer se necessário.
        lock.unlock();
        
        auto start = std::chrono::high_resolution_clock::now();
        processamento_pesado_ia();
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        // 4. BLOQUEIA novamente para resetar as flags de estado
        lock.lock();
        
        {
            std::lock_guard<std::mutex> lock_tela(mtx_console);
            std::cout << "[CAMERA] Inspecao concluida em " << elapsed.count() << " ms.\n";
        }

        // RESET DAS FLAGS: Aqui o robô recebe permissão para voltar a 5.0
        sensor.o_liga_camera = false;
        sensor.e_inspecao = false; 
        
        // O lock é liberado automaticamente ao voltar para o wait ou fim do loop
    }
}