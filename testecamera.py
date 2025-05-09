import cv2
import datetime
import os
import subprocess

# Nome do arquivo com data/hora
timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
filename = f"photo_{timestamp}.jpg"

# Inicializar câmera
cap = cv2.VideoCapture(0)

# Verifica se a câmera abriu corretamente
if not cap.isOpened():
    raise IOError("Erro ao abrir a câmera")

ret, frame = cap.read()

if ret:
    cv2.imwrite(filename, frame)
    print(f"Foto salva como: {filename}")
else:
    print("Erro ao capturar a imagem")

cap.release()

# Enviar para o GitHub
os.system("git add .")
os.system(f'git commit -m "Nova foto: {filename}"')
os.system("git push")
