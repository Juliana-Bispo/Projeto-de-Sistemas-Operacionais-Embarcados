#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <map>

using namespace cv;
using namespace std;

// Detecta cor dominante HSV em uma imagem
string detectarCorPredominante(const Mat& bgr_roi) {
    Mat hsv;
    cvtColor(bgr_roi, hsv, COLOR_BGR2HSV);

    vector<Mat> hsv_channels;
    split(hsv, hsv_channels);  // H, S, V

    if (hsv_channels.size() != 3) {
        return "Desconhecido";
    }

    Ptr<CLAHE> clahe = createCLAHE();
    clahe->setClipLimit(4.0);
    clahe->apply(hsv_channels[2], hsv_channels[2]);  // Equaliza o brilho

    merge(hsv_channels, hsv);

    GaussianBlur(hsv, hsv, Size(5, 5), 0);

    Mat brilhoMask;
    inRange(hsv_channels[2], 40, 255, brilhoMask);

    map<string, int> contador;

    vector<pair<string, Scalar>> coresInferiores = {
        {"Verde", Scalar(35, 50, 40)},
        {"Vermelho1", Scalar(0, 70, 40)},
        {"Vermelho2", Scalar(170, 70, 40)},
        {"Azul", Scalar(90, 40, 40)},
        {"Laranja", Scalar(11, 100, 50)},
        {"Roxo", Scalar(130, 40, 40)}
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
        bitwise_and(mask, brilhoMask, mask);
        int count = countNonZero(mask);
        contador[coresInferiores[i].first] += count;
    }

    contador["Vermelho"] = contador["Vermelho1"] + contador["Vermelho2"];

    string corDominante = "Desconhecido";
    int maxPix = 0;
    for (auto& par : contador) {
        if (par.second > maxPix && par.first != "Vermelho1" && par.first != "Vermelho2") {
            maxPix = par.second;
            corDominante = par.first;
        }
    }

    return corDominante;  // <-- AQUI estava faltando a chave de fechamento
}  // <- ESSA chave fecha a função 'detectarCorPredominante'

// Agora sim é permitido definir outra função fora dela
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
            Rect frameRect(0, 0, frame.cols, frame.rows);
            Rect validROI = tercoInferior & frameRect;
            if (validROI.area() < tercoInferior.area() * 0.8) continue;

            Mat roi = frame(validROI);
            string cor = detectarCorPredominante(roi);
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
