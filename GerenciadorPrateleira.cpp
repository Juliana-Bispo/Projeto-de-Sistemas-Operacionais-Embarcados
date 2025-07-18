#include "GerenciadorPrateleira.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <vector>

using namespace cv;
using namespace std;
using namespace std::chrono;

// SUAS CONSTANTES DE TELEGRAM AQUI
const string TELEGRAM_BOT_TOKEN = "SEU_TOKEN_AQUI"; // Substitua pelo seu token
const string TELEGRAM_CHAT_ID = "SEU_CHAT_ID_AQUI"; // Substitua pelo seu chat ID

GerenciadorPrateleira::GerenciadorPrateleira() : proximoIdLata(0) {
    prateleiraArea = Rect(20, 40, 280, 180);
    cout << "[INFO] Gerenciador de Prateleira inicializado." << endl;
}

// Função para calcular "Intersection over Union"
double GerenciadorPrateleira::calcularIoU(const Rect& a, const Rect& b) const {
    int xA = max(a.x, b.x);
    int yA = max(a.y, b.y);
    int xB = min(a.x + a.width, b.x + b.width);
    int yB = min(a.y + a.height, b.y + b.height);
    
    double interArea = max(0, xB - xA) * max(0, yB - yA);
    double unionArea = a.area() + b.area() - interArea;
    
    return (unionArea > 0) ? (interArea / unionArea) : 0.0;
}

// LÓGICA DE RASTREAMENTO REFEITA PARA MAIOR ROBUSTEZ
void GerenciadorPrateleira::atualizarDeteccoes(const vector<pair<Rect, string>>& novasDeteccoes) {
    lock_guard<mutex> lock(mtx);

    vector<bool> deteccoesAssociadas(novasDeteccoes.size(), false);

    // Itera sobre as latas que já estamos rastreando
    for (auto& lata : latasRastreadas) {
        double melhorIoU = 0.2; // Limiar mínimo para considerar uma associação
        int melhorIndiceDetec = -1;

        // Procura a melhor detecção nova para associar com esta lata rastreada
        for (int i = 0; i < novasDeteccoes.size(); ++i) {
            if (deteccoesAssociadas[i]) continue; // Pula se a detecção já foi usada

            double iou = calcularIoU(lata.ultimaPosicao, novasDeteccoes[i].first);
            if (iou > melhorIoU) {
                melhorIoU = iou;
                melhorIndiceDetec = i;
            }
        }

        if (melhorIndiceDetec != -1) { // Se encontramos uma boa associação
            // ATUALIZA a lata existente
            lata.ultimaPosicao = novasDeteccoes[melhorIndiceDetec].first;
            lata.ultimaVezVista = steady_clock::now();
            lata.marca = novasDeteccoes[melhorIndiceDetec].second;
            deteccoesAssociadas[melhorIndiceDetec] = true;

            // Se um alerta estava ativo para esta lata, significa que ela foi REPOSTA
            if (lata.alertaEnviado) {
                cout << "[EVENTO] REPOSICAO DETECTADA para a lata ID " << lata.id << " (" << lata.marca << ")" << endl;
                string msg = "✅ ESTOQUE REPOSTO: " + lata.marca + "\n" + formatarMensagemEstoque();
                enviarNotificacaoTelegram(msg);
                lata.alertaEnviado = false; // Reseta o status do alerta
            }
        }
        // Se não encontramos associação, o tempo de 'ultimaVezVista' NÃO é atualizado,
        // e ela se torna candidata a ser considerada "ausente".
    }

    // Adiciona detecções que não foram associadas a nenhuma lata existente como NOVAS latas
    for (int i = 0; i < novasDeteccoes.size(); ++i) {
        if (!deteccoesAssociadas[i]) {
            cout << "[EVENTO] NOVA LATA DETECTADA: " << novasDeteccoes[i].second << endl;
            latasRastreadas.push_back({proximoIdLata++, novasDeteccoes[i].first, novasDeteccoes[i].second, steady_clock::now(), false});
        }
    }
}

// LÓGICA DE VERIFICAÇÃO DE AUSÊNCIA
void GerenciadorPrateleira::verificarAlertasDeFalta() {
    lock_guard<mutex> lock(mtx);
    
    auto agora = steady_clock::now();
    bool houveAlertaNesteCiclo = false;

    for (auto& lata : latasRastreadas) {
        if (lata.alertaEnviado) continue; // Pula se já alertamos sobre esta lata

        auto tempoAusente = duration_cast<seconds>(agora - lata.ultimaVezVista).count();

        // Se a lata não é vista há mais de 30 segundos, dispara o alerta
        if (tempoAusente >= SEGUNDOS_PARA_ALERTA) {
            cout << "[EVENTO] ALERTA DE FALTA para a lata ID " << lata.id << " (" << lata.marca << "), ausente por " << tempoAusente << "s" << endl;
            lata.alertaEnviado = true;
            houveAlertaNesteCiclo = true;
        }
    }

    // Se pelo menos um alerta foi disparado, envia a notificação com o status geral
    if (houveAlertaNesteCiclo) {
        cout << "[NOTIFICACAO] Enviando alerta de falta de estoque..." << endl;
        string msg = "🚨 ALERTA: FALTA DE ESTOQUE!\n" + formatarMensagemEstoque();
        enviarNotificacaoTelegram(msg);
    }

    // Limpa da memória as latas que sumiram há muito tempo (ex: 5 minutos)
    latasRastreadas.erase(remove_if(latasRastreadas.begin(), latasRastreadas.end(),
        [&](const LataRastreada& lata){
            return duration_cast<minutes>(agora - lata.ultimaVezVista).count() >= 5;
        }), latasRastreadas.end());
}

// Formata a mensagem de status do estoque
string GerenciadorPrateleira::formatarMensagemEstoque() const {
    map<string, int> contagem;
    for (const auto& lata : latasRastreadas) {
        // Conta apenas as latas que estão visíveis ou que desapareceram há menos de 30s
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

// ALTERADO: A saída de erro do comando curl agora será exibida no terminal
void GerenciadorPrateleira::enviarNotificacaoTelegram(const string& mensagem) {
    cout << "[TELEGRAM] Tentando enviar: \"" << mensagem << "\"" << endl;
    // Removi o "> /dev/null" para que possamos ver erros do curl, se houver
    string command = "curl -s -X POST https://api.telegram.org/bot" + TELEGRAM_BOT_TOKEN + "/sendMessage -d chat_id=" + TELEGRAM_CHAT_ID + " -d text=\"" + mensagem + "\"";
    int result = system(command.c_str());
    if (result != 0) {
        cout << "[ERRO] O comando para enviar a notificação do Telegram falhou com o código de saída: " << result << endl;
    }
}
