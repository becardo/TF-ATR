#ifndef COLETOR_HPP
#define COLETOR_HPP

#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include "shared_buffers.hpp"

// Classe Coletor de Dados: lógica de salvar arquivos no HD 
class ColetorCSV {
private:
    std::ofstream arquivo;

public:
    // Construtor: Abre o arquivo assim que a classe for instanciada
    ColetorCSV(const std::string& nome_arquivo) {
        arquivo.open(nome_arquivo, std::ios::app);
        if (!arquivo.is_open()) {
            std::cerr << "[ERRO] Falha ao abrir o arquivo de log: " << nome_arquivo << std::endl;
        }
    }

    // Destrutor: Fecha o arquivo sozinho para não corromper os dados
    ~ColetorCSV() {
        if (arquivo.is_open()) {
            arquivo.close();
        }
    }

    bool estaAberto() const {
        return arquivo.is_open();
    }

    // Método para salvar os dados
    void gravar(const Medicao& dado) {
        if (arquivo.is_open()) {
            arquivo << dado.timestamp << "," 
                    << dado.i_encoder << "," 
                    << std::fixed << std::setprecision(2) << dado.i_lidar << "," 
                    << std::fixed << std::setprecision(2) << dado.nivel_confianca << std::endl;
        }
    }
};

// Protótipo da função da thread
void t_coletor_dados(SensorBuffer& sensor);

#endif