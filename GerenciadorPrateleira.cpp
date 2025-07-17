#include "GerenciadorPrateleira.hpp"

using namespace cv;
using namespace std;
// 8197945619:AAE29qftsp2QSaxwWizakv2RMc-s-az4DMo
// 9

GerenciadorPrateleira::GerenciadorPrateleira() {
    prateleiraArea = Rect(20, 40, 280, 180);
    capacidadeTotal = 4;
    
    contagemMarcas["Guarana"] = 0;
    contagemMarcas["Coca-Cola"] = 0;
    contagemMarcas["Pepsi"] = 0;
    contagemMarcas["Fanta Laranja"] = 0;
    contagemMarcas["Desconhecida"] = 0;
}

void GerenciadorPrateleira::atualizarContagem(const map<string, int>& novasDeteccoes) {
    lock_guard<mutex> lock(mtx);
    for (auto& par : contagemMarcas) {
        par.second = 0;
    }
    for (auto const& [marca, contagem] : novasDeteccoes) {
        if (contagemMarcas.count(marca)) {
            contagemMarcas[marca] = contagem;
        }
    }
}

Mat GerenciadorPrateleira::criarJanelaEstoque() const {
    lock_guard<mutex> lock(mtx);
    Mat janela = Mat::zeros(300, 400, CV_8UC3);
    janela.setTo(Scalar(50, 50, 50));
    putText(janela, "ESTOQUE NA PRATELEIRA", Point(40, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
    
    int y = 70;
    int totalLatas = 0;
    for (auto const& [marca, contagem] : contagemMarcas) {
        if (marca != "Desconhecida" && contagem > 0) {
             putText(janela, marca + ":", Point(20, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);
             putText(janela, to_string(contagem), Point(200, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 255), 2);
             y += 30;
             totalLatas += contagem;
        }
    }
    
    y += 20;
    string statusGeral = "Total: " + to_string(totalLatas) + "/" + to_string(capacidadeTotal);
    Scalar corStatus = (totalLatas >= capacidadeTotal) ? Scalar(0, 255, 0) : Scalar(255, 255, 0);
    putText(janela, statusGeral, Point(20, y), FONT_HERSHEY_SIMPLEX, 0.7, corStatus, 2);

    // Usa .at() para evitar erro se a chave não existir, embora já tenhamos inicializado
    if(contagemMarcas.at("Desconhecida") > 0) {
        y += 30;
        putText(janela, "Latas nao identificadas: " + to_string(contagemMarcas.at("Desconhecida")), Point(20, y), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,255), 1);
    }

    return janela;
}

const Rect& GerenciadorPrateleira::getAreaPrateleira() const {
    return prateleiraArea;
}
