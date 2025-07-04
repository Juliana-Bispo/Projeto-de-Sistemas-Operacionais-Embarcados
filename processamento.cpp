#include "processamento.hpp"

using namespace cv;
using namespace std;

string detectarCorPredominante(const Mat& bgr_roi) {
    if (bgr_roi.empty()) return "Desconhecida";
    
    Mat hsv;
    cvtColor(bgr_roi, hsv, COLOR_BGR2HSV);
    map<string, int> contadorDePixels;

    Mat maskVermelho1, maskVermelho2;
    inRange(hsv, Scalar(0, 100, 100), Scalar(10, 255, 255), maskVermelho1);
    inRange(hsv, Scalar(160, 100, 100), Scalar(179, 255, 255), maskVermelho2);
    contadorDePixels["Vermelho"] = countNonZero(maskVermelho1 | maskVermelho2);

    Mat maskVerde;
    inRange(hsv, Scalar(35, 40, 40), Scalar(85, 255, 255), maskVerde);
    contadorDePixels["Verde"] = countNonZero(maskVerde);

    Mat maskAzul;
    inRange(hsv, Scalar(95, 60, 40), Scalar(130, 255, 255), maskAzul);
    contadorDePixels["Azul"] = countNonZero(maskAzul);
    
    Mat maskLaranja;
    inRange(hsv, Scalar(5, 100, 100), Scalar(25, 255, 255), maskLaranja);
    contadorDePixels["Laranja"] = countNonZero(maskLaranja);

    string corDominante = "Desconhecida";
    int maxPix = 0;
    for (auto const& [cor, contagem] : contadorDePixels) {
        if (contagem > maxPix) {
            maxPix = contagem;
            corDominante = cor;
        }
    }

    if (maxPix < (bgr_roi.total() * 0.10)) {
        return "Desconhecida";
    }

    return corDominante;
}

string classificarMarcaPorCor(const string& cor) {
    if (cor == "Verde") return "Guarana";
    if (cor == "Vermelho") return "Coca-Cola";
    if (cor == "Azul") return "Pepsi";
    if (cor == "Laranja") return "Fanta Laranja";
    return "Desconhecida";
}