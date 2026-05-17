import tkinter as tk

class DiagramaArquiteturaMestre:
    def __init__(self, root):
        self.root = root
        self.root.title("Arquitetura Detalhada do Sistema - Etapa 1")
        # Canvas grande para acomodar todos os componentes sem aperto
        self.canvas = tk.Canvas(root, width=1450, height=850, bg="#F8F9FA")
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.desenhar_diagrama()

    def desenhar_bloco(self, x, y, largura, altura, texto, cor_fundo, cor_borda, fonte_tamanho=10):
        """Função auxiliar para desenhar caixas padronizadas."""
        self.canvas.create_rectangle(x, y, x + largura, y + altura, fill=cor_fundo, outline=cor_borda, width=2)
        cor_texto = "#000000" if cor_fundo in ["#F6AD55", "#D69E2E"] else "#FFFFFF"
        self.canvas.create_text(x + largura/2, y + altura/2, text=texto, font=("Times New Roman", fonte_tamanho, "bold"), justify=tk.CENTER, fill=cor_texto)

    def desenhar_sinal(self, x, y, largura, altura, texto):
        """Função auxiliar para desenhar o formato oval das Condition Variables."""
        self.canvas.create_oval(x, y, x + largura, y + altura, fill="#E53E3E", outline="#9B2C2C", width=2)
        self.canvas.create_text(x + largura/2, y + altura/2, text=texto, font=("Times New Roman", 9, "bold"), justify=tk.CENTER, fill="#FFFFFF")

    def desenhar_seta(self, x1, y1, x2, y2, texto="", curvatura=False, offset_texto_y=0, invertida=False):
        """Desenha setas de conexão com suporte a rótulos e curvatura opcional."""
        if curvatura:
            # Ponto de curvatura para as setas não encavalarem
            xm = (x1 + x2) / 2
            self.canvas.create_line(x1, y1, xm, (y1+y2)/2, x2, y2, arrow=tk.LAST, width=2, fill="#2D3748")
            xm_text, ym_text = xm, (y1+y2)/2
        else:
            self.canvas.create_line(x1, y1, x2, y2, arrow=tk.LAST, width=2, fill="#2D3748")
            xm_text, ym_text = (x1 + x2) / 2, (y1 + y2) / 2

        if texto:
            # Fundo branco para o texto da seta facilitar a leitura
            bbox = self.canvas.create_text(xm_text, ym_text + offset_texto_y, text=texto, font=("Times New Roman", 9, "bold"), fill="#C53030")
            x0, y0, x1_box, y1_box = self.canvas.bbox(bbox)
            self.canvas.create_rectangle(x0 - 2, y0 - 2, x1_box + 2, y1_box + 2, fill="#F8F9FA", outline="")
            # Redesenha o texto por cima do fundo branco
            self.canvas.create_text(xm_text, ym_text + offset_texto_y, text=texto, font=("Times New Roman", 9, "bold"), fill="#C53030")

    def desenhar_diagrama(self):
        # Paleta de Cores e Tamanhos Padronizados
        c_cpp = "#2B5B84"      # Azul (Backend C++)
        c_python = "#D69E2E"   # Amarelo (Frontend Python)
        c_mutex = "#F6AD55"    # Laranja (Sincronização)
        c_io = "#48BB78"       # Verde (Ficheiros)

        w_t = 240 # Largura Threads
        h_t = 60  # Altura Threads
        w_m = 260 # Largura Mutex
        
        # ==========================================
        # 1. CAMADA DE EXECUÇÃO (COLUNA ESQUERDA)
        # ==========================================
        # Agrupamento tracejado do C++
        self.canvas.create_rectangle(30, 20, 900, 780, fill="", outline="#1A365D", width=3, dash=(5, 5))
        self.canvas.create_text(465, 40, text="BACKEND E CONTROLO (Linguagem: C++ | API: Boost.Asio e std::thread)", font=("Times New Roman", 12, "bold"), fill="#1A365D")

        x_c1 = 50
        self.desenhar_bloco(x_c1, 80, w_t, h_t, "⚙️ Comando de Navegação\n(boost::asio timer - 80ms)", c_cpp, "#1A365D")
        self.desenhar_bloco(x_c1, 180, w_t, h_t, "⚙️ Controle de Navegação (PID)\n(boost::asio timer - 80ms)", c_cpp, "#1A365D")
        self.desenhar_bloco(x_c1, 280, w_t, h_t, "⚙️ Cálculo de Distância Percorrida\n(std::thread - 20ms)", c_cpp, "#1A365D")
        self.desenhar_bloco(x_c1, 380, w_t, h_t, "⚙️ Reconstrução Superfície Teto\n(std::thread - 100ms)", c_cpp, "#1A365D")
        self.desenhar_bloco(x_c1, 520, w_t, h_t, "📷 Inspeção Detalhada por Câmera\n(Thread em Espera)", c_cpp, "#1A365D")
        self.desenhar_bloco(x_c1, 640, w_t, h_t, "💾 Coletor de Dados\n(Thread em Espera)", c_cpp, "#1A365D")

        # ==========================================
        # 2. CAMADA DE ESTADO (COLUNA CENTRAL)
        # ==========================================
        x_c2 = 550
        # Mutexes/Buffers (Cofres)
        self.desenhar_bloco(x_c2, 80, w_m, 90, "🔒 nav.mtx (NavBuffer)\n- j_sp_velocidade\n- o_aceleracao\n- e_automatico", c_mutex, "#C05621")
        self.desenhar_bloco(x_c2, 200, w_m, 60, "🔒 sensor.mtx_leituras\n(Var. Sensor)\n- velocidade_real_medida", c_mutex, "#C05621")
        self.desenhar_bloco(x_c2, 300, w_m, 60, "🔒 sensor.mtx_camera\n(Flag Câmera)\n- e_inspecao (Flag)", c_mutex, "#C05621")
        self.desenhar_bloco(x_c2, 400, w_m, 60, "🔒 sensor.mtx_fila\n(Fila Dados)\n- fila_medicoes (queue)", c_mutex, "#C05621")
        
        # Condition Variables (Alarmes)
        self.desenhar_sinal(x_c2 + 40, 520, 180, 40, "🔔 cv_camera (Alarme)")
        self.desenhar_sinal(x_c2 + 40, 640, 180, 40, "🔔 cv_coletor (Alarme)")

        # ==========================================
        # 3. CAMADA DE I/O E FRONTEND (DIREITA)
        # ==========================================
        # Agrupamento tracejado do I/O e Python
        self.canvas.create_rectangle(950, 20, 1400, 780, fill="", outline="#744210", width=3, dash=(5, 5))
        self.canvas.create_text(1175, 40, text="FRONTEND E DADOS (Linguagem: Python)", font=("Times New Roman", 12, "bold"), fill="#744210")

        x_c3 = 1000
        # I/O e Frontend (Python)
        self.desenhar_bloco(x_c3, 640, w_t, h_t, "📄 telemetria_inspecao.csv\n(Ficheiro de Log)", c_io, "#276749")
        self.desenhar_bloco(x_c3, 400, w_t, 100, "🖥️ Interface Gráfica (GUI)\nFicheiro: gui_main.py\nAPI: Tkinter / Pandas", c_python, "#975A16", fonte_tamanho=11)

        # ==========================================
        # LIGAÇÕES DETALHADAS (Mapeadas da imagem)
        # ==========================================
        
        # 1. Reconstrução do Teto -> Sincronização
        self.desenhar_seta(x_c1 + w_t, 410, x_c2, 430, "Empilha")
        self.desenhar_seta(x_c1 + w_t, 400, x_c2, 330, "Atualiza flag: e_inspecao", curvatura=True)
        # Notificações (Setas curvadas para não encavalar)
        self.desenhar_seta(x_c1 + w_t, 430, x_c2 + 40, 530, "Notifica buraco detectado", curvatura=True)
        self.desenhar_seta(x_c1 + w_t, 440, x_c2 + 40, 650, "Notifica novo dado", curvatura=True)

        # 2. Comando de Navegação <-> NavBuffer
        self.desenhar_seta(x_c1 + w_t, 110, x_c2, 110, "Escreve: j_sp_velocidade\ne_automatico")
        # O diagrama mostra NavBuffer -> Comando com rótulo "Lê e_inspecao", mas e_inspecao está em outro mutex.
        # A imagem tem um erro de rotulação no bloco NavBuffer. Vou corrigir para apontar para o mutex correto.
        self.desenhar_seta(x_c2, 320, x_c1 + w_t, 130, "Lê: e_inspecao (Gera Slowdown)", curvatura=True)
        
        # 3. Controle de Navegação <-> NavBuffer
        self.desenhar_seta(x_c1 + w_t, 200, x_c2, 140, "Escreve: o_aceleracao", curvatura=True)
        self.desenhar_seta(x_c2, 130, x_c1 + w_t, 210, "Lê: j_sp_velocidade", curvatura=True)

        # 4. Odometria (Cálculo Distância) -> Var. Sensor
        self.desenhar_seta(x_c1 + w_t, 310, x_c2, 230, "Atualiza: velocidade_real")
        
        # 5. Controle <- Var. Sensor (Feedback)
        self.desenhar_seta(x_c2, 230, x_c1 + w_t, 220, "Lê: velocidade_real\n(Feedback PID)")

        # 6. Alarmes acordam as threads reativas
        self.desenhar_seta(x_c2 + 40, 540, x_c1 + w_t, 550, "Acorda a Câmera")
        self.desenhar_seta(x_c2 + 40, 660, x_c1 + w_t, 670, "Acorda o Coletor")

        # 7. Inspeção e Coleta (Acesso aos Buffers)
        self.desenhar_seta(x_c1 + w_t, 550, x_c2, 330, "Verifica flag: e_inspecao", curvatura=True)
        # O rótulo "<Medicao>" não está em image_3, então vou simplificar para "Desempilha"
        self.desenhar_seta(x_c2, 430, x_c1 + w_t, 650, "Desempilha", curvatura=True)

        # 8. Saída de Disco (I/O)
        self.desenhar_seta(x_c1 + w_t, 670, x_c3, 670, "Grava no disco")
        
        # 9. Frontend Python (Destaque para o Pandas lê os dados)
        self.desenhar_seta(x_c3 + 120, 640, x_c3 + 120, 500, "Pandas lê os dados\n(IPC Assíncrona)")

if __name__ == "__main__":
    root = tk.Tk()
    # Tenta abrir a janela maximizada (Windows/Linux)
    try:
        root.state('zoomed')
    except tk.TclError:
        pass # Ignora no Mac, que não suporta 'zoomed'
    app = DiagramaArquiteturaMestre(root)
    root.mainloop()