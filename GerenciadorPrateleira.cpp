#include "GerenciadorPrateleira.hpp"
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <sstream>
#include <ctime>
#include <iomanip>

using namespace cv;
using namespace std;
using namespace std::chrono;

// Configuração do Telegram
const string TELEGRAM_BOT_TOKEN = "8032466567:AAHzaRC_RHM7peoVXQBwYdSv6MG57MkOZtA";
const string TELEGRAM_CHAT_ID = "996099722";

GerenciadorPrateleira::GerenciadorPrateleira() : intervaloAtualizacao(30) {
    prateleiraArea = Rect(20, 40, 280, 180);
    capacidadeTotal = 4;
    
    // Inicializa contagem e status de notificação
    const vector<string> marcas = {"Guarana", "Coca-Cola", "Pepsi", "Fanta Laranja", "Desconhecida"};
    for (const auto& marca : marcas) {
        contagemMarcas[marca] = 0;
        notificacaoEnviada[marca] = false;
    }
    
    // Inicializa o tempo da última atualização
    ultimaAtualizacao = system_clock::now();
}

void GerenciadorPrateleira::atualizarContagem(const map<string, int>& novasDeteccoes) {
    lock_guard<mutex> lock(mtx);
    
    // Cópia do estado anterior para comparação
    map<string, int> contagemAnterior = contagemMarcas;

    // Atualiza contagem
    for (auto& par : contagemMarcas) {
        par.second = 0;
    }
    for (auto const& [marca, contagem] : novasDeteccoes) {
        if (contagemMarcas.count(marca)) {
            contagemMarcas[marca] = contagem;
        }
    }

    // Lógica de notificação
    for (auto const& [marca, contagemAtual] : contagemMarcas) {
        if (marca == "Desconhecida") continue;

        int contagemAnt = contagemAnterior[marca];

        if (contagemAtual == 0 && contagemAnt > 0 && !notificacaoEnviada[marca]) {
            string mensagem = "⚠️ ALERTA: Estoque de " + marca + " está em falta!";
            cout << "ENVIANDO NOTIFICACAO: " << mensagem << endl;
            enviarNotificacaoTelegram(mensagem);
            notificacaoEnviada[marca] = true;
        } 
        else if (contagemAtual > 0 && notificacaoEnviada[marca]) {
            cout << "RESETANDO STATUS DE NOTIFICACAO PARA: " << marca << endl;
            notificacaoEnviada[marca] = false;
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

    if(contagemMarcas.at("Desconhecida") > 0) {
        y += 30;
        putText(janela, "Latas nao identificadas: " + to_string(contagemMarcas.at("Desconhecida")), 
               Point(20, y), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,255), 1);
    }

    return janela;
}

const Rect& GerenciadorPrateleira::getAreaPrateleira() const {
    return prateleiraArea;
}

void GerenciadorPrateleira::verificarTempoNotificacao() {
    auto agora = system_clock::now();
    lock_guard<mutex> lock(mtx);
    
    if (agora - ultimaAtualizacao >= intervaloAtualizacao) {
        enviarAtualizacaoCompleta();
        ultimaAtualizacao = agora;
    }
}

void GerenciadorPrateleira::enviarAtualizacaoCompleta() {
    stringstream mensagem;
    
    // Obtém a hora atual formatada
    auto now = system_clock::to_time_t(system_clock::now());
    mensagem << put_time(localtime(&now), "%H:%M:%S") << " - 📊 ESTOQUE ATUALIZADO\n";
    mensagem << "------------------------------\n";
    
    int totalLatas = 0;
    bool temEstoque = false;
    
    for (const auto& [marca, contagem] : contagemMarcas) {
        if (marca != "Desconhecida") {
            mensagem << "➡️ " << marca << ": " << contagem << "\n";
            totalLatas += contagem;
            if (contagem > 0) temEstoque = true;
        }
    }
    
    mensagem << "------------------------------\n";
    mensagem << "🔄 Total: " << totalLatas << "/" << capacidadeTotal << "\n";
    
    if (!temEstoque) {
        mensagem << "\n🚨 PRATELEIRA VAZIA!\n";
    }
    
    if (contagemMarcas.at("Desconhecida") > 0) {
        mensagem << "\n⚠️ ATENÇÃO: " << contagemMarcas.at("Desconhecida") 
                << " lata(s) não identificada(s)\n";
    }
    
    enviarNotificacaoTelegram(mensagem.str());
}

void GerenciadorPrateleira::enviarNotificacaoTelegram(const string& mensagem) {
    string comando = "curl -s -X POST https://api.telegram.org/bot" + TELEGRAM_BOT_TOKEN +
                     "/sendMessage -d chat_id=" + TELEGRAM_CHAT_ID +
                     " -d text=\"" + mensagem + "\"";
    comando += " &"; // Executa em background
    system(comando.c_str());
}
