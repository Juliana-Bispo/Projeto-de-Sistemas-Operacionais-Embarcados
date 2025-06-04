#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;

// Estrutura para armazenar informações das cores detectadas
struct ColorArea {
    Point center;
    int radius;
    Scalar color;
    string colorName;
};

// Função para detectar áreas coloridas na folha
vector<ColorArea> detectColors(Mat &frame) {
    vector<ColorArea> colors;
    
    // Converte para HSV (melhor para detecção de cor)
    Mat hsv;
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    
    // Define os intervalos de cores para teste
    vector<tuple<Scalar, Scalar, string>> colorRanges = {
        // Vermelho (dois intervalos porque o vermelho "passa" pelo 0 no HSV)
        {Scalar(0, 120, 70), Scalar(10, 255, 255), "Vermelho"},
        {Scalar(170, 120, 70), Scalar(180, 255, 255), "Vermelho"},
        
        // Laranja (entre vermelho e amarelo)
        {Scalar(11, 150, 100), Scalar(19, 255, 255), "Laranja"},
        
        // Verde
        {Scalar(35, 100, 50), Scalar(85, 255, 255), "Verde"},
        
        // Preto (valor V muito baixo)
        {Scalar(0, 0, 0), Scalar(180, 255, 30), "Preto"}
    };
    
    // Processa cada faixa de cor
    for (const auto &range : colorRanges) {
        Mat mask;
        inRange(hsv, get<0>(range), get<1>(range), mask);
        
        // Operações morfológicas para limpeza
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
        morphologyEx(mask, mask, MORPH_OPEN, kernel);
        morphologyEx(mask, mask, MORPH_CLOSE, kernel);
        
        // Encontra contornos
        vector<vector<Point>> contours;
        findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        
        // Processa cada contorno
        for (const auto &contour : contours) {
            double area = contourArea(contour);
            if (area < 300) continue; // Área mínima ajustável
            
            // Encontra o círculo envolvente
            Point2f center;
            float radius;
            minEnclosingCircle(contour, center, radius);
            
            // Filtro de circularidade (opcional para áreas coloridas)
            double perimeter = arcLength(contour, true);
            double circularity = 4 * CV_PI * area / (perimeter * perimeter);
            if (circularity < 0.5) continue;
            
            // Armazena a área colorida
            ColorArea colorArea;
            colorArea.center = center;
            colorArea.radius = radius;
            colorArea.color = get<0>(range);
            colorArea.colorName = get<2>(range);
            colors.push_back(colorArea);
        }
    }
    
    return colors;
}

// Função principal
int main() {
    // Inicializa a câmera
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir a câmera!" << endl;
        return -1;
    }
    
    // Configura resolução
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);
    
    // Cria janela para ajustes
    namedWindow("Teste de Cores", WINDOW_NORMAL);
    
    Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        
        // Detecta cores
        vector<ColorArea> colors = detectColors(frame);
        
        // Desenha resultados
        for (const auto &color : colors) {
            // Desenha contorno
            circle(frame, color.center, color.radius, Scalar(0, 255, 255), 2);
            
            // Desenha nome da cor
            putText(frame, color.colorName, 
                    Point(color.center.x - 50, color.center.y),
                    FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
            
            // Exibe informações no console
            cout << "Cor detectada: " << color.colorName 
                 << " em (" << color.center.x << "," << color.center.y << ")" << endl;
        }
        
        // Mostra resultado
        imshow("Teste de Cores", frame);
        
        // Sai com ESC
        if (waitKey(30) == 27) break;
    }
    
    cap.release();
    destroyAllWindows();
    return 0;
}
