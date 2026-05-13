#include "Camera.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

void processamento_pesado_ia() {
    // Simula processamento de imagem realizando cálculos matemáticos intensos
    // para garantir uso real de CPU conforme pedido no trabalho.
    double resultado = 0.0;
    for (int i = 0; i < 1000000; ++i) {
        resultado += std::sin(i) * std::tan(i);
    }
    // Apenas para o compilador não otimizar e remover o loop
    if(resultado == 123.45) std::cout << "Algo impossível ocorreu"; 
}

void t_inspecao_camera(SensorBuffer& sensor) {
    while (true) {
        std::unique_lock<std::mutex> lock(sensor.mtx_camera);

        // Aguarda sinalização de falha (o_liga_camera)
        sensor.cv_camera.wait(lock, [&sensor] { 
            return sensor.o_liga_camera; 
        });

        std::cout << "[CAMERA] Falha detectada! Iniciando processamento de IA...\n";

        // Executa o processamento pesado fora do lock para não travar o mutex
        lock.unlock();
        
        auto start = std::chrono::high_resolution_clock::now();
        processamento_pesado_ia();
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "[CAMERA] Inspecao concluida em " << elapsed.count() << " ms.\n";

        // Reseta o estado para a próxima detecção
        lock.lock();
        sensor.o_liga_camera = false;
        sensor.e_inspecao = false; 
    }
}