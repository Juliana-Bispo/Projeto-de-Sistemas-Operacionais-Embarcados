#include "GerenciadorPrateleira.hpp"
#include <iostream> // Para debug
#include <cstdlib>  // Para a função system()

using namespace cv;
using namespace std;

// --- CONFIGURAÇÃO DO TELEGRAM ---
const string TELEGRAM_BOT_TOKEN = "8032466567:AAHzaRC_RHM7peoVXQBwYdSv6MG57MkOZtA";
const string TELEGRAM_CHAT_ID = "996099722";


GerenciadorPrateleira::GerenciadorPrateleira() {
    prateleiraArea = Rect(20, 40, 280, 180);
    capacidadeTotal = 4;
    
    // Inicializa a contagem e o status de notificação para cada marca
    const vector<string> marcas = {"Guarana", "Coca-Cola", "Pepsi", "Fanta Laranja", "Desconhecida"};
    for (const auto& marca : marcas) {
        contagemMarcas[marca] = 0;
        notificacaoEnviada[marca] = false; // Inicia como "nenhuma notificação foi enviada"
    }
    // ADICIONADO: Inicializa o tempo da última verificação
    ultimaVerificacao = std::chrono::steady_clock::now();
}

// ADICIONADO: Método auxiliar para verificar se deve checar notificações
bool GerenciadorPrateleira::deveVerificarNotificacao() {
    auto agora = std::chrono::steady_clock::now();
    auto tempoDecorrido = std::chrono::duration_cast<std::chrono::seconds>(agora - ultimaVerificacao).count();
    
    if (tempoDecorrido >= INTERVALO_VERIFICACAO_SEGUNDOS) {
        ultimaVerificacao = agora;
        return true;
    }
    return false;
}

// MODIFICADO: A função 'atualizarContagem' agora verifica o tempo antes de enviar notificações
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

    // --- LÓGICA DE NOTIFICAÇÃO ---
    // Compara o estado novo com o anterior para decidir se envia a mensagem
    if (deveVerificarNotificacao()) {
        cout << "VERIFICANDO NOTIFICACOES... (30 segundos se passaram)" << endl;
        
        for (auto const& [marca, contagemAtual] : contagemMarcas) {
            if (marca == "Desconhecida") continue; // Não notificar para latas desconhecidas

            int contagemAnt = contagemAnterior[marca];

            // CONDIÇÃO 1: A lata sumiu (contagem foi de >0 para 0) E a notificação ainda não foi enviada
            if (contagemAtual == 0 && contagemAnt > 0 && !notificacaoEnviada[marca]) {
                string mensagem = "ALERTA: Estoque de " + marca + " esta em falta!";
                cout << "ENVIANDO NOTIFICACAO: " << mensagem << endl;
                enviarNotificacaoTelegram(mensagem);
                notificacaoEnviada[marca] = true; // Marca que a notificação foi enviada para evitar spam
            } 
            // CONDIÇÃO 2: A lata voltou ao estoque, então resetamos o status
            else if (contagemAtual > 0 && notificacaoEnviada[marca]) {
                cout << "RESETANDO STATUS DE NOTIFICACAO PARA: " << marca << endl;
                notificacaoEnviada[marca] = false; // Permite que seja notificado novamente no futuro se faltar
            }
        }
    }
}

// A função 'criarJanelaEstoque()' continua a mesma que você enviou.
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

// --- NOVA FUNÇÃO ADICIONADA ---
// Função que envia a mensagem para o Telegram usando o comando 'curl' do sistema
void GerenciadorPrateleira::enviarNotificacaoTelegram(const string& mensagem) {
    string comando = "curl -s -X POST https://api.telegram.org/bot" + TELEGRAM_BOT_TOKEN +
                     "/sendMessage -d chat_id=" + TELEGRAM_CHAT_ID +
                     " -d text=\"" + mensagem + "\"";

    // O '&' no final executa o comando em segundo plano para não travar o programa
    comando += " &";

    // Executa o comando no terminal
    system(comando.c_str());
}
