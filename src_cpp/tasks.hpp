#ifndef TASKS_HPP
#define TASKS_HPP

#include "shared_buffers.hpp"


void t_comando_navegacao(NavBuffer& nav, SensorBuffer& sensor);
void t_controle_navegacao(NavBuffer& nav, SensorBuffer& sensor);

// --- Threads do Bloco de Sensores ---
void t_calculo_distancia(NavBuffer& nav, SensorBuffer& sensor);
void t_reconstrucao_teto(SensorBuffer& sensor);

#endif