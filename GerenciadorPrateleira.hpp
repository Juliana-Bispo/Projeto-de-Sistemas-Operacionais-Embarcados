#pragma once

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

// NOVO: Estrutura para rastrear cada lata individualmente
struct LataRastreada {
    int id;
    cv::Rect ultimaPosicao;
    std::string marca;
    std::chrono::steady_clock::time_point ultimaVezVista;
    bool alertaEnviado;
};

class GerenciadorPrateleira {
public:
    GerenciadorPrateleira();
    
    // ALTERADO: Recebe um vetor de detecções (posição e marca)
    void atualizarDeteccoes(const std::vector<std::pair<cv::Rect, std::string>>& novasDeteccoes);
    
    // NOVO: Verifica alertas de falta com base no tempo
    void verificarAlertasDeFalta();

    cv::Mat criarJanelaEstoque() const;
    const cv::Rect& getAreaPrateleira() const;

private:
    void enviarNotificacaoTelegram(const std::string& mensagem);
    std::string formatarMensagemEstoque() const;
    double calcularIoU(const cv::Rect& a, const cv::Rect& b) const; // Ajuda no rastreamento

    cv::Rect prateleiraArea;
    mutable std::mutex mtx;

    // ALTERADO: A estrutura de dados principal agora é um vetor de latas rastreadas
    std::vector<LataRastreada> latasRastreadas;
    int proximoIdLata;

    const int SEGUNDOS_PARA_ALERTA = 30; // Tempo de espera para confirmar a ausência
};
