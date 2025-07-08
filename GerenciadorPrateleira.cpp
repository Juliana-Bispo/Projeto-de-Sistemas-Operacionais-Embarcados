#include "GerenciadorPrateleira.hpp"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <iomanip>

using namespace cv;
using namespace std;

// --- CONFIGURAÇÃO DO TELEGRAM ---
const string TELEGRAM_BOT_TOKEN = "8032466567:AAHzaRC_RHM7peoVXQBwYdSv6MG57MkOZtA";
const string TELEGRAM_CHAT_ID = "996099722";

GerenciadorPrateleira::GerenciadorPrateleira() 
    : executandoEnvioPeriodico(false), intervaloEnvio(30) {
    
    prateleiraArea = Rect(20, 40, 280, 180);
    capacidadeTotal = 4;
    
    // Inicializa a contagem e o status de notificação para cada marca
    const vector<string> marcas = {"Guarana", "Coca-Cola", "Pepsi", "Fanta Laranja", "Desconhecida"};
    for (const auto& marca : marcas) {
        contagemMarcas[marca] = 0;
        notificacaoEnviada[marca] = false;
    }
}

GerenciadorPrateleira::~GerenciadorPrateleira() {
    pararEnvioPeriodico();
}

void GerenciadorPrateleira::atualizarContagem(const map<string, int>& novasDeteccoes) {
    lock_guard<mutex> lock(mtx);
    
    // Cria uma cópia do estado anterior para comparação
    map<string, int> contagemAnterior = contagemMarcas;

    // Reseta a contagem atual para preencher com as novas detecções
    for (auto& par : contagemMarcas) {
        par.second = 0;
    }
    for (auto const& [marca, contagem] : novasDeteccoes) {
        if (contagemMarcas.count(marca)) {
            contagemMarcas[marca] = contagem;
        }
    }

    // --- LÓGICA DE NOTIFICAÇÃO IMEDIATA ---
    for (auto const& [marca, contagemAtual] : contagemMarcas) {
        if (marca == "Desconhecida") continue;

        int contagemAnt = contagemAnterior[marca];

        // CONDIÇÃO 1: A lata sumiu (contagem foi de >0 para 0) E a notificação ainda não foi enviada
        if (contagemAtual == 0 && contagemAnt > 0 && !notificacaoEnviada[marca]) {
            string mensagem = "🚨 ALERTA IMEDIATO: Estoque de " + marca + " esta em falta!";
            cout << "ENVIANDO NOTIFICACAO IMEDIATA: " << mensagem << endl;
            enviarNotificacaoTelegram(mensagem);
            notificacaoEnviada[marca] = true;
        } 
        // CONDIÇÃO 2: A lata voltou ao estoque, então resetamos o status
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
        putText(janela, "Latas nao identificadas: " + to_string(contagemMarcas.at("Desconhecida")), Point(20, y), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,255), 1);
    }

    return janela;
}

const Rect& GerenciadorPrateleira::getAreaPrateleira() const {
    return prateleiraArea;
}

void GerenciadorPrateleira::iniciarEnvioPeriodico() {
    if (!executandoEnvioPeriodico.load()) {
        executandoEnvioPeriodico.store(true);
        threadPeriodica = thread(&GerenciadorPrateleira::threadEnvioPeriodico, this);
        cout << "Envio periódico iniciado (intervalo: " << intervaloEnvio.count() << " segundos)" << endl;
    }
}

void GerenciadorPrateleira::pararEnvioPeriodico() {
    if (executandoEnvioPeriodico.load()) {
        executandoEnvioPeriodico.store(false);
        if (threadPeriodica.joinable()) {
            threadPeriodica.join();
        }
        cout << "Envio periódico parado" << endl;
    }
}

void GerenciadorPrateleira::definirIntervaloEnvio(int segundos) {
    intervaloEnvio = chrono::seconds(segundos);
    cout << "Intervalo de envio alterado para " << segundos << " segundos" << endl;
}

void GerenciadorPrateleira::threadEnvioPeriodico() {
    while (executandoEnvioPeriodico.load()) {
        // Aguarda o intervalo especificado
        this_thread::sleep_for(intervaloEnvio);
        
        if (!executandoEnvioPeriodico.load()) break;
        
        // Cria e envia mensagem de status
        string mensagemStatus = criarMensagemStatus();
        
        // Verifica se o status mudou desde o último envio para evitar spam
        {
            lock_guard<mutex> lock(mtxUltimoStatus);
            if (mensagemStatus != ultimoStatusEnviado) {
                cout << "ENVIANDO STATUS PERIODICO" << endl;
                enviarNotificacaoTelegram(mensagemStatus);
                ultimoStatusEnviado = mensagemStatus;
            }
        }
    }
}

string GerenciadorPrateleira::criarMensagemStatus() const {
    lock_guard<mutex> lock(mtx);
    
    // Obtém timestamp atual
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    
    stringstream ss;
    ss << "📊 RELATÓRIO DE ESTOQUE - " << put_time(localtime(&time_t), "%H:%M:%S") << "\n\n";
    
    int totalLatas = 0;
    vector<string> marcasEmFalta;
    
    for (auto const& [marca, contagem] : contagemMarcas) {
        if (marca != "Desconhecida") {
            if (contagem > 0) {
                ss << "✅ " << marca << ": " << contagem << " unidade(s)\n";
                totalLatas += contagem;
            } else {
                ss << "❌ " << marca << ": SEM ESTOQUE\n";
                marcasEmFalta.push_back(marca);
            }
        }
    }
    
    ss << "\n📈 Total no estoque: " << totalLatas << "/" << capacidadeTotal << "\n";
    
    if (contagemMarcas.at("Desconhecida") > 0) {
        ss << "❓ Latas não identificadas: " << contagemMarcas.at("Desconhecida") << "\n";
    }
    
    // Status geral
    if (totalLatas >= capacidadeTotal) {
        ss << "\n🟢 STATUS: ESTOQUE COMPLETO";
    } else if (totalLatas > 0) {
        ss << "\n🟡 STATUS: ESTOQUE PARCIAL";
    } else {
        ss << "\n🔴 STATUS: ESTOQUE VAZIO";
    }
    
    if (!marcasEmFalta.empty()) {
        ss << "\n\n⚠️ ATENÇÃO: Necessário repor estoque!";
    }
    
    return ss.str();
}

void GerenciadorPrateleira::enviarNotificacaoTelegram(const string& mensagem) {
    string comando = "curl -s -X POST https://api.telegram.org/bot" + TELEGRAM_BOT_TOKEN +
                     "/sendMessage -d chat_id=" + TELEGRAM_CHAT_ID +
                     " -d text=\"" + mensagem + "\"";

    // O '&' no final executa o comando em segundo plano para não travar o programa
    comando += " &";

    // Executa o comando no terminal
    system(comando.c_str());
}
