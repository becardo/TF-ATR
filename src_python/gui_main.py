#!/usr/bin/env python3
import sys
import paho.mqtt.client as mqtt
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QHBoxLayout, 
                             QVBoxLayout, QPushButton, QFrame, QLabel, 
                             QSplitter, QSpinBox, QGroupBox, QFormLayout)
from PyQt6.QtCore import Qt, pyqtSignal, QObject, QTimer

# Classe auxiliar para emitir sinais do MQTT de forma segura para a Thread da GUI do PyQt
class SinaisMQTT(QObject):
    dados_recebidos = pyqtSignal(str, str)

class GUIOperacaoRemota(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ATR 2026/1 - GUI Operação Remota (Centro de Controle)")
        self.setGeometry(100, 100, 1100, 750)

        # Mecanismo de comunicação interna de Threads
        self.sinais = SinaisMQTT()
        self.sinais.dados_recebidos.connect(self.processar_atualizacao_gui)

        # Variáveis de Estado Internas (Últimos valores recebidos)
        self.ultimo_encoder = 0
        self.ultima_velocidade = 0.0
        self.ultimo_lidar = 10.0
        self.ultima_aceleracao = 0.0
        self.status_inspecao = 0

        # "0" = Manual | "1" = Automático
        self.modo_operacao = "0" 
        self.missao_iniciada = False

        # Temporizador de atualização da janela 
        self.timer_tela = QTimer()
        self.timer_tela.timeout.connect(self.atualizar_tela)
        self.timer_tela.start(100) # Atualiza a tela a cada 100ms

        # Inicializa Layout e Conexões MQTT
        self.init_ui()
        self.init_mqtt()

    def init_ui(self):
        widget_central = QWidget()
        self.setCentralWidget(widget_central)
        layout_principal = QHBoxLayout(widget_central)

        # Painel de controle
        painel_controle = QWidget()
        painel_controle.setFixedWidth(380)
        layout_painel = QVBoxLayout(painel_controle)
        layout_painel.setAlignment(Qt.AlignmentFlag.AlignTop)

        grupo_estados = QGroupBox("Telemetria do Robô")
        layout_estados = QFormLayout(grupo_estados)
        
        self.lbl_modo = QLabel("Selecionar")
        self.lbl_modo.setStyleSheet("color: blue; font-weight: bold; font-size: 14px;")
        self.lbl_inspecao = QLabel("Aguardando...")
        self.lbl_inspecao.setStyleSheet("color: green; font-weight: bold; font-size: 14px;")
        
        self.lbl_encoder = QLabel("0 m")
        self.lbl_velocidade = QLabel("0.00 m/s")
        self.lbl_telemetria_sp = QLabel("1 m/s")
        self.lbl_lidar = QLabel("10.00 m")
        self.lbl_aceleracao = QLabel("0.00 m/s²")
        
        layout_estados.addRow("Modo de Operação:", self.lbl_modo)
        layout_estados.addRow("Status Inspeção:", self.lbl_inspecao)
        layout_estados.addRow("Posição (Encoder):", self.lbl_encoder)
        layout_estados.addRow("Velocidade SetPoint:", self.lbl_telemetria_sp)
        layout_estados.addRow("Velocidade Atual:", self.lbl_velocidade)
        layout_estados.addRow("Distância Teto (LIDAR):", self.lbl_lidar)
        layout_estados.addRow("Aceleração:", self.lbl_aceleracao)
        
        layout_painel.addWidget(grupo_estados)

        # Comandos Remotos (Envio via MQTT)
        grupo_comandos = QGroupBox("Comandos Remotos")
        layout_comandos = QVBoxLayout(grupo_comandos)

        # Seleção de Modo (Auto / Manual)
        layout_modos = QHBoxLayout()
        self.btn_modo_auto = QPushButton("Automático")
        self.btn_modo_man = QPushButton("Manual")
        self.btn_modo_auto.clicked.connect(lambda: self.publish_comando("modo", "AUTOMATICO"))
        self.btn_modo_man.clicked.connect(lambda: self.publish_comando("modo", "MANUAL"))
        layout_modos.addWidget(self.btn_modo_auto)
        layout_modos.addWidget(self.btn_modo_man)
        layout_comandos.addLayout(layout_modos)

        # Setpoint de Velocidade (Alvo para o PID no C++)
        layout_vel = QHBoxLayout()
        layout_vel.addWidget(QLabel("Sp Velocidade (m/s):"))
        self.sp_velocidade = QSpinBox()
        self.sp_velocidade.setRange(0, 20)
        self.sp_velocidade.setValue(1) 
        self.sp_velocidade.valueChanged.connect(self.atualizar_sp_tela)
        layout_vel.addWidget(self.sp_velocidade)
        layout_comandos.addLayout(layout_vel)

        # Botões de Direção e Movimentação
        layout_dir = QHBoxLayout()
        self.btn_esquerda = QPushButton("◀ Esquerda")
        self.btn_direita = QPushButton("Direita ▶")
        self.btn_esquerda.clicked.connect(lambda: self.publish_direcao("ESQUERDA"))
        self.btn_direita.clicked.connect(lambda: self.publish_direcao("DIREITA"))
        layout_dir.addWidget(self.btn_esquerda)
        layout_dir.addWidget(self.btn_direita)
        layout_comandos.addLayout(layout_dir)

        # Botão Iniciar, Continuar e Parar
        self.btn_iniciar = QPushButton("Iniciar")
        self.btn_iniciar.setStyleSheet("background-color: green; color: white; font-weight: bold; height: 35px;")
        self.btn_iniciar.setEnabled(False) # botão Iniciar começa desabilitado
        self.btn_iniciar.clicked.connect(self.publish_iniciar)

        self.btn_continuar = QPushButton("Continuar")
        self.btn_continuar.clicked.connect(lambda: self.publish_direcao("CONTINUAR"))
        
        self.btn_para = QPushButton("■ PARAR (Emergência)")
        self.btn_para.setStyleSheet("background-color: #d9534f; color: white; font-weight: bold; height: 35px;")
        self.btn_para.clicked.connect(lambda: self.publish_direcao("PARAR"))

        layout_comandos.addWidget(self.btn_iniciar)
        layout_comandos.addWidget(self.btn_continuar)
        layout_comandos.addWidget(self.btn_para)

        # Impede os botões de roubarem as setas do teclado 
        self.btn_esquerda.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_direita.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_para.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_continuar.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_iniciar.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_modo_auto.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_modo_man.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        
        layout_painel.addWidget(grupo_comandos)
        layout_principal.addWidget(painel_controle)

        # Visuzalização Gráfica
        divisor_visualizacao = QSplitter(Qt.Orientation.Vertical)

        # Simulação 2D
        self.container_simulacao = QFrame()
        self.container_simulacao.setFrameShape(QFrame.Shape.StyledPanel)
        self.container_simulacao.setStyleSheet("background-color: #2b2b2b; border: 2px solid #555; border-radius: 6px;")
        
        layout_sim = QVBoxLayout(self.container_simulacao)
        self.lbl_sim_placeholder = QLabel("VISTA LATERAL DO TÚNEL 2D\n[Aqui o Pygame ou QPainter renderizará o movimento do robô]", self.container_simulacao)
        self.lbl_sim_placeholder.setStyleSheet("color: #aaa; font-weight: bold;")
        self.lbl_sim_placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout_sim.addWidget(self.lbl_sim_placeholder)

        # Gráfico LIDAR
        self.container_graficos = QFrame()
        self.container_graficos.setFrameShape(QFrame.Shape.StyledPanel)
        self.container_graficos.setStyleSheet("background-color: #151515; border: 2px solid #333; border-radius: 6px;")
        
        layout_graf = QVBoxLayout(self.container_graficos)
        self.lbl_graf_placeholder = QLabel("GRÁFICO DE PERFIL DO TETO (DADOS DO LIDAR FILTRADOS)\nEixo X: Encoder (m) | Eixo Y: Altura Recalculada (m)", self.container_graficos)
        self.lbl_graf_placeholder.setStyleSheet("color: #00ff00; font-family: monospace;")
        self.lbl_graf_placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout_graf.addWidget(self.lbl_graf_placeholder)

        divisor_visualizacao.addWidget(self.container_simulacao)
        divisor_visualizacao.addWidget(self.container_graficos)
        divisor_visualizacao.setSizes([400, 300])

        layout_principal.addWidget(divisor_visualizacao)

    # Comunicação de Rede - MQTT Cliente
    def init_mqtt(self):
        self.cliente_mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.cliente_mqtt.on_message = self.ao_receber_mensagem
        
        try:
            self.cliente_mqtt.connect("localhost", 1883, 60)
            
            # Inscrição para ouvir o C++
            self.cliente_mqtt.subscribe("tunel/sensor/encoder")
            self.cliente_mqtt.subscribe("tunel/sensor/lidar")
            self.cliente_mqtt.subscribe("tunel/sensor/velocidade")
            self.cliente_mqtt.subscribe("tunel/cmd/aceleracao")
            self.cliente_mqtt.subscribe("tunel/status/inspecao")
            
            self.cliente_mqtt.loop_start()

            print("[GUI MQTT] Conectado com sucesso ao Broker Mosquitto.")

        except Exception as e:
            print(f"[ERRO GUI MQTT] Não foi possível conectar ao Broker: {e}")

    # Callback: recebe e manda pro correio interno
    def ao_receber_mensagem(self, client, userdata, msg):
        topico = msg.topic
        payload = msg.payload.decode()
        self.sinais.dados_recebidos.emit(topico, payload)

    # Atualiza as variáveis de memória conforme o correio entrega os sinais
    def processar_atualizacao_gui(self, topico, payload):
        if topico == "tunel/sensor/encoder":
            self.ultimo_encoder = int(payload)
        elif topico == "tunel/sensor/lidar":
            self.ultimo_lidar = float(payload)
        elif topico == "tunel/sensor/velocidade":
            self.ultima_velocidade = float(payload)
        elif topico == "tunel/cmd/aceleracao":
            self.ultima_aceleracao = float(payload)
        elif topico == "tunel/status/inspecao":
            self.status_inspecao = int(payload)

    def atualizar_tela(self):
        self.lbl_encoder.setText(f"{self.ultimo_encoder} m")
        self.lbl_lidar.setText(f"{self.ultimo_lidar:.2f} m")
        self.lbl_velocidade.setText(f"{self.ultima_velocidade:.2f} m/s")
        self.lbl_aceleracao.setText(f"{self.ultima_aceleracao:.2f} m/s²")
        
        if not self.missao_iniciada:
            self.lbl_inspecao.setText("Aguardando...")
            self.lbl_inspecao.setStyleSheet("color: gray; font-weight: bold; font-size: 14px;")
        elif self.status_inspecao == 1:
            self.lbl_inspecao.setText("ALARME: FALHA DETECTADA")
            self.lbl_inspecao.setStyleSheet("color: red; font-weight: bold; font-size: 14px;")
        else:
            self.lbl_inspecao.setText("TETO INTEGRAL")
            self.lbl_inspecao.setStyleSheet("color: green; font-weight: bold; font-size: 14px;")

    # Quando o usuário escolhe outro SETPOINT de velocidade em modo Manual
    def atualizar_sp_tela(self, valor):
        # Atualiza a tela de telemetria se estiver no modo manual
        if self.modo_operacao == "0": 
            self.lbl_telemetria_sp.setText(f"{valor} m/s")
        # Envia para o C++
        self.publish_mqtt_data("tunel/controle/sp_velocidade", str(valor))

    # Comunicação GUI - Rede
    def publish_iniciar( self):
        self.missao_iniciada = True
        self.btn_iniciar.setText("INICIAR")
        self.publish_mqtt_data("tunel/cmd/modo", self.modo_operacao)
        self.publish_mqtt_data("tunel/cmd/iniciar", "1")
        self.btn_iniciar.setEnabled(False)

    # Envio de Comandos - Publicações
    def publish_mqtt_data(self, topic, payload):
        self.cliente_mqtt.publish(topic, payload)

    def publish_comando(self, tipo, valor):
        if valor == "AUTOMATICO":
            self.modo_operacao = "1"
            self.lbl_modo.setText("AUTOMÁTICO")
            self.lbl_modo.setStyleSheet("color: darkorange; font-weight: bold; font-size: 14px;")
            self.lbl_telemetria_sp.setText("1 m/s")

            # Desabilita o SetPoint de velocidade, e botões de esquerda e direita
            self.sp_velocidade.setEnabled(False)
            self.btn_esquerda.setEnabled(False)
            self.btn_direita.setEnabled(False)

            self.publish_mqtt_data("tunel/controle/modo", "AUTO")
        else:
            self.modo_operacao = "0"
            self.lbl_modo.setText("MANUAL")
            self.lbl_modo.setStyleSheet("color: blue; font-weight: bold; font-size: 14px;")
            self.lbl_telemetria_sp.setText(f"{self.sp_velocidade.value()} m/s")

            # Habilita os botões e o setpoint
            self.sp_velocidade.setEnabled(True)
            self.btn_esquerda.setEnabled(True)
            self.btn_direita.setEnabled(True)

            self.publish_mqtt_data("tunel/controle/modo", "MANUAL")
            
        self.btn_iniciar.setEnabled(True) # habilita o botão Iniciar
        self.publish_mqtt_data("tunel/controle/sp_velocidade", str(self.sp_velocidade.value()))

    def publish_direcao(self, comando):
        print(f"[GUI COMANDO] Enviando ação de movimentação: {comando}")
        self.publish_mqtt_data("tunel/controle/direcao", comando)
        
        # Se for uma emergência (PARAR), zera a caixa de seleção de velocidade
        if comando == "PARAR":
            self.sp_velocidade.setValue(0)
            self.publish_mqtt_data("tunel/controle/sp_velocidade", "0.0")

    # Encerra as portas de rede ao fechar a janela
    def closeEvent(self, event):
        self.cliente_mqtt.loop_stop()
        self.cliente_mqtt.disconnect()
        event.accept()

    # Operação via Teclado
    def keyPressEvent(self, event):
        if event.isAutoRepeat(): return

        # Só permite pilotar se o sistema já tiver sido iniciado
        if self.btn_iniciar.isEnabled(): 
            return # Se o botão ainda está ativado, o robô está em Standby
        
        # Ignora as setas do teclado se estiver no modo automático
        if self.modo_operacao == "1" and event.key() in (Qt.Key.Key_Right, Qt.Key.Key_Left):
            return

        if event.key() == Qt.Key.Key_Right:
            self.publish_direcao("DIREITA")
            self.btn_direita.setDown(True) # Efeito visual de botão afundado na tela
            
        elif event.key() == Qt.Key.Key_Left:
            self.publish_direcao("ESQUERDA")
            self.btn_esquerda.setDown(True)
            
        elif event.key() == Qt.Key.Key_Space: # spacebar = parar
            self.publish_direcao("PARAR")
            self.btn_para.setDown(True)

    def keyReleaseEvent(self, event):
        if event.isAutoRepeat(): return

        # Levanta os botões na tela quando o usuário solta a tecla
        if self.modo_operacao == "1" and event.key() in (Qt.Key.Key_Right, Qt.Key.Key_Left):
            return
        
        if event.key() == Qt.Key.Key_Right:
            self.btn_direita.setDown(False)
            self.publish_direcao("PARAR") # Para o robo quando a tecla deixa de ser pressionada 

        elif event.key() == Qt.Key.Key_Left:
            self.btn_esquerda.setDown(False)
            self.publish_direcao("PARAR")

        elif event.key() == Qt.Key.Key_Space:
            self.btn_para.setDown(False)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = GUIOperacaoRemota()
    window.show()
    sys.exit(app.exec())
