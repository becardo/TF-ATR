#!/usr/bin/env python3
import time
import pygame
import paho.mqtt.client as mqtt
from robot_physics import RobotPhysicsSimulator

# Variáveis Globais de Controle de Rede MQTT
aceleracao_recebida_cpp = 0.0
status_inspecao = 0
cpp_pronto = False  
sistema_iniciado = False
reset_solicitado = False

# Callback de recebimento 
# Atualiza as variáveis globais assim que uma ordem chega do C++ ou da GUI
def ao_receber_mensagem(client, userdata, msg):
    global aceleracao_recebida_cpp, status_inspecao, cpp_pronto, sistema_iniciado, reset_solicitado
    
    topico = msg.topic
    payload = msg.payload.decode()
    
    if topico == "tunel/cmd/aceleracao":
        aceleracao_recebida_cpp = float(payload)
    elif topico == "tunel/status/inspecao":
        status_inspecao = int(payload)
    elif topico == "tunel/sistema/status" and payload == "CPP_READY":
        cpp_pronto = True
    elif topico == "tunel/cmd/iniciar" and payload == "1":
        sistema_iniciado = True
    elif topico == "tunel/sistema/status" and payload == "SIMULACAO_CONCLUIDA":
        sistema_iniciado = False
        reset_solicitado = True

if __name__ == "__main__":
    print("[SISTEMA] Inicializando Ponte MQTT com a Física do Robô...")
    pygame.init()
    
    # Configuração e Conexão do cliente MQTT
    cliente_mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    cliente_mqtt.on_message = ao_receber_mensagem
    cliente_mqtt.connect("localhost", 1883, 60)
    
    # Inscrição nos tópicos de controle e sincronismo
    cliente_mqtt.subscribe("tunel/cmd/aceleracao")
    cliente_mqtt.subscribe("tunel/status/inspecao")
    cliente_mqtt.subscribe("tunel/sistema/status")
    cliente_mqtt.subscribe("tunel/cmd/iniciar")
    cliente_mqtt.loop_start() # Inicia a thread paralela da rede do Paho MQTT
    
    # Simulação física só começa a rodar quando o Python avisa que está vivo
    print("[SINCRONISMO] Publicando 'PYTHON_READY' e aguardando o Back-end C++...")
    cliente_mqtt.publish("tunel/sistema/status", "PYTHON_READY", retain=True)
    
    # Trava a inicialização aqui até receber a resposta do C++
    while not cpp_pronto:
        time.sleep(0.1)
    print("[OK] Handshake bem-sucedido! C++ detectado na rede.")
    
    # Instanciando o simulador físico após o sincronismo
    simulador = RobotPhysicsSimulator(dt=0.02)
    tempo_simulado = 0.0
    ultima_pos_enviada = -1.0 
    clock = pygame.time.Clock() 
    
    print("[OK] Iniciando loop de tempo real (Túnel: 250m)...")
    
    try:
        # Motor em Tempo Real - 50Hz
        while True:
            # --- O TELETRANSPORTE (HARD RESET) ---
            if reset_solicitado and aceleracao_recebida_cpp == 0.0:
                print("\n[SIMULADOR FÍSICO] Reset Total: Robô teletransportado para o início do túnel!")
                
                # Joga a física velha fora e cria um carrinho 100% novo!
                simulador = RobotPhysicsSimulator(dt=0.02) 
                
                # Zera as memórias de tempo e rede
                tempo_simulado = 0.0
                ultima_pos_enviada = -1.0 
                aceleracao_recebida_cpp = 0.0
                reset_solicitado = False
                
                # Pula para a próxima batida do motor para não processar nada velho
                continue

            telemetria = simulador.obter_estado_telemetria(sensor_ligado=sistema_iniciado)
            pos_atual = telemetria["posicao_x"]
            vel_real = telemetria["velocidade_real"]
            
            # Publica a velocidade real e leitura do lidar continuamente para a GUI ler
            cliente_mqtt.publish("tunel/sensor/velocidade", float(vel_real))
            cliente_mqtt.publish("tunel/sensor/lidar", float(telemetria["leitura_lidar"]))
            
            # Controle de vazão de rede (Filtro de QoS) baseado em metros inteiros
            pos_inteira = int(pos_atual)
            if pos_inteira != ultima_pos_enviada and pos_atual < 250.0:
                cliente_mqtt.publish("tunel/sensor/encoder", pos_inteira)
                ultima_pos_enviada = pos_inteira
            
            # Atualiza a engine física com a aceleração vinda da malha de controle do C++
            simulador.atualizar_física(aceleracao_recebida_cpp)
            
            # Impressão da telemetria a cada 0.05s ou no fim do túnel
            if int(tempo_simulado / 0.02) % 25 == 0 or pos_atual >= 250.0: 
                if not sistema_iniciado:
                    print(f"Tempo: {tempo_simulado:.2f}s | [STANDBY] Aguardando comando INICIAR pela GUI...")
                else:
                    alerta = "ALARME!" if status_inspecao == 1 else "OK"
                    vel_exibida = 0.00 if pos_atual >= 250.0 else vel_real
                    accel_exibida = 0.00 if pos_atual >= 250.0 else aceleracao_recebida_cpp
                    pos_exibida = 250.00 if pos_atual >= 250.0 else pos_atual

                    print(f"Tempo: {tempo_simulado:.2f}s | "
                          f"Pos X: {pos_exibida:>6.2f}m | "
                          f"Vel: {vel_exibida:>5.2f} m/s | "
                          f"LIDAR: {telemetria['leitura_lidar']:.3f}m | "
                          f"PID C++ Accel: {accel_exibida:>6.2f} m/s² | Status: {alerta}")
            
            # Trava de segurança - Fim de curso crítico
            if pos_atual >= 250.0:
                print(f"\n[FIM DE CURSO] Linha de chegada atingida ({pos_atual:.2f}m).")
                print("[SISTEMA] Cravando velocidade e aceleração em zero absoluto.")
                
                # Força os estados residuais finais na rede para o Coletor e GUI
                cliente_mqtt.publish("tunel/sensor/encoder", 250)
                cliente_mqtt.publish("tunel/sensor/velocidade", 0.0)
                cliente_mqtt.publish("tunel/sensor/lidar", 10.0)
                
                # Envia o comando de reset final no tópico de status do sistema
                cliente_mqtt.publish("tunel/sistema/status", "SIMULACAO_CONCLUIDA", retain=True)
                
                time.sleep(0.5) 
                #break  # Quebra o laço de execução principal com segurança
                
            clock.tick(50) 
            tempo_simulado += 0.02
            
    except KeyboardInterrupt:
        print("\n[SISTEMA] Interrupção manual do usuário recebida.")
    finally:
        print("[SISTEMA] Encerrando simulador de forma segura e liberando portas...")
        cliente_mqtt.loop_stop()
        cliente_mqtt.disconnect()
        pygame.quit()