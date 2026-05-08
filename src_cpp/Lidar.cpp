#include "Lidar.hpp"
#include <numeric>
#include <iostream>

// Bloco: Reconstrução superfície do teto do túnel

// Lógica: Filtro de Média Móvel com Buffer Circular
// Guarda as última "N" leituras e tira uma média, suavizando a visão
// do robô e eliminando ruídos. 

// ----- Construtor da classe LidarFilter -----
// size: tamanho da janela.
// buffer(size, 0): cria uma lista com 'size' espaços.
// window_size(size): guarda o número 'size' para calcular a média.
LidarFilter::LidarFilter(int size) : buffer(size, 0), window_size(size) {}

// Buffer Circular 
float LidarFilter::calcular(int nova_leitura) {

    // Substitui a leitura mais velha pela mais nova.
    buffer[head] = nova_leitura;

    // Lógica do buffer circular: quando o resto da divisão da posição pelo tamanho da janela
    // for 0 (indicando estar na última posição do buffer), head volta para a primeira posição.
    head = (head + 1) % window_size;

    // Função accumulate soma todos os itens da lista.
    float soma = std::accumulate(buffer.begin(), buffer.end(), 0.0);

    ultima_media = soma / window_size;
    std::cout << "[LIDAR] Medida atual: " << ultima_media << "\n";
    return ultima_media;
}

// Para leitura da última medida
float LidarFilter::get_ultima_media() {
    return ultima_media;
}