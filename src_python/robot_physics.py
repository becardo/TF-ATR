#!/usr/bin/env python3
import random
import math
import pygame

class RobotPhysicsSimulator:
    def __init__(self, dt=0.02):
        self.dt = dt
        
        # Estados dinâmicos do robô
        self.posicao_x = 0.0      
        self.velocidade_x = 0.0   
        self.aceleracao_x = 0.0   
        
        # Parâmetros do cenário do túnel
        self.comprimento_tunel = 100  
        self.altura_nominal_teto = 10.0 
        self.ruido_desvio_padrao = 0.03 
        
        self.resolucao_mapa = 0.1  
        self.mapa_teto = self._gerar_perfil_teto()

    def _gerar_perfil_teto(self):
        num_pontos = int(self.comprimento_tunel / self.resolucao_mapa)
        perfil = [self.altura_nominal_teto] * num_pontos
        
        # Injeção de anomalias geométricas no mapa do teto
        # Saliência entre 10m e 12m
        idx_ini_sal, idx_fim_sal = int(10 / self.resolucao_mapa), int(12 / self.resolucao_mapa)
        for i in range(idx_ini_sal, idx_fim_sal):
            perfil[i] = 9.0
            
        # Buraco entre 25m e 28m
        idx_ini_bur, idx_fim_bur = int(25 / self.resolucao_mapa), int(28 / self.resolucao_mapa)
        for i in range(idx_ini_bur, idx_fim_bur):
            perfil[i] = 11.2

        # Deformação gradual (parábola) entre 45m e 50m
        idx_ini_sua, idx_fim_sua = int(45 / self.resolucao_mapa), int(50 / self.resolucao_mapa)
        for i in range(idx_ini_sua, idx_fim_sua):
            progresso = (i - idx_ini_sua) / (idx_fim_sua - idx_ini_sua)
            perfil[i] = 10.0 - 1.5 * math.sin(progresso * math.pi)
            
        return perfil

    def atualizar_física(self, nova_aceleracao):
        self.aceleracao_x = max(min(nova_aceleracao, 100.0), -100.0)
        
        # Fator mecânico para simular a inércia do robô
        aceleracao_real = self.aceleracao_x * 0.1 
        
        # Integração numérica de Euler (V = V0 + a*dt)
        self.velocidade_x += aceleracao_real * self.dt
        if self.velocidade_x < 0.0: 
            self.velocidade_x = 0.0 
            
        # Integração numérica de Euler (X = X0 + V*dt)
        self.posicao_x += self.velocidade_x * self.dt
        
        if self.posicao_x > self.comprimento_tunel:
            self.posicao_x = float(self.comprimento_tunel)
            self.velocidade_x = 0.0

    def ler_sensor_lidar(self):
        indice = int(self.posicao_x / self.resolucao_mapa)
        indice = max(min(indice, len(self.mapa_teto) - 1), 0)
        
        altura_base = self.mapa_teto[indice]
        ruido = random.gauss(0, self.ruido_desvio_padrao)
        
        return round(altura_base + ruido, 3)

    def obter_estado_telemetria(self):
        return {
            "posicao_x": round(self.posicao_x, 4),
            "velocidade_real": round(self.velocidade_x, 4),
            "aceleracao_atual": round(self.aceleracao_x, 4),
            "leitura_lidar": self.ler_sensor_lidar()
        }

if __name__ == "__main__":
    print("[TESTE FISICA] Iniciando simulação com controle temporal estável do Pygame...")
    pygame.init()
    
    simulador = RobotPhysicsSimulator(dt=0.02)
    aceleracao_mock = 45.0 
    tempo_simulado = 0.0
    clock = pygame.time.Clock()
    
    for _ in range(250): 
        simulador.atualizar_física(aceleracao_mock)
        telemetria = simulador.obter_estado_telemetria()
        
        # Mostra os dados a cada 500ms no console
        if int(tempo_simulado / 0.02) % 25 == 0:
            print(f"Tempo: {tempo_simulado:.2f}s | "
                  f"Pos X: {telemetria['posicao_x']:>6}m | "
                  f"Vel: {telemetria['velocidade_real']:>5} m/s | "
                  f"LIDAR: {telemetria['leitura_lidar']}m")
            
        # Garante a taxa estável de 50 FPS (ciclos de 20ms) compensando atrasos internos
        clock.tick(50) 
        tempo_simulado += 0.02
        
    pygame.quit()