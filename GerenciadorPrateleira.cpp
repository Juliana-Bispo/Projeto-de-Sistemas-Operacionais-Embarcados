#include "GerenciadorPrateleira.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <iomanip> // NOVO: Header necessário para a codificação de URL

using namespace cv;
using namespace std;
using namespace std::chrono;

// SUAS CONSTANTES DE TELEGRAM AQUI
const string TELEGRAM_BOT_TOKEN = "SEU_TOKEN_AQUI"; // Substitua pelo seu token
const string TELEGRAM_CHAT_ID = "SEU_CHAT_ID_AQUI"; // Substitua pelo seu chat ID


// NOVO: Função auxiliar para codificar a mensagem para um formato seguro para URL
string url_encode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (char c : value) {
        // Mantém caracteres seguros
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            // Codifica outros caracteres
            escaped << '%' << setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}


GerenciadorPrateleira::GerenciadorPrateleira() : proximoIdLata(0) {
    prateleiraArea = Rect(20, 40, 280, 180);
    cout << "[INFO] Gerenciador de Prateleira inicializado." << endl;
}

double GerenciadorPrateleira::calcularIoU(const Rect& a, const Rect& b) const {
    int xA = max(a.x, b.x);
    int yA = max(a.y, b.y);
    int xB = min(a.x + a.width, b.x + b.width);
    int yB = min(a.y + a.height, b.y + a.height);
    
    double interArea = max(0, xB - xA) * max(0, yB - yA);
    double unionArea = a.area() + b.area() - interArea;
    
    return (unionArea > 0) ? (interArea / unionArea) : 0.0;
}

void GerenciadorPrateleira::atualizarDeteccoes(const vector<pair<Rect, string>>& novasDeteccoes) {
    lock_guard<mutex> lock(mtx);

    vector<bool> deteccoesAssociadas(novasDeteccoes.size(), false);

    for (auto& lata : latasRastreadas) {
        double melhorIoU = 0.2;
        int melhorIndiceDetec = -1;

        for (int i = 0; i < novasDeteccoes.size(); ++i) {
            if (deteccoesAssociadas[i]) continue;

            double iou = calcularIoU(lata.ultimaPosicao, novasDeteccoes[i].first);
            if (iou > melhorIoU) {
                melhorIoU = iou;
                melhorIndiceDetec = i;
            }
        }

        if (melhorIndiceDetec != -1) {
            lata.ultimaPosicao = novasDeteccoes[melhorIndiceDetec].first;
            lata.ultimaVezVista = steady_clock::now();
            lata.marca = novasDeteccoes[melhorIndiceDetec].second;
            deteccoesAssociadas[melhorIndiceDetec] = true;

            if (lata.alertaEnviado) {
                cout << "[EVENTO] REPOSICAO DETECTADA para a lata ID " << lata.id << " (" << lata.marca << ")" << endl;
                string msg = "✅ ESTOQUE REPOSTO: " + lata.marca + "\n" + formatarMensagemEstoque();
                enviarNotificacaoTelegram(msg);
                lata.alertaEnviado = false;
            }
        }
    }

    for (int i = 0; i < novasDeteccoes.size(); ++i) {
        if (!deteccoesAssociadas[i]) {
            cout << "[EVENTO] NOVA LATA DETECTADA: " << novasDeteccoes[i].second << endl;
            latasRastreadas.push_back({proximoIdLata++, novasDeteccoes[i].first, novasDeteccoes[i].second, steady_clock::now(), false});
        }
    }
}

void GerenciadorPrateleira::verificarAlertasDeFalta() {
    lock_guard<mutex> lock(mtx);
    
    auto agora = steady_clock::now();
    bool houveAlertaNesteCiclo = false;

    for (auto& lata : latasRastreadas) {
        if (lata.alertaEnviado) continue;

        auto tempoAusente = duration_cast<seconds>(agora - lata.ultimaVezVista).count();

        if (tempoAusente >= SEGUNDOS_PARA_ALERTA) {
            cout << "[EVENTO] ALERTA DE FALTA para a lata ID " << lata.id << " (" << lata.marca << "), ausente por " << tempoAusente << "s" << endl;
            lata.alertaEnviado = true;
            houveAlertaNesteCiclo = true;
        }
    }

    if (houveAlertaNesteCiclo) {
        cout << "[NOTIFICACAO] Enviando alerta de falta de estoque..." << endl;
        string msg = "🚨 ALERTA: FALTA DE ESTOQUE!\n" + formatarMensagemEstoque();
        enviarNotificacaoTelegram(msg);
    }

    latasRastreadas.erase(remove_if(latasRastreadas.begin(), latasRastreadas.end(),
        [&](const LataRastreada& lata){
            return duration_cast<minutes>(agora - lata.ultimaVezVista).count() >= 5;
        }), latasRastreadas.end());
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

// ALTERADO: A função agora codifica a mensagem antes de enviar
void GerenciadorPrateleira::enviarNotificacaoTelegram(const string& mensagem) {
    cout << "[TELEGRAM] Tentando enviar: \"" << mensagem << "\"" << endl;

    // NOVO: Codifica a mensagem para o formato de URL
    string mensagem_codificada = url_encode(mensagem);

    // ALTERADO: Usa a mensagem codificada no comando curl
    string command = "curl -s -X POST https://api.telegram.org/bot" + TELEGRAM_BOT_TOKEN + "/sendMessage -d chat_id=" + TELEGRAM_CHAT_ID + " -d text=\"" + mensagem_codificada + "\"";
    
    int result = system(command.c_str());
    if (result != 0) {
        cout << "[ERRO] O comando para enviar a notificação do Telegram falhou com o código de saída: " << result << endl;
    } else {
        cout << "[TELEGRAM] Comando de envio executado com sucesso." << endl;
    }
}
