#!/usr/bin/env python3
import random
import math
import pygame

# Motor físico do ambiente - Simulação do mundo real
class RobotPhysicsSimulator:
    def __init__(self, dt=0.02):
        self.dt = dt
        
        # Estados dinâmicos do robô (nascem em repouso)
        self.posicao_x = 0.0      
        self.velocidade_x = 0.0   
        self.aceleracao_x = 0.0   
        
        # Parâmetros do cenário estendido do túnel
        self.comprimento_tunel = 250  
        self.altura_nominal_teto = 10.0 

        # Desvio padrão do Ruído Branco
        self.ruido_desvio_padrao = 0.03 
        
        self.resolucao_mapa = 0.1  
        self.mapa_teto = self._gerar_perfil_teto()

    def _gerar_perfil_teto(self):
        # Cria um vetor de 2500 posições preenchendo com o teto
        num_pontos = int(self.comprimento_tunel / self.resolucao_mapa)
        perfil = [self.altura_nominal_teto] * num_pontos
        
        # === Bloco 1 de anomalias 0m - 50m ===

        # Saliência retangular (Degrau Seco): do metro 10 ao metro 12
        idx_ini_sal, idx_fim_sal = int(10 / self.resolucao_mapa), int(12 / self.resolucao_mapa)
        for i in range(idx_ini_sal, idx_fim_sal):
            perfil[i] = 9.0
            
        # Buraco retangular (Degrau seco): do metro 25 ao metro 28
        idx_ini_bur, idx_fim_bur = int(25 / self.resolucao_mapa), int(28 / self.resolucao_mapa)
        for i in range(idx_ini_bur, idx_fim_bur):
            perfil[i] = 11.2

        # Saliência suave (Curva Senoidal que afunda até 8.5m): do metro 45 ao metro 50
        idx_ini_sua, idx_fim_sua = int(45 / self.resolucao_mapa), int(50 / self.resolucao_mapa)
        for i in range(idx_ini_sua, idx_fim_sua):
            progresso = (i - idx_ini_sua) / (idx_fim_sua - idx_ini_sua)
            perfil[i] = 10.0 - 1.5 * math.sin(progresso * math.pi)
            
        # === Bloco 2 de anomalias 150m - 250m ===

        # Buraco retangular (Degrau seco): do metro 150 ao metro 155
        idx_ini_b2, idx_fim_b2 = int(150 / self.resolucao_mapa), int(155 / self.resolucao_mapa)
        for i in range(idx_ini_b2, idx_fim_b2):
            perfil[i] = 11.5

        # Adiciona uma zona densa de ondulações rítmicas de concreto entre o metro 200 e o metro 220
        idx_ini_ond, idx_fim_ond = int(200 / self.resolucao_mapa), int(220 / self.resolucao_mapa)
        for i in range(idx_ini_ond, idx_fim_ond):
            perfil[i] = 10.0 - 1.2 * math.sin(i * 0.5)

        return perfil

    def atualizar_física(self, nova_aceleracao):
        self.aceleracao_x = max(min(nova_aceleracao, 100.0), -100.0) # Limites de força do robo
        
        # Fator mecânico de inércia
        aceleracao_real = self.aceleracao_x * 0.1 
        
        # Integra a aceleração para encontrar a velocidade (V = V0 + a*dt)
        self.velocidade_x += aceleracao_real * self.dt
            
        # Integra a velocidade para encontrar a nova posição (X = X0 + V*dt)
        self.posicao_x += self.velocidade_x * self.dt
        
        # Proteção estrita de fim de curso (250m)
        if self.posicao_x > self.comprimento_tunel:
            self.posicao_x = float(self.comprimento_tunel)
            self.velocidade_x = 0.0
            self.aceleracao_x = 0.0

    def ler_sensor_lidar(self):
        # Transforma a posição contínua em metros num índice discreto do vetor
        indice = int(self.posicao_x / self.resolucao_mapa)

        # Trava de segurança da array para não tentar ler fora da memória
        indice = max(min(indice, len(self.mapa_teto) - 1), 0)
        
        altura_base = self.mapa_teto[indice]

        # Injeta ruído estocástico na medição 
        ruido = random.gauss(0, self.ruido_desvio_padrao)
        
        return round(altura_base + ruido, 3)

    def obter_estado_telemetria(self, sensor_ligado=True):
        # Empacota os dados para enviar para o MQTT
        return {
            "posicao_x": round(self.posicao_x, 4),
            "velocidade_real": round(self.velocidade_x, 4),
            "aceleracao_atual": round(self.aceleracao_x, 4),
            "leitura_lidar": self.ler_sensor_lidar() if sensor_ligado else 10.0
        }

if __name__ == "__main__":
    print("[TESTE FISICA] Iniciando simulação temporal estável (Túnel: 250m)...")
    pygame.init()
    
    simulador = RobotPhysicsSimulator(dt=0.02)
    aceleracao_mock = 45.0 
    tempo_simulado = 0.0
    clock = pygame.time.Clock()
    
    # Executa por 500 ciclos para dar tempo de o robô correr mais no túnel longo
    for _ in range(500): 
        simulador.atualizar_física(aceleracao_mock)
        telemetria = simulador.obter_estado_telemetria()
        
        if int(tempo_simulado / 0.02) % 25 == 0:
            print(f"Tempo: {tempo_simulado:.2f}s | "
                  f"Pos X: {telemetria['posicao_x']:>7.2f}m | "
                  f"Vel: {telemetria['velocidade_real']:>5.2f} m/s | "
                  f"LIDAR: {telemetria['leitura_lidar']:.3f}m")
            
        clock.tick(50) 
        tempo_simulado += 0.02
        
    pygame.quit()