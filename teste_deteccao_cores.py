import cv2
import numpy as np

def cor_dominante_h(roi_hsv):
    """Extrai o canal H e retorna o valor mais frequente (cor dominante)"""
    h, s, v = cv2.split(roi_hsv)
    hist = cv2.calcHist([h], [0], None, [180], [0, 180])
    return np.argmax(hist)

# Definição das faixas de Hue para cada refrigerante
cores_refri = {
    'Coca-Cola': [(0, 10), (170, 179)],    # Vermelho (duas faixas)
    'Sukita': [(11, 25)],                  # Laranja
    'Guaraná': [(35, 85)],                # Verde
    'Pepsi': [(100, 150)],                # Azul
    'Fanta Uva': [(135, 160)]             # Roxo
}

# Carregamento da imagem
img = cv2.imread('/home/julio-cesar/Documents/Semestres/2025_1/SOE/Projeto/PC2/imagem_real_refri.jpg')
img = cv2.resize(img, (800, 600))
output = img.copy()

# Pré-processamento para detecção de contornos
gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
blur = cv2.GaussianBlur(gray, (5, 5), 0)
edges = cv2.Canny(blur, 50, 150)

# Detecção de contornos
contornos, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

for i, cnt in enumerate(contornos):
    x, y, w, h = cv2.boundingRect(cnt)
    area = cv2.contourArea(cnt)
    if area > 1000 and h > 50 and w > 50:
        roi = img[y:y+h, x:x+w]
        hsv_roi = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
        hue = cor_dominante_h(hsv_roi)

        identificado = False
        for nome, faixas in cores_refri.items():
            for (hmin, hmax) in faixas:
                if hmin <= hue <= hmax:
                    cv2.rectangle(output, (x, y), (x+w, y+h), (0, 255, 0), 2)
                    cv2.putText(output, f"{nome}", (x, y-10),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
                    print(f"Lata na posição '{nome}' identificada como: {nome} (Hue: {hue})")
                    identificado = True
                    break
            if identificado:
                break
        if not identificado:
            cv2.rectangle(output, (x, y), (x+w, y+h), (0, 0, 255), 2)
            cv2.putText(output, "Desconhecido", (x, y-10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
            print(f"Lata na posição '{i}' com Hue {hue} não foi classificada.")

# Exibição da imagem com resultados
cv2.imshow("Deteccao de Latas", output)
cv2.waitKey(0)
cv2.destroyAllWindows()
