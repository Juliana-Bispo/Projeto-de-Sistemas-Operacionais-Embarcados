#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <map>

using namespace cv;
using namespace std;

// Função que detecta a cor dominante em uma região de interesse usando HSV
string detectarCorPredominante(const Mat& bgr_roi) {
    Mat hsv;
    // Converte BGR para HSV (melhor para detecção de cores)
    cvtColor(bgr_roi, hsv, COLOR_BGR2HSV);

    vector<Mat> hsv_channels;
    split(hsv, hsv_channels);  // Separa canais: H (matiz), S (saturação), V (brilho)

    if (hsv_channels.size() != 3) {
        return "Desconhecido";
    }

    // Aplica equalização adaptativa para melhorar contraste no canal de brilho
    Ptr<CLAHE> clahe = createCLAHE();
    clahe->setClipLimit(4.0);
    clahe->apply(hsv_channels[2], hsv_channels[2]);  // Equaliza o canal V (brilho)

    merge(hsv_channels, hsv);

     // Aplica filtro Gaussiano para reduzir ruído
    GaussianBlur(hsv, hsv, Size(5, 5), 0);

     // Cria máscara para filtrar pixels muito escuros (brilho entre 40-255)
    Mat brilhoMask;
    inRange(hsv_channels[2], 40, 255, brilhoMask);

    // Contador para armazenar quantidade de pixels de cada cor
    map<string, int> contador;

    // Define limites inferiores HSV para cada cor
    vector<pair<string, Scalar>> coresInferiores = {
        {"Verde", Scalar(35, 50, 40)},
        {"Vermelho1", Scalar(0, 70, 40)},
        {"Vermelho2", Scalar(170, 70, 40)},
        {"Azul", Scalar(90, 40, 40)},
        {"Laranja", Scalar(11, 100, 50)},
        {"Roxo", Scalar(130, 40, 40)}
    };

    // Define limites superiores HSV para cada cor
    vector<pair<string, Scalar>> coresSuperiores = {
        {"Verde", Scalar(85, 255, 255)},
        {"Vermelho1", Scalar(10, 255, 255)},
        {"Vermelho2", Scalar(180, 255, 255)},
        {"Azul", Scalar(130, 255, 255)},
        {"Laranja", Scalar(25, 255, 255)},
        {"Roxo", Scalar(160, 255, 255)}
    };

    // Para cada cor, conta pixels que estão dentro dos limites HSV
    for (size_t i = 0; i < coresInferiores.size(); ++i) {
        Mat mask;
        inRange(hsv, coresInferiores[i].second, coresSuperiores[i].second, mask);
        bitwise_and(mask, brilhoMask, mask);  // Aplica filtro de brilho
        int count = countNonZero(mask);
        contador[coresInferiores[i].first] += count;
    }

    // Combina as duas partes do vermelho (devido ao wrap-around no HSV)
    contador["Vermelho"] = contador["Vermelho1"] + contador["Vermelho2"];

    // Encontra a cor com maior número de pixels
    string corDominante = "Desconhecido";
    int maxPix = 0;
    for (auto& par : contador) {
        if (par.second > maxPix && par.first != "Vermelho1" && par.first != "Vermelho2") {
            maxPix = par.second;
            corDominante = par.first;
        }
    }

    return corDominante;  
}  

// Função que mapeia cor detectada para marca de refrigerante
string classificarMarcaPorCor(const string& cor) {
    if (cor == "Verde") return "Guaraná";
    if (cor == "Vermelho") return "Coca-Cola";
    if (cor == "Azul") return "Pepsi";
    if (cor == "Laranja") return "Fanta Laranja";
    if (cor == "Roxo") return "Fanta Uva";
    return "Desconhecida";
}


int main() {
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir câmera" << endl;
        return -1;
    }

    // Cria janela para exibir resultado
    namedWindow("Lata Detection", WINDOW_NORMAL);

    Mat frame;
    while (true) {
        cap >> frame; // Captura frame da câmera
        if (frame.empty()) break;

        // Prepara imagem para detecção de bordas
        Mat gray, edges;
        cvtColor(frame, gray, COLOR_BGR2GRAY); // Converte para escala de cinza
        GaussianBlur(gray, gray, Size(5, 5), 0); //Remove
        Canny(gray, edges, 50, 150); //Detecta bordas usando algoritmo Canny

        // Encontra contornos na imagem de bordas
        vector<vector<Point>> contornos;
        findContours(edges, contornos, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        // Analisa cada contorno encontrado
        for (const auto& contorno : contornos) {
            Rect bbox = boundingRect(contorno);

            // Filtros para identificar objetos similares a latas
            if (bbox.height < 100 || bbox.width < 30) continue; // Tamanho mínimo
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.0) continue; //Proporção altura/largura

            // Define a região do terço inferior da lata
            Rect tercoInferior(bbox.x, bbox.y + (2 * bbox.height / 3), bbox.width, bbox.height / 3);
            Rect frameRect(0, 0, frame.cols, frame.rows);
            Rect validROI = tercoInferior & frameRect; //Garante que está dentro da imagem
            if (validROI.area() < tercoInferior.area() * 0.8) continue;

            // Extrai região de interesse e detecta cor dominante
            Mat roi = frame(validROI);
            string cor = detectarCorPredominante(roi);
            string marca = classificarMarcaPorCor(cor);

            // Desenha retângulo ao redor da lata detectada
            rectangle(frame, bbox, Scalar(0, 255, 0), 2);
            // Exibe nome da marca detectada
            putText(frame, marca, Point(bbox.x, bbox.y - 10), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);

            cout << "Marca detectada: " << marca << endl;
        }

        // Exibe frame com detecções
        imshow("Lata Detection", frame);
        if (waitKey(30) == 27) break;
    }

    // Libera os recursos
    cap.release();
    destroyAllWindows();
    return 0;
}
