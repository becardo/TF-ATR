from PyQt6.QtWidgets import QWidget, QVBoxLayout
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

class GraficoLidar(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)  # Inicialização da classe pai
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0) # Remoção de margens externas

        # Guardar os pontos atuais X (encoder) e Y (LIDAR)
        self.historico_x = []
        self.historico_y = []

        # Criação da figura Matplotlib
        self.fig = Figure(figsize=(5, 3), facecolor='#151515')
        self.canvas = FigureCanvas(self.fig)
        layout.addWidget(self.canvas)

        # Configuração dos Eixos
        self.ax = self.fig.add_subplot(111) # 1 linha, 1 coluna, primeiro grafico
        self.fig.subplots_adjust( # Ajuste das margens
            left=0.12,
            right=0.98,
            top=0.90,
            bottom=0.18
        )
        self.ax.set_facecolor('#151515')
        
        # Legendas do painel
        self.ax.set_title('GRÁFICO DE DADOS LIDAR PROCESSADOS', color='#00ff00', fontfamily='monospace', fontsize=10, fontweight='bold', pad=10)
        self.ax.set_xlabel('X: POSIÇÃO HORIZONTAL (m)', color='#00ff00', fontfamily='monospace', fontsize=8, fontweight='bold')
        self.ax.set_ylabel('Y: DISTÂNCIA DO TETO / ALTURA (m)', color='#00ff00', fontfamily='monospace', fontsize=8, fontweight='bold')
        
        # Deixa os números das escalas e as bordas verdes
        self.ax.tick_params(colors='#00ff00', labelsize=8)
        for spine in self.ax.spines.values():
            spine.set_color('#333') # cor das bordas

        # Limites fixos de resolução do túnel (primeiros 40m) e altura regulada (acima do valor máximo de buraco encontrado)
        self.ax.set_xlim(0, 40)
        self.ax.set_ylim(0, 14)
        self.ax.grid(True, linestyle='--', color='#252525', alpha=0.7) # ativa a grade do gráfico

        # Cria a linha verde do gráfico
        self.linha_dinamica, = self.ax.plot([], [], color='#00ff00', linewidth=2)

    # Recebe a telemetria do MQTT e adiciona no gráfico se o robô moveu
    def atualizar_ponto(self, x_novo, y_novo):
        if x_novo > 0:
            # Filtro para não duplicar pontos na mesma posição
            if not self.historico_x or self.historico_x[-1] != x_novo: 
                # armazena novas posições
                self.historico_x.append(x_novo)
                self.historico_y.append(y_novo)
                
                # Atualiza os dados da linha e redesenha a tela de forma leve
                self.linha_dinamica.set_data(self.historico_x, self.historico_y)

                largura_da_janela = 40  # Quantos metros aparecem na tela por vez
                folga_na_frente = 5     # Margem para ver o teto um pouco à frente do robô
                
                # Gráfico móvel
                if x_novo > (largura_da_janela - folga_na_frente):
                    # Se o robô avançou, a janela desliza junto com ele
                    limite_esquerdo = x_novo - largura_da_janela + folga_na_frente
                    limite_direito = x_novo + folga_na_frente
                    self.ax.set_xlim(limite_esquerdo, limite_direito)
                else:
                    # Se ele está no início do túnel, mantém de 0 a 40m fixo
                    self.ax.set_xlim(0, largura_da_janela)

                self.canvas.draw_idle()

    # Limpa a tela 
    def resetar_grafico(self):
        self.historico_x.clear()
        self.historico_y.clear()
        self.linha_dinamica.set_data([], [])
        self.ax.set_xlim(0, 40)
        self.canvas.draw_idle()