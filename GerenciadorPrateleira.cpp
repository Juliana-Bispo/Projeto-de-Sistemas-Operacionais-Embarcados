#include "GerenciadorPrateleira.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <vector>

using namespace cv;
using namespace std;
using namespace std::chrono;

const string TELEGRAM_BOT_TOKEN = "8032466567:AAHzaRC_RHM7peoVXQBwYdSv6MG57MkOZtA";
const string TELEGRAM_CHAT_ID = "996099722";

// NOVO: Função auxiliar para codificar a mensagem para um formato seguro para URL
string url_encode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;
    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << setw(2) << int((unsigned char)c);
        }
    }
    return escaped.str();
}

GerenciadorPrateleira::GerenciadorPrateleira() : proximoIdLata(0) {
    prateleiraArea = Rect(20, 40, 280, 180);
}

// NOVO: Função para calcular "Intersection over Union", para associar detecções
double GerenciadorPrateleira::calcularIoU(const Rect& a, const Rect& b) const {
    int xA = max(a.x, b.x);
    int yA = max(a.y, b.y);
    int xB = min(a.x + a.width, b.x + b.width);
    int yB = min(a.y + a.height, b.y + b.height);
    
    double interArea = max(0, xB - xA) * max(0, yB - yA);
    double unionArea = a.area() + b.area() - interArea;
    
    return (unionArea > 0) ? (interArea / unionArea) : 0.0;
}

// ALTERADO: Lógica principal refeita para rastreamento de objetos
void GerenciadorPrateleira::atualizarDeteccoes(const vector<pair<Rect, string>>& novasDeteccoes) {
    lock_guard<mutex> lock(mtx);
    vector<bool> deteccoesAssociadas(novasDeteccoes.size(), false);

    for (auto& lata : latasRastreadas) {
        double melhorIoU = 0.2; // Limiar mínimo para considerar uma associação
        int melhorIndiceDetec = -1;
        for (int i = 0; i < novasDeteccoes.size(); ++i) {
            if (deteccoesAssociadas[i]) continue;
            double iou = calcularIoU(lata.ultimaPosicao, novasDeteccoes[i].first);
            if (iou > melhorIoU) {
                melhorIoU = iou;
                melhorIndiceDetec = i;
            }
        }

        if (melhorIndiceDetec != -1) { // Associação encontrada
            lata.ultimaPosicao = novasDeteccoes[melhorIndiceDetec].first;
            lata.ultimaVezVista = steady_clock::now();
            lata.marca = novasDeteccoes[melhorIndiceDetec].second;
            deteccoesAssociadas[melhorIndiceDetec] = true;

            if (lata.alertaEnviado) { // Se estava em falta, foi reposta
                cout << "REPOSICAO DETECTADA: " << lata.marca << endl;
                string msg = "✅ ESTOQUE REPOSTO: " + lata.marca + "\n" + formatarMensagemEstoque();
                enviarNotificacaoTelegram(msg);
                lata.alertaEnviado = false;
            }
        }
    }

    for (int i = 0; i < novasDeteccoes.size(); ++i) { // Adiciona latas não associadas como novas
        if (!deteccoesAssociadas[i]) {
            cout << "NOVA LATA DETECTADA: " << novasDeteccoes[i].second << endl;
            latasRastreadas.push_back({proximoIdLata++, novasDeteccoes[i].first, novasDeteccoes[i].second, steady_clock::now(), false});
        }
    }
}

// NOVO: Lógica de verificação de ausência baseada no tempo
void GerenciadorPrateleira::verificarAlertasDeFalta() {
    lock_guard<mutex> lock(mtx);
    auto agora = steady_clock::now();
    bool houveAlertaNesteCiclo = false;

    for (auto& lata : latasRastreadas) {
        if (lata.alertaEnviado) continue;
        auto tempoAusente = duration_cast<seconds>(agora - lata.ultimaVezVista).count();

        if (tempoAusente >= SEGUNDOS_PARA_ALERTA) {
            cout << "ALERTA DE FALTA CONFIRMADA PARA: " << lata.marca << endl;
            lata.alertaEnviado = true;
            houveAlertaNesteCiclo = true;
        }
    }

    if (houveAlertaNesteCiclo) {
        string msg = "🚨 ALERTA: FALTA DE ESTOQUE!\n" + formatarMensagemEstoque();
        enviarNotificacaoTelegram(msg);
    }
}

string GerenciadorPrateleira::formatarMensagemEstoque() const {
    map<string, int> contagem;
    for (const auto& lata : latasRastreadas) {
        if(duration_cast<seconds>(steady_clock::now() - lata.ultimaVezVista).count() < SEGUNDOS_PARA_ALERTA) {
            if (lata.marca != "Desconhecida") {
                 contagem[lata.marca]++;
            }
        }
    }
    stringstream ss;
    ss << "Situação atual do estoque:\n";
    const vector<string> ordemMarcas = {"Guarana", "Coca-Cola", "Pepsi", "Fanta Laranja"};
    for(const auto& marca : ordemMarcas){
        int qtd = contagem.count(marca) ? contagem.at(marca) : 0;
        ss << "- " << marca << ": " << qtd << "\n";
    }
    return ss.str();
}

const cv::Rect& GerenciadorPrateleira::getAreaPrateleira() const { return prateleiraArea; }

cv::Mat GerenciadorPrateleira::criarJanelaEstoque() const {
    Mat estoqueImg(200, 300, CV_8UC3, Scalar(70, 70, 70));
    putText(estoqueImg, "Controle de Estoque", Point(10, 20), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255));
    
    string status = formatarMensagemEstoque();
    int yPos = 40;
    string linha;
    stringstream ss(status);
    while(getline(ss, linha, '\n')){
        putText(estoqueImg, linha, Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255));
        yPos += 20;
    }
    return estoqueImg;
}

void GerenciadorPrateleira::enviarNotificacaoTelegram(const string& mensagem) {
    string mensagem_codificada = url_encode(mensagem);
    string command = "curl -s -X POST https://api.telegram.org/bot" + TELEGRAM_BOT_TOKEN + "/sendMessage -d chat_id=" + TELEGRAM_CHAT_ID + " -d text=\"" + mensagem_codificada + "\" &";
    system(command.c_str());
}
