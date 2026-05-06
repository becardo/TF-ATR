#ifndef ODOMETRIA_HPP
#define ODOMETRIA_HPP

class Odometria {
private:
    bool estado_anterior = false;
    int distancia_total_x = 0;

public:
    Odometria();
    int atualizar(bool sinal_encoder);
    int get_distancia();
};

#endif