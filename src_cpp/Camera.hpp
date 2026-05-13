#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "shared_buffers.hpp"

// Thread que simula a inspeção visual detalhada
void t_inspecao_camera(SensorBuffer& sensor);

// Função para simular carga de CPU (IA)
void processamento_pesado_ia();

#endif