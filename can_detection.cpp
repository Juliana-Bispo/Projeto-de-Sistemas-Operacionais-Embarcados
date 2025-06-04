/*
 * Programa para detecção de latas coloridas usando OpenCV no Raspberry Pi
 * Inclui detecção para: Vermelho, Laranja, Amarelo, Verde, Azul e Roxo
 * Autor: [Seu Nome]
 * Data: [Data]
 */

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;

// Estrutura para armazenar informações das latas detectadas
struct Can {
    Point center;   // Centro (x,y) da lata
    int radius;     // Raio do círculo que envolve a lata
    Scalar color;   // Cor no formato HSV
    string type;    // Tipo da lata (ex: "Lata Laranja")
};

/*
 * Função: detectCans
 * Parâmetros:
 *    frame - Imagem capturada da câmera (Mat)
 * Retorno:
 *    Vetor de latas detectadas (vector<Can>)
 */
vector<Can> detectCans(Mat &frame) {
    vector<Can> cans;
    Mat hsv;
    
    // Converte BGR para HSV (melhor para detecção de cor)
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    
    // Definição das faixas de cores (HSV) para cada lata
    vector<tuple<Scalar, Scalar, string>> colorRanges = {
        // Vermelho (H: 0-10)
        {Scalar(0, 120, 70), Scalar(10, 255, 255), "Lata Vermelha"},
        
        // Laranja (H: 11-19) - Adicionado
        {Scalar(11, 150, 100), Scalar(19, 255, 255), "Lata Laranja"},
        
        // Amarelo (H: 20-30)
        {Scalar(20, 100, 100), Scalar(30, 255, 255), "Lata Amarela"},
        
        // Verde (H: 35-85)
        {Scalar(35, 100, 50), Scalar(85, 255, 255), "Lata Verde"},
        
        // Azul (H: 100-140)
        {Scalar(100, 150, 50), Scalar(140, 255, 255), "Lata Azul"},
        
        // Roxo (H: 141-170) - Adicionado
        {Scalar(141, 50, 50), Scalar(170, 255, 255), "Lata Roxa"}
    };
    
    // Processa cada faixa de cor
    for (const auto &range : colorRanges) {
        Mat mask;
        inRange(hsv, get<0>(range), get<1>(range), mask);
        
        // Operações morfológicas para limpar a máscara
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
        morphologyEx(mask, mask, MORPH_OPEN, kernel);  // Remove ruídos
        morphologyEx(mask, mask, MORPH_CLOSE, kernel); // Fecha buracos
        
        // Encontra contornos na máscara
        vector<vector<Point>> contours;
        findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        
        // Processa cada contorno encontrado
        for (const auto &contour : contours) {
            double area = contourArea(contour);
            if (area < 500) continue; // Filtra contornos pequenos
            
            Point2f center;
            float radius;
            minEnclosingCircle(contour, center, radius);
            
            // Filtra por circularidade (evita falsos positivos)
            double perimeter = arcLength(contour, true);
            double circularity = 4 * CV_PI * area / (perimeter * perimeter);
            if (circularity < 0.7) continue;
            
            // Armazena a lata detectada
            Can can;
            can.center = center;
            can.radius = radius;
            can.color = get<0>(range);
            can.type = get<2>(range);
            cans.push_back(can);
        }
    }
    return cans;
}

/*
 * Função principal
 */
int main() {
    // Inicializa a câmera (0 = câmera padrão)
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir a câmera!" << endl;
        return -1;
    }
    
    // Configura resolução (ajustável conforme necessidade)
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);
    
    Mat frame;
    while (true) {
        cap >> frame; // Captura um frame
        if (frame.empty()) break;
        
        // Detecta latas no frame atual
        vector<Can> cans = detectCans(frame);
        
        // Desenha os resultados
        for (const auto &can : cans) {
            circle(frame, can.center, can.radius, Scalar(0, 255, 0), 2); // Círculo verde
            circle(frame, can.center, 2, Scalar(0, 0, 255), 3);           // Ponto central
            putText(frame, can.type, Point(can.center.x, can.center.y - can.radius - 10),
                   FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1);  // Texto
        }
        
        imshow("Detector de Latas Coloridas", frame);
        if (waitKey(30) == 27) break; // ESC para sair
    }
    
    cap.release();
    destroyAllWindows();
    return 0;
}
