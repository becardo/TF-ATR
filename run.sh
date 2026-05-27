#!/bin/bash
echo "Compilando o backend C++..."
make

echo "Iniciando a simulação física (Python)..."
python3 src_python/py_mqtt.py &
PID_FISICA=$!

# echo "Iniciando a Interface Gráfica (Pygame)..."
# python3 src_python/gui_operacao.py &
# PID_GUI=$!

# Armadilha atualizada para matar os DOIS scripts Python
cleanup() {
    echo -e "\n[SISTEMA] Encerrando todos os processos de forma limpa..."
    kill -9 $PID_FISICA $PID_GUI 2>/dev/null
    killall -9 inspecao_tuneis 2>/dev/null
}
trap cleanup EXIT INT TERM

echo "Iniciando o Controle de Navegação (C++)..."
./inspecao_tuneis