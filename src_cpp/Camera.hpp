#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <iostream>
#include <cmath>
#include "shared_buffers.hpp"

// Classe que simula o algoritmo da Câmera
class Camera {
public:
    Camera() = default;

    void processarImagemPesada() {

    }
};

// Protótipo da função que vai rodar na thread
void t_inspecao_camera(SensorBuffer& sensor);

#endif