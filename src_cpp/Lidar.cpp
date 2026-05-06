#include "Lidar.hpp"
#include <numeric>

LidarFilter::LidarFilter(int size) : buffer(size, 0), window_size(size) {}

float LidarFilter::calcular(int nova_leitura) {
    buffer[head] = nova_leitura;
    head = (head + 1) % window_size;
    float soma = std::accumulate(buffer.begin(), buffer.end(), 0.0);
    ultima_media = soma / window_size;
    return ultima_media;
}

float LidarFilter::get_ultima_media() {
    return ultima_media;
}