#!/usr/bin/env python3
import os
import sys
import math
import pygame
from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import QTimer
from PyQt6.QtGui import QImage, QPixmap, QPainter

# Desativa drivers de vídeo físicos do Pygame/SDL (Roda em modo "Dummy" na memória)
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = "hide"

class PygameWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        
        # Variáveis de telemetria sincronizadas
        self.encoder_m = 0
        self.velocidade_ms = 0.0
        self.lidar_m = 10.0
        self.status_inspecao = 0
        
        # Parâmetros de escala e resolução idênticos ao RobotPhysicsSimulator
        self.resolucao_mapa = 0.1  # 1 ponto a cada 10cm
        self.escala_y = 20.0       # Multiplicador para converter metros em pixels na tela
        
        # Inicializa o Pygame internamente sem criar janela física
        pygame.init()
        pygame.font.init()
        
        self.largura = 600
        self.altura = 350
        self.surface_virtual = pygame.Surface((self.largura, self.altura))
        self.q_pixmap = QPixmap()

        # Timer interno para gerenciar o redesenho constante (~60 FPS)
        self.timer_fps = QTimer(self)
        self.timer_fps.timeout.connect(self.processar_e_atualizar)
        self.timer_fps.start(16) 

    def atualizar_dados(self, encoder, velocidaded, lidar, status):
        """Atualiza os parâmetros internos vindo do broker MQTT."""
        self.encoder_m = encoder
        self.velocidade_ms = float(velocidaded)
        self.lidar_m = float(lidar)
        self.status_inspecao = int(status)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.largura = max(self.width(), 100)
        self.altura = max(self.height(), 100)
        self.surface_virtual = pygame.Surface((self.largura, self.altura))

    def processar_e_atualizar(self):
        self.renderizar_conteudo_pygame()
        self.converter_pygame_para_pyqt()
        self.update()

    def calcular_altura_teto_em_x(self, x_metros):
        """Replicador matemático exato do perfil gerado no RobotPhysicsSimulator."""
        altura_nominal = 10.0
        
        # === Bloco 1 de anomalias 0m - 50m ===
        # Saliência retangular (Degrau Seco): m10 ao m12
        if 10.0 <= x_metros < 12.0:
            return 9.0
        # Buraco retangular (Degrau seco): m25 ao m28
        if 25.0 <= x_metros < 28.0:
            return 11.2
        # Saliência suave (Curva Senoidal): m45 ao m50
        if 45.0 <= x_metros <= 50.0:
            progresso = (x_metros - 45.0) / (50.0 - 45.0)
            return 10.0 - 1.5 * math.sin(progresso * math.pi)
            
        # === Bloco 2 de anomalias 150m - 250m ===
        # Buraco retangular: m150 ao m155
        if 150.0 <= x_metros < 155.0:
            return 11.5
        # Ondulações rítmicas de concreto: m200 ao m220
        if 200.0 <= x_metros <= 220.0:
            # Usando a mesma discretização aproximada do índice do vetor
            indice_simulado = x_metros / self.resolucao_mapa
            return 10.0 - 1.2 * math.sin(indice_simulado * 0.5)
            
        return altura_nominal

    def renderizar_conteudo_pygame(self):
        """Desenha a lógica do robô e o teto com anomalias baseado no deslocamento."""
        COR_FUNDO = (30, 30, 30)
        COR_PAREDE = (65, 65, 65)
        COR_ROBO = (0, 120, 255) if self.status_inspecao == 0 else (230, 40, 40)
        COR_LASER = (0, 255, 60) if self.status_inspecao == 0 else (255, 30, 30)

        self.surface_virtual.fill(COR_FUNDO)

        # Altura física de referência para o chão da simulação
        chao_y = int(self.altura * 0.75)
        
        # O robô fica fixado no centro horizontal da viewport
        centro_tela_x = self.largura // 2

        # -----------------------------------------------------------------
        # GERAÇÃO E RENDERIZAÇÃO DO TETO DINÂMICO COORDENADO PELO ENCODER
        # -----------------------------------------------------------------
        pontos_teto_pixels = []
        
        # Varre cada pixel de largura da tela para amostrar o perfil correto do túnel
        for pixel_x in range(0, self.largura, 2): # Amostragem a cada 2px para otimizar performance
            # Calcula a distância em metros desse pixel em relação à posição global do robô
            distancia_relativa_m = (pixel_x - centro_tela_x) / self.escala_y
            posicao_global_m = self.encoder_m + distancia_relativa_m
            
            # Limita a leitura matemática ao tamanho total do túnel mapeado
            posicao_global_m = max(0.0, min(posicao_global_m, 250.0))
            
            # Obtém a altura física estrutural do teto (em metros) naquele ponto do mapa
            altura_m = self.calcular_altura_teto_em_x(posicao_global_m)
            
            # Converte a leitura métrica para coordenadas de pixels na tela
            # O teto é desenhado de cima para baixo a partir do chão do simulador
            teto_pixel_y = chao_y - int(altura_m * self.escala_y)
            pontos_teto_pixels.append((pixel_x, teto_pixel_y))

        # Desenha o perfil recortado do teto deformado usando linhas conectadas
        if len(pontos_teto_pixels) > 1:
            pygame.draw.lines(self.surface_virtual, COR_PAREDE, False, pontos_teto_pixels, 5)

        # -----------------------------------------------------------------
        # RENDERIZAÇÃO DO CHÃO (ESTEIRA DE MOVIMENTO INFINITO)
        # -----------------------------------------------------------------
        # Deslocamento visual baseado no acumulador do encoder para sincronizar com o teto
        offset_chao = -int((self.encoder_m * self.escala_y) % 40)
        for x in range(offset_chao - 40, self.largura + 40, 40):
            pygame.draw.line(self.surface_virtual, (55, 55, 55), (x, chao_y), (x + 15, chao_y + 12), 3)

        pygame.draw.line(self.surface_virtual, COR_PAREDE, (0, chao_y), (self.largura, chao_y), 5)

        # -----------------------------------------------------------------
        # DESENHO DO ROBÔ E DO FEIXE LASER ATIVO
        # -----------------------------------------------------------------
        r_largura, r_altura = 75, 32
        r_x = centro_tela_x - (r_largura // 2)
        r_y = chao_y - r_altura

        # Desenha Chassi e Rodas
        pygame.draw.rect(self.surface_virtual, COR_ROBO, (r_x, r_y, r_largura, r_altura), border_radius=4)
        pygame.draw.circle(self.surface_virtual, (15, 15, 15), (r_x + 18, chao_y), 9)
        pygame.draw.circle(self.surface_virtual, (15, 15, 15), (r_x + r_largura - 18, chao_y), 9)

        # Emissor óptico centralizado
        s_x = r_x + (r_largura // 2)
        s_y = r_y - 4
        pygame.draw.rect(self.surface_virtual, (180, 180, 180), (s_x - 6, s_y, 12, 4))

        # Calcula o ponto de colisão exato do laser baseado no teto calculado para a posição central do robô
        altura_teto_atual_m = self.calcular_altura_teto_em_x(self.encoder_m)
        laser_fim_y = chao_y - int(altura_teto_atual_m * self.escala_y)

        # Desenha feixe dinâmico e o ponto de impacto
        pygame.draw.line(self.surface_virtual, COR_LASER, (s_x, s_y), (s_x, laser_fim_y), 2)
        pygame.draw.circle(self.surface_virtual, COR_LASER, (s_x, laser_fim_y), 5)

        # -----------------------------------------------------------------
        # TELEMETRIA OVERLAY (TEXTOS)
        # -----------------------------------------------------------------
        font = pygame.font.SysFont("monospace", 12, bold=True)
        self.surface_virtual.blit(font.render(f"Posicao: {self.encoder_m} m", True, (200, 200, 200)), (15, 15))
        self.surface_virtual.blit(font.render(f"Velocidade: {self.velocidade_ms:.2f} m/s", True, (200, 200, 200)), (15, 32))
        self.surface_virtual.blit(font.render(f"LIDAR (Real): {self.lidar_m:.2f} m", True, COR_LASER), (15, 49))

        if self.status_inspecao == 1:
            font_aviso = pygame.font.SysFont("sans", 14, bold=True)
            txt = font_aviso.render("FALHA DE ESTRUTURA DETECTADA NO TETO", True, (255, 50, 50))
            self.surface_virtual.blit(txt, (self.largura // 2 - txt.get_width() // 2, 15))

    def converter_pygame_para_pyqt(self):
        dados_raw = pygame.image.tostring(self.surface_virtual, 'RGB')
        q_img = QImage(dados_raw, self.largura, self.altura, QImage.Format.Format_RGB888)
        self.q_pixmap = QPixmap.fromImage(q_img)

    def paintEvent(self, event):
        if not self.q_pixmap.isNull():
            painter = QPainter(self)
            painter.drawPixmap(0, 0, self.q_pixmap)
            painter.end()

    def closeEvent(self, event):
        pygame.quit()
        event.accept()