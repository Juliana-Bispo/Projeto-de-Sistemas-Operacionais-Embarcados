#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <map>

using namespace cv;
using namespace std;

// Estrutura para armazenar informações das latinhas
struct SodaCan {
    Point center;
    int radius;
    string brand;  // Nome da marca (ex: "Coca-Cola")
    Scalar color;  // Cor representativa
};

// Função para detectar latinhas por cor e formato
vector<SodaCan> detectSodaCans(Mat &frame) {
    vector<SodaCan> cans;
    Mat hsv, mask;
    
    // Converter para HSV
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    
    // Mapeamento de cores para marcas específicas
    map<string, pair<Scalar, Scalar>> brandColors = {
        {"Coca-Cola",    {Scalar(0, 120, 70),   Scalar(10, 255, 255)}},   // Vermelho
        {"Pepsi",        {Scalar(100, 150, 50), Scalar(140, 255, 255)}},  // Azul
        {"Guaraná",      {Scalar(35, 100, 50),  Scalar(85, 255, 255)}},   // Verde
        {"Fanta Laranja",{Scalar(11, 150, 100), Scalar(19, 255, 255)}},   // Laranja
        {"Fanta Uva",    {Scalar(141, 50, 50),  Scalar(170, 255, 255)}}   // Roxo
    };

    // Processar cada marca
    for (const auto &brand : brandColors) {
        inRange(hsv, brandColors[brand.first].first, 
               brandColors[brand.first].second, mask);
        
        // Operações morfológicas
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
        morphologyEx(mask, mask, MORPH_OPEN, kernel);
        morphologyEx(mask, mask, MORPH_CLOSE, kernel);
        
        // Encontrar contornos
        vector<vector<Point>> contours;
        findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        
        for (const auto &contour : contours) {
            double area = contourArea(contour);
            if (area < 500) continue;
            
            Point2f center;
            float radius;
            minEnclosingCircle(contour, center, radius);
            
            double perimeter = arcLength(contour, true);
            double circularity = 4 * CV_PI * area / (perimeter * perimeter);
            if (circularity < 0.7) continue;
            
            SodaCan can;
            can.center = center;
            can.radius = radius;
            can.brand = brand.first;
            
            // Definir cores para desenho (em BGR)
            if (brand.first == "Coca-Cola")    can.color = Scalar(0, 0, 255);
            else if (brand.first == "Pepsi")   can.color = Scalar(255, 0, 0);
            else if (brand.first == "Guaraná") can.color = Scalar(0, 255, 0);
            else if (brand.first == "Fanta Laranja") can.color = Scalar(0, 165, 255);
            else if (brand.first == "Fanta Uva") can.color = Scalar(128, 0, 128);
            
            cans.push_back(can);
        }
    }
    return cans;
}

int main() {
    VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir a câmera!" << endl;
        return -1;
    }
    
    // Configurações para melhor detecção em fundo preto
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(CAP_PROP_BRIGHTNESS, 40);
    cap.set(CAP_PROP_CONTRAST, 70);
    
    namedWindow("Identificação de Refrigerantes", WINDOW_NORMAL);
    
    Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        
        // Melhorar contraste para fundo preto
        Mat lab;
        cvtColor(frame, lab, COLOR_BGR2Lab);
        vector<Mat> labChannels(3);
        split(lab, labChannels);
        equalizeHist(labChannels[0], labChannels[0]);
        merge(labChannels, lab);
        cvtColor(lab, frame, COLOR_Lab2BGR);
        
        vector<SodaCan> cans = detectSodaCans(frame);
        
        // Desenhar resultados
        for (const auto &can : cans) {
            // Desenhar círculo com a cor da marca
            circle(frame, can.center, can.radius, can.color, 3);
            
            // Desenhar centro
            circle(frame, can.center, 3, Scalar(0, 0, 0), -1);
            
            // Exibir nome da marca
            putText(frame, can.brand, 
                    Point(can.center.x - can.radius, can.center.y - can.radius - 10),
                    FONT_HERSHEY_SIMPLEX, 0.8, can.color, 2);
            
            // Exibir informações adicionais
            string info = "Raio: " + to_string(can.radius);
            putText(frame, info, 
                    Point(can.center.x - can.radius, can.center.y - can.radius + 20),
                    FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1);
        }
        
        imshow("Identificação de Refrigerantes", frame);
        if (waitKey(30) == 27) break;
    }
    
    cap.release();
    destroyAllWindows();
    return 0;
}
