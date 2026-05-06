#include "Odometria.hpp"

Odometria::Odometria() : estado_anterior(false), distancia_total_x(0) {}

int Odometria::atualizar(bool sinal_encoder) {
    // Detecta borda de subida: sinal atual é true (1) e anterior era false (0)
    if (sinal_encoder && !estado_anterior) {
        distancia_total_x++;
    }
    estado_anterior = sinal_encoder;
    return distancia_total_x;
}

int Odometria::get_distancia() {
    return distancia_total_x;
}