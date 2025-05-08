import cv2
import numpy as np
import time
import signal
import sys
from datetime import datetime

# Define o intervalo de tempo para impressão no terminal (em segundos)
INTERVALO_ATUALIZACAO = 300  # 5 minutos

# Dicionário com faixas de cores HSV para detectar
cores_hsv = {
    'vermelho': [(0, 100, 100), (10, 255, 255)],  
    'verde': [(40, 40, 40), (70, 255, 255)],
    'azul': [(100, 100, 100), (130, 255, 255)],
    'preto': [(0, 0, 0), (180, 255, 50)],
    'laranja': [(10, 100, 100), (25, 255, 255)]
}

# Inicializa a câmera Pi uma única vez
camera = cv2.VideoCapture(0)
camera.set(3, 640)
camera.set(4, 480)

# Captura sinal de interrupção para encerrar o programa corretamente
def finalizar_programa(sig, frame):
    print("\nEncerrando o programa...")
    camera.release()
    cv2.destroyAllWindows()
    sys.exit(0)

signal.signal(signal.SIGINT, finalizar_programa)

ultimo_relatorio = time.time()

print("Sistema de reconhecimento de cores iniciado...")


while True:
    # Captura a imagem
    ret, frame = camera.read()
    if not ret:
        continue

    # Redimensiona, aplica blur e converte para HSV
    frame_resized = cv2.resize(frame, (320, 240))
    blur = cv2.GaussianBlur(frame_resized, (5, 5), 0)
    hsv = cv2.cvtColor(blur, cv2.COLOR_BGR2HSV)

    # Define regiões de interesse (filas)
    regioes = [
        ((60, 50, 120, 100), hsv[50:100, 60:120]),   # Regiao 1
        ((130, 50, 190, 100), hsv[50:100, 130:190]), # Regiao 2
        ((200, 50, 260, 100), hsv[50:100, 200:260])  # Regiao 3
    ]

    resultado_filiera = []
    contagem_cores = {cor: 0 for cor in cores_hsv.keys()}
    
    # Processa cada fileira
    for idx, (coords, roi) in enumerate(regioes):
        media_h = np.median(roi[:, :, 0])  # Usa mediana para maior robustez

        cor_dominante = None
        for cor, (lim_inf, lim_sup) in cores_hsv.items():
            if lim_inf[0] <= media_h <= lim_sup[0]:
                cor_dominante = cor
                contagem_cores[cor] += 1
                break

        if cor_dominante:
            resultado_filiera.append("preenchida")
            # Desenha o retângulo e escreve a cor detectada
            x1, y1, x2, y2 = coords
            cv2.rectangle(frame_resized, (x1, y1), (x2, y2), (0, 255, 0), 2)
            cv2.putText(frame_resized, cor_dominante, (x1, y1 - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)
        else:
            resultado_filiera.append("vazia")
            # Desenha o retângulo vazio
            x1, y1, x2, y2 = coords
            cv2.rectangle(frame_resized, (x1, y1), (x2, y2), (0, 0, 255), 2)
            cv2.putText(frame_resized, "vazio", (x1, y1 - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)

    # Verifica se alguma fileira está vazia e emite notificação
    if "vazia" in resultado_filiera:
        print(f"[Aviso - {datetime.now().strftime('%H:%M:%S')}] Alguma fileira está vazia. Notificando estoquista...")
    else:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Todas as fileiras estão preenchidas.")

    # Exibe contagem de bolinhas a cada INTERVALO_ATUALIZACAO segundos
    tempo_atual = time.time()
    if tempo_atual - ultimo_relatorio >= INTERVALO_ATUALIZACAO:
        print("\nRelatório de contagem de bolinhas:")
        for cor, contagem in contagem_cores.items():
            print(f" - {cor.capitalize()}: {contagem}")
        print("-")
        ultimo_relatorio = tempo_atual

    # Exibe imagem com identificações
    cv2.imshow("Camera", frame_resized)

    # Aguarda por tecla por 1ms (necessário para o OpenCV funcionar corretamente)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Libera recursos
finalizar_programa(None, None)

