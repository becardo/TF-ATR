#ifndef LIDAR_HPP
#define LIDAR_HPP

#include <vector>

class LidarFilter {
private:
    std::vector<int> buffer;
    int head = 0;
    int window_size;
    float ultima_media = 0.0;

public:
    LidarFilter(int size = 5);
    float calcular(int nova_leitura);
    float get_ultima_media();
};

#endif