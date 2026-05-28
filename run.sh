#!/bin/bash

echo "Compilando o backend C++..."
make
if [ $? -ne 0 ]; then
    echo "Erro na compilação! Abortando."
    exit 1
fi

# Nome da sessão do tmux
SESSION="inspecao"

# Mata uma sessão antiga com o mesmo nome, se existir
tmux kill-session -t $SESSION 2>/dev/null

echo "Iniciando os processos no tmux..."

# 1. Cria a sessão e roda o MQTT no primeiro painel
tmux new-session -d -s $SESSION -n "Simulacao" "python3 -u src_python/py_mqtt.py; read"

# 2. Divide a tela horizontalmente e roda a GUI (Pygame)
tmux split-window -h -t $SESSION "python3 -u src_python/gui_main.py; read"

# 3. Divide a tela verticalmente para rodar o C++
tmux split-window -v -t $SESSION "./inspecao_tuneis; read"

# Organiza o layout dos painéis de forma justa
tmux select-layout -t $SESSION tiled

# Abre a tela do tmux para você ver tudo acontecendo
tmux attach-session -t $SESSION