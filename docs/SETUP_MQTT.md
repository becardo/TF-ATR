1. Abra o terminal e instale o Mosquitto e as ferramentas de cliente para teste:

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients
```

2. Garanta que ele está rodando:

```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

** Teste rápido: abra dois terminais. Em um digite ``` mosquitto_sub -t "teste" ```. No outro, digite ``` mosquitto_pub -t "teste" -m "Ola" ```. Se o "Ola" aparecer no primeiro terminal, o seu broker está correto.

*** Como abrir dois terminais? 
---- no terminal Ubunto (o que vc usa no vs code), dentro da pasta do projeto, rode o comando ``` tmux ``` (pode ser necessária a instalação do mesmo). Com ctrl+B e shift+5, você consegue abrir outro terminal na mesma tela. Para transitar entre os terminais, ctrl+B e setinhas do teclado para esquerda e direita. ctrl+D para sair da tela tmux.

3. Tabela de Tópicos

As comunicações que existem no nosso código, entre o Python e o shared_buffer.hpp.

| Tópico MQTT            | Publicador (Envia) | Assinante (Escuta) | Formato (Payload) | Descrição do Dado                                        |
| :---                   | :---               | :---               | :---              | :---                                                     |
| `tunel/sensor/encoder` | **Python** (Front) | **C++** (Back)     | `int`             | Distância percorrida no eixo X (em metros).              |
| `tunel/sensor/lidar`   | **Python** (Front) | **C++** (Back)     | `float`           | Distância real até o teto contendo a injeção de ruído.   |
| `tunel/cmd/aceleracao` | **C++** (Back)     | **Python** (Front) | `double`          | Esforço de aceleração (PWM) calculado pela malha do PID. |
| `tunel/status/inspecao`| **C++** (Back)     | **Python** (Front) | `int` / `bool`    | `0` (Operação Normal) ou `1` (IA da Câmera ativada).     |

4. Configurar o Python(Front-end):

Biblioteca oficial do Paho. Instale a biblioteca forçando a permissão:

```bash

```

-> arquivo teste_mqtt.py criado

5. Configurar C++ (Back-end):

No C++, a forma mais estável e fácil de conectar no Ubuntu é usando a biblioteca Paho em C. Instale a biblioteca baseada em C direto pelo terminal:

```bash
sudo apt install libpaho-mqtt-dev
```

6. Executar: 

- Rodar um ``` mchmod +x run.sh ``` uma vez
- terminal: ``` ./run.sh ```

Isso irá executar c++ e o python ao mesmo tempo. Quando chegar em 100m, velocidade e aceleração ficam 0. FLAGS continuam como um estado de "carro parado".