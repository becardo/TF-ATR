#ifndef TASKS_HPP
#define TASKS_HPP

#include "shared_buffers.hpp"

// Aqui estão as declarações das threads de sensores e processamento
void t_calculo_distancia(SensorBuffer& sensor);
void t_reconstrucao_teto(SensorBuffer& sensor);
void t_inspecao_camera(SensorBuffer& sensor);
void t_coletor_dados(SensorBuffer& sensor);

#endif