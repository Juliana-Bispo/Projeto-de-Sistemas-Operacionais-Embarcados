#include <opencv2/opencv.hpp>
#include <iostream>
#include <map>

using namespace cv;
using namespace std;

// Detecta cor dominante HSV em uma imagem
string detectarCorPredominante(const Mat& hsv) {
    map<string, int> contador;

    // Máscaras de cor
    vector<pair<string, Scalar>> coresInferiores = {
        {"Verde", Scalar(35, 100, 50)},
        {"Vermelho1", Scalar(0, 120, 70)},
        {"Vermelho2", Scalar(170, 120, 70)},
        {"Azul", Scalar(90, 50, 70)},
        {"Laranja", Scalar(11, 150, 100)},
        {"Roxo", Scalar(130, 50, 70)}
    };

    vector<pair<string, Scalar>> coresSuperiores = {
        {"Verde", Scalar(85, 255, 255)},
        {"Vermelho1", Scalar(10, 255, 255)},
        {"Vermelho2", Scalar(180, 255, 255)},
        {"Azul", Scalar(130, 255, 255)},
        {"Laranja", Scalar(25, 255, 255)},
        {"Roxo", Scalar(160, 255, 255)}
    };

    for (size_t i = 0; i < coresInferiores.size(); ++i) {
        Mat mask;
        inRange(hsv, coresInferiores[i].second, coresSuperiores[i].second, mask);
        int count = countNonZero(mask);
        contador[coresInferiores[i].first] += count;
    }

    // Agrupar vermelho 1 e 2
    contador["Vermelho"] = contador["Vermelho1"] + contador["Vermelho2"];

    // Determina cor com maior contagem
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

// Classifica marca com base na cor
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

    namedWindow("Lata Detection", WINDOW_NORMAL);

    Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        Mat gray, edges;
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, gray, Size(5, 5), 0);
        Canny(gray, edges, 50, 150);

        vector<vector<Point>> contornos;
        findContours(edges, contornos, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        for (const auto& contorno : contornos) {
            Rect bbox = boundingRect(contorno);

            // Filtro por proporção e área
            if (bbox.height < 100 || bbox.width < 30) continue;
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.0) continue;

            // Pega o terço inferior
            Rect tercoInferior(bbox.x, bbox.y + (2 * bbox.height / 3), bbox.width, bbox.height / 3);
            if ((tercoInferior & Rect(0, 0, frame.cols, frame.rows)) != tercoInferior) continue;

            Mat roi = frame(tercoInferior);
            Mat hsv;
            cvtColor(roi, hsv, COLOR_BGR2HSV);

            string cor = detectarCorPredominante(hsv);
            string marca = classificarMarcaPorCor(cor);

            rectangle(frame, bbox, Scalar(0, 255, 0), 2);
            putText(frame, marca, Point(bbox.x, bbox.y - 10), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);

            cout << "Marca detectada: " << marca << endl;
        }

        imshow("Lata Detection", frame);
        if (waitKey(30) == 27) break;
    }

    cap.release();
    destroyAllWindows();
    return 0;
}
