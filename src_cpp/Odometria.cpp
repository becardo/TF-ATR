#include "Odometria.hpp"

#include <iostream>

// Bloco: Cálculo da distância percorrida

// ----- Contrutor da Classe Odometria -----
// distancia_total_x(0): robô é ligado, roda está parada -> distância inicial = 0.
// estado_anterior(false): sensor começou desligado.
Odometria::Odometria() : estado_anterior(false), distancia_total_x(0) {}

// Borda de Subida
int Odometria::atualizar(bool sinal_encoder) {

    // Detecta borda de subida
    // garante contagem única apenas na exata transição de 0 para 1.
    if (sinal_encoder && !estado_anterior) {
        distancia_total_x++; // Soma 1 metro
        std::cout << "[ODOMETRIA]: Distancia percorrida: " << distancia_total_x << "\n";
    }

    // Atualiza memória
    estado_anterior = sinal_encoder;
    
    return distancia_total_x;
}

int Odometria::get_distancia() {
    return distancia_total_x;
}