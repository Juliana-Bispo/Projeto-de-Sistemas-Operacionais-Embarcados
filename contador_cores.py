import cv2
import numpy as np
import pandas as pd
from picamera import PiCamera
from time import sleep, time
import os
from datetime import datetime

# Inicializa a câmera Pi uma única vez
camera = PiCamera()
camera.resolution = (640, 480)
sleep(2)  # Aguarda estabilização da câmera

# Dicionário com faixas de cores HSV para detectar
color_ranges = {
    'vermelho': [(0, 100, 100), (10, 255, 255)],  
    'verde': [(40, 40, 40), (70, 255, 255)],
    'azul': [(100, 100, 100), (130, 255, 255)],
    'preto': [(0, 0, 0), (180, 255, 50)],
    'laranja': [(10, 100, 100), (25, 255, 255)]
}

# Contadores acumulados por cor
contagem_total = {cor: 0 for cor in color_ranges}

# Tempo de referência para a saída a cada 5 minutos
inicio_loop = time()

try:
    while True:
        # Captura a imagem
        imagem_path = '/home/pi/imagem.jpg'
        camera.capture(imagem_path)

        # Carrega e converte a imagem para HSV
        image = cv2.imread(imagem_path)
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)

        resultados_atuais = []  # Contagem atual por cor

        for cor, (lower, upper) in color_ranges.items():
            # Cria máscara e aplica morfologia
            mask = cv2.inRange(hsv, np.array(lower), np.array(upper))
            kernel = np.ones((5, 5), np.uint8)
            mask = cv2.erode(mask, kernel, iterations=1)
            mask = cv2.dilate(mask, kernel, iterations=1)

            # Encontra e filtra contornos
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            contornos_filtrados = [cnt for cnt in contours if cv2.contourArea(cnt) > 100]

            # Atualiza contagem
            quantidade = len(contornos_filtrados)
            contagem_total[cor] += quantidade
            resultados_atuais.append({'Cor': cor, 'Quantidade': quantidade})

            # Desenha os contornos detectados
            for cnt in contornos_filtrados:
                cv2.drawContours(image, [cnt], -1, (0, 255, 255), 2)

        # Salva imagem com contornos
        cv2.imwrite('/home/pi/imagem_processada.jpg', image)

        # Salva contagem atual como CSV (opcional, sobrescreve)
        df = pd.DataFrame(resultados_atuais)
        df.to_csv('/home/pi/contagem_cores_atual.csv', index=False)

        # A cada 5 minutos, exibe contagem acumulada no terminal
        tempo_agora = time()
        if tempo_agora - inicio_loop >= 300:  # 300 segundos = 5 minutos
            print("\n[Resumo a cada 5 minutos]")
            for cor, total in contagem_total.items():
                print(f"{cor}: {total} bolinhas detectadas")
            print("------")
            inicio_loop = tempo_agora  # reinicia contagem do tempo

        sleep(10)  # Espera 10 segundos antes de capturar nova imagem

except Exception as e:
    # Salva erro no log
    with open('/home/pi/erro_log.txt', 'a') as f:
        f.write(f"{datetime.now()}: {str(e)}\n")
    print("Erro detectado. Veja erro_log.txt para detalhes.")
