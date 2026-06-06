#!/usr/bin/env python3
import sys
import paho.mqtt.client as mqtt
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QHBoxLayout, 
                             QVBoxLayout, QPushButton, QFrame, QLabel, 
                             QSplitter, QGroupBox, QFormLayout)
from PyQt6.QtCore import Qt, pyqtSignal, QObject, QTimer

# Importando a visualização 2D protegida contra falhas X11
from view_2d import PygameWidget

class SinaisMQTT(QObject):
    dados_recebidos = pyqtSignal(str, str)

class GUIOperacaoRemota(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ATR 2026/1 - GUI Operação Remota (Centro de Controle)")
        self.setGeometry(100, 100, 1100, 750)

        self.sinais = SinaisMQTT()
        self.sinais.dados_recebidos.connect(self.processar_atualizacao_gui)

        self.ultimo_encoder = 0
        self.ultima_velocidade = 0.0
        self.ultimo_lidar = 10.0
        self.ultima_aceleracao = 0.0
        self.status_inspecao = 0

        self.modo_operacao = "0" 
        self.missao_iniciada = False

        self.timer_tela = QTimer()
        self.timer_tela.timeout.connect(self.atualizar_tela)
        self.timer_tela.start(100) 

        self.init_ui()
        self.init_mqtt()

    def init_ui(self):
        widget_central = QWidget()
        self.setCentralWidget(widget_central)
        layout_principal = QHBoxLayout(widget_central)

        # Painel de controle esquerdo
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
        self.lbl_lidar = QLabel("10.00 m")
        self.lbl_aceleracao = QLabel("0.00 m/s²")
        
        layout_estados.addRow("Modo de Operação:", self.lbl_modo)
        layout_estados.addRow("Status Inspeção:", self.lbl_inspecao)
        layout_estados.addRow("Posição (Encoder):", self.lbl_encoder)
        layout_estados.addRow("Velocidade Atual:", self.lbl_velocidade)
        layout_estados.addRow("Distância Teto (LIDAR):", self.lbl_lidar)
        layout_estados.addRow("Aceleração:", self.lbl_aceleracao)
        
        layout_painel.addWidget(grupo_estados)

        grupo_comandos = QGroupBox("Comandos Remotos")
        layout_comandos = QVBoxLayout(grupo_comandos)

        layout_modos = QHBoxLayout()
        self.btn_modo_auto = QPushButton("Automático")
        self.btn_modo_man = QPushButton("Manual")
        self.btn_modo_auto.clicked.connect(lambda: self.publish_comando("modo", "AUTOMATICO"))
        self.btn_modo_man.clicked.connect(lambda: self.publish_comando("modo", "MANUAL"))
        layout_modos.addWidget(self.btn_modo_auto)
        layout_modos.addWidget(self.btn_modo_man)
        layout_comandos.addLayout(layout_modos)

        # Botões de Direção e Movimentação Manual
        layout_dir = QHBoxLayout()
        self.btn_esquerda = QPushButton("◀ Esquerda")
        self.btn_direita = QPushButton("Direita ▶")
        self.btn_esquerda.clicked.connect(lambda: self.publish_direcao("ESQUERDA"))
        self.btn_direita.clicked.connect(lambda: self.publish_direcao("DIREITA"))
        layout_dir.addWidget(self.btn_esquerda)
        layout_dir.addWidget(self.btn_direita)
        layout_comandos.addLayout(layout_dir)

        self.btn_iniciar = QPushButton("Iniciar")
        self.btn_iniciar.setStyleSheet("background-color: green; color: white; font-weight: bold; height: 35px;")
        self.btn_iniciar.setEnabled(False) 
        self.btn_iniciar.clicked.connect(self.publish_iniciar)

        self.btn_continuar = QPushButton("Continuar")
        self.btn_continuar.setStyleSheet("background-color: #f0ad4e; color: black; font-weight: bold; height: 35px;")
        self.btn_continuar.clicked.connect(self.publish_continuar)
        
        self.btn_para = QPushButton("PARAR (Emergência)")
        self.btn_para.setStyleSheet("background-color: #d9534f; color: white; font-weight: bold; height: 35px;")
        self.btn_para.clicked.connect(self.publish_parar)

        self.btn_finalizar = QPushButton("Finalizar Inspeção")
        self.btn_finalizar.setStyleSheet("background-color: #337ab7; color: white; font-weight: bold; height: 35px;")
        self.btn_finalizar.clicked.connect(self.publish_finalizar)

        layout_comandos.addWidget(self.btn_iniciar)
        layout_comandos.addWidget(self.btn_continuar)
        layout_comandos.addWidget(self.btn_para)
        layout_comandos.addWidget(self.btn_finalizar)

        self.btn_esquerda.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_direita.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_para.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_continuar.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_finalizar.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_iniciar.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_modo_auto.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.btn_modo_man.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        
        layout_painel.addWidget(grupo_comandos)
        layout_principal.addWidget(painel_controle)

        # Divisor Lado Direito
        divisor_visualizacao = QSplitter(Qt.Orientation.Vertical)

        # Container da Simulação 2D
        self.container_simulacao = QFrame()
        self.container_simulacao.setFrameShape(QFrame.Shape.StyledPanel)
        self.container_simulacao.setStyleSheet("background-color: #1e1e1e; border: 2px solid #555; border-radius: 6px;")
        
        layout_sim = QVBoxLayout(self.container_simulacao)
        layout_sim.setContentsMargins(2, 2, 2, 2)
        
        self.pygame_simulador = PygameWidget()
        layout_sim.addWidget(self.pygame_simulador)

        # Gráfico estático inferior
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
        divisor_visualizacao.setSizes([420, 280])

        layout_principal.addWidget(divisor_visualizacao)

    def init_mqtt(self):
        self.cliente_mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.cliente_mqtt.on_message = self.ao_receber_mensagem
        
        try:
            self.cliente_mqtt.connect("localhost", 1883, 60)
            self.cliente_mqtt.subscribe("tunel/sensor/encoder")
            self.cliente_mqtt.subscribe("tunel/sensor/lidar")
            self.cliente_mqtt.subscribe("tunel/sensor/velocidade")
            self.cliente_mqtt.subscribe("tunel/cmd/aceleracao")
            self.cliente_mqtt.subscribe("tunel/status/inspecao")
            self.cliente_mqtt.loop_start()
            print("[GUI MQTT] Conectado com sucesso ao Broker Mosquitto.")
        except Exception as e:
            print(f"[ERRO GUI MQTT] Não foi possível conectar ao Broker: {e}")

    def ao_receber_mensagem(self, client, userdata, msg):
        self.sinais.dados_recebidos.emit(msg.topic, msg.payload.decode())

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
        
        self.pygame_simulador.atualizar_dados(
            self.ultimo_encoder, 
            self.ultima_velocidade, 
            self.ultimo_lidar, 
            self.status_inspecao
        )
        
        if not self.missao_iniciada:
            self.lbl_inspecao.setText("Aguardando...")
            self.lbl_inspecao.setStyleSheet("color: gray; font-weight: bold; font-size: 14px;")
        elif self.status_inspecao == 1:
            self.lbl_inspecao.setText("ALARME: FALHA DETECTADA")
            self.lbl_inspecao.setStyleSheet("color: red; font-weight: bold; font-size: 14px;")
        else:
            self.lbl_inspecao.setText("TETO INTEGRAL")
            self.lbl_inspecao.setStyleSheet("color: green; font-weight: bold; font-size: 14px;")

    def publish_iniciar(self):
        self.missao_iniciada = True
        self.btn_iniciar.setText("INICIAR")
        self.publish_mqtt_data("tunel/cmd/modo", self.modo_operacao)
        self.publish_mqtt_data("tunel/cmd/iniciar", "1")
        self.btn_iniciar.setEnabled(False)

    def publish_parar(self):
        print("[GUI COMANDO] EMERGÊNCIA: Parando o robô!")
        self.publish_mqtt_data("tunel/controle/direcao", "PARAR")
        self.lbl_inspecao.setText("PARADA DE EMERGÊNCIA")
        self.lbl_inspecao.setStyleSheet("color: red; font-weight: bold; font-size: 14px;")
        self.setFocus()

    def publish_continuar(self):
        print("[GUI COMANDO] Retomando operação...")
        self.publish_mqtt_data("tunel/controle/direcao", "CONTINUAR")
        self.setFocus()

    def publish_finalizar(self):
        print("[GUI COMANDO] Finalizando inspeção e resetando sistema.")
        # Puxa o freio de mão para cravar o robô
        self.publish_mqtt_data("tunel/controle/direcao", "PARAR")
        
        # Informa a rede inteira (C++ e Física) que a missão acabou
        self.publish_mqtt_data("tunel/sistema/status", "SIMULACAO_CONCLUIDA")
        
        # Restaura o estado da Interface
        self.missao_iniciada = False
        self.btn_iniciar.setText("Iniciar")
        self.btn_iniciar.setEnabled(True)
        self.lbl_inspecao.setText("Inspeção Finalizada")
        self.lbl_inspecao.setStyleSheet("color: blue; font-weight: bold; font-size: 14px;")
        self.setFocus()

    def publish_mqtt_data(self, topic, payload):
        self.cliente_mqtt.publish(topic, payload)

    def publish_comando(self, tipo, valor):
        if valor == "AUTOMATICO":
            self.modo_operacao = "1"
            self.lbl_modo.setText("AUTOMÁTICO")
            self.lbl_modo.setStyleSheet("color: darkorange; font-weight: bold; font-size: 14px;")
            self.btn_esquerda.setEnabled(False)
            self.btn_direita.setEnabled(False)
            self.publish_mqtt_data("tunel/controle/modo", "AUTO")
        else:
            self.modo_operacao = "0"
            self.lbl_modo.setText("MANUAL")
            self.lbl_modo.setStyleSheet("color: blue; font-weight: bold; font-size: 14px;")
            self.btn_esquerda.setEnabled(True)
            self.btn_direita.setEnabled(True)
            self.publish_mqtt_data("tunel/controle/modo", "MANUAL")
            
        self.btn_iniciar.setEnabled(True) 

    def publish_direcao(self, comando):
        print(f"[GUI COMANDO] Enviando ação de movimentação: {comando}")
        self.publish_mqtt_data("tunel/controle/direcao", comando)

    def closeEvent(self, event):
        self.cliente_mqtt.loop_stop()
        self.cliente_mqtt.disconnect()
        self.pygame_simulador.close()
        event.accept()

    def keyPressEvent(self, event):
        if event.isAutoRepeat(): return
        if self.btn_iniciar.isEnabled(): return 
        if self.modo_operacao == "1" and event.key() in (Qt.Key.Key_Right, Qt.Key.Key_Left):
            return

        if event.key() == Qt.Key.Key_Right:
            self.publish_direcao("DIREITA")
            self.btn_direita.setDown(True)
        elif event.key() == Qt.Key.Key_Left:
            self.publish_direcao("ESQUERDA")
            self.btn_esquerda.setDown(True)
        elif event.key() == Qt.Key.Key_Space: 
            self.publish_direcao("PARAR")
            self.btn_para.setDown(True)

    def keyReleaseEvent(self, event):
        if event.isAutoRepeat(): return
        if self.modo_operacao == "1" and event.key() in (Qt.Key.Key_Right, Qt.Key.Key_Left):
            return
        
        if event.key() == Qt.Key.Key_Right:
            self.btn_direita.setDown(False)
            self.publish_direcao("PARAR") 
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