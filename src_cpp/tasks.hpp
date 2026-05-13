#ifndef TASKS_HPP
#define TASKS_HPP

#include "shared_buffers.hpp"

// Aqui estão as declarações das threads de sensores
void t_calculo_distancia(SensorBuffer& sensor);
void t_reconstrucao_teto(SensorBuffer& sensor);

#endif