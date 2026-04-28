# 🤖 Sistema de Inspeção Autônoma de Túneis  
### Trabalho Final – Automação em Tempo Real

Este projeto consiste no desenvolvimento de um sistema completo de **inspeção automática de túneis**, utilizando conceitos de sistemas em tempo real, concorrência e simulação física.

A proposta é implementar um robô autônomo capaz de **navegar, mapear e detectar falhas estruturais** no teto de túneis, operando de forma inteligente e integrada com um sistema remoto.

---

## 👩‍💻 Integrantes

- **Ana Beatriz Soares Cardoso** – [@becardo](https://github.com/becardo)  
- **Julia Pereira Maia Ribeiro** – [@ribju](https://github.com/ribju)  
- **Pedro Eduardo Alvino Tapia** – [@ptapia7](https://github.com/ptapia7)  

👨‍🏫 **Professor:** Leandro Freitas  

---

## 📌 Objetivo

Desenvolver um sistema embarcado capaz de:

- 🚗 Controlar a navegação de um robô em um túnel  
- 📏 Medir distâncias e reconstruir o perfil do teto  
- ⚠️ Detectar anomalias (buracos, saliências, falhas)  
- 📸 Acionar inspeção detalhada via câmera  
- 📡 Comunicar-se com um sistema remoto via MQTT  
- 🧠 Operar com múltiplas tarefas concorrentes e sincronizadas  

---

## 🧠 Arquitetura do Sistema

O sistema é composto por múltiplas **tarefas executando em paralelo**, simulando um ambiente real de tempo real:

### 🔹 Principais módulos

- **Comando de Navegação**  
  Responsável por interpretar comandos e definir o comportamento do robô (manual/automático).

- **Controle de Navegação (PID)**  
  Controla a velocidade do robô com base no setpoint.

- **Cálculo de Distância**  
  Usa dados de encoder para calcular deslocamento.

- **Reconstrução da Superfície**  
  Processa dados do LIDAR e detecta anomalias.

- **Inspeção por Câmera**  
  Executa processamento intensivo ao detectar falhas.

- **Coletor de Dados**  
  Armazena informações com timestamp, posição e nível de confiança.

- **Simulação do Ambiente**  
  Emula sensores e física do sistema.

- **Operação Remota**  
  Interface para monitoramento e controle.

---

## ⚙️ Tecnologias e Conceitos

- 🧵 Programação concorrente (multithreading)
- 🔐 Sincronização:
  - Mutex
  - Semáforos
  - Variáveis de condição
- 📡 Comunicação via MQTT
- 🎮 Simulação (ex: Python + Pygame)
- ⚙️ Controle PID
- 📊 Processamento de sinais (filtros de média móvel)

---

## 📝 Padrão de Commits

Este projeto segue o padrão **Conventional Commits**, visando manter um histórico organizado, legível e profissional.

### 📌 Formato

| Tipo      | Descrição |
|----------|----------|
| `feat`   | Nova funcionalidade |
| `fix`    | Correção de bug |
| `refactor` | Refatoração de código (sem alterar comportamento) |
| `docs`   | Alterações na documentação |
| `style`  | Formatação (indentação, espaços, etc.) |
| `test`   | Adição ou alteração de testes |
| `chore`  | Configurações e tarefas auxiliares |


