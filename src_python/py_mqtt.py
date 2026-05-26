#!/usr/bin/env python3
import pygame
import paho.mqtt.client as mqtt
from robot_physics import RobotPhysicsSimulator

# Variáveis Globais de Controle de Rede MQTT
aceleracao_recebida_cpp = 0.0
status_inspecao = 0

def ao_receber_mensagem(client, userdata, msg):
    global aceleracao_recebida_cpp, status_inspecao
    topico = msg.topic
    payload = msg.payload.decode()
    
    if topico == "tunel/cmd/aceleracao":
        aceleracao_recebida_cpp = float(payload)
    elif topico == "tunel/status/inspecao":
        status_inspecao = int(payload)

if __name__ == "__main__":
    print("[SISTEMA] Inicializando Ponte MQTT com a Física do Robô...")
    pygame.init()
    
    cliente_mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    cliente_mqtt.on_message = ao_receber_mensagem
    cliente_mqtt.connect("localhost", 1883, 60)
    
    cliente_mqtt.subscribe("tunel/cmd/aceleracao")
    cliente_mqtt.subscribe("tunel/status/inspecao")
    cliente_mqtt.loop_start()
    
    simulador = RobotPhysicsSimulator(dt=0.02)
    tempo_simulado = 0.0
    ultima_pos_enviada = -1.0 # Controle de vazão de rede
    clock = pygame.time.Clock()
    
    print("[OK] Conectado ao Broker Mosquitto. Aguardando dados do Back-end C++...")
    
    try:
        while True:
            telemetria = simulador.obter_estado_telemetria()
            pos_atual = int(telemetria["posicao_x"])
            
            # Só inunda a rede com publicação se o robô se mover no eixo X (Filtro de QoS)
            if pos_atual != ultima_pos_enviada:
                cliente_mqtt.publish("tunel/sensor/encoder", pos_atual)
                cliente_mqtt.publish("tunel/sensor/lidar", float(telemetria["leitura_lidar"]))
                ultima_pos_enviada = pos_atual
            
            simulador.atualizar_física(aceleracao_recebida_cpp)
            
            if int(tempo_simulado / 0.02) % 25 == 0:
                alerta = "ALARME!" if status_inspecao == 1 else " OK"
                print(f"Tempo: {tempo_simulado:.2f}s | "
                      f"Pos X: {telemetria['posicao_x']:>6.2f}m | "
                      f"Vel: {telemetria['velocidade_real']:>5.2f} m/s | "
                      f"LIDAR: {telemetria['leitura_lidar']:.3f}m | "
                      f"PID C++ Accel: {aceleracao_recebida_cpp:>6.2f} m/s² | Status: {alerta}")
                
            clock.tick(50) 
            tempo_simulado += 0.02
            
    except KeyboardInterrupt:
        print("\n[SISTEMA] Encerrando simulador de forma segura...")
    finally:
        cliente_mqtt.loop_stop()
        cliente_mqtt.disconnect()
        pygame.quit()