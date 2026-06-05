#ifndef LIDAR_HPP
#define LIDAR_HPP

#include <vector>

class LidarFilter {
private:
    std::vector<float> buffer;
    int head = 0;
    int window_size;
    float ultima_media = 0.0;

public:
    // Escolha: size = 5. Com a janela cíclica de 100ms, uma janela de tamanho 5
    // dá um atraso de meio segundo, filtrando picos isolados do sensor.
    LidarFilter(int size = 5);
    float calcular(float nova_leitura);
    float get_ultima_media();
};

#endif