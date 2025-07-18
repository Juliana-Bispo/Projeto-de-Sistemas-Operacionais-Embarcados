#pragma once

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

// Estrutura para rastrear cada lata individualmente
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
    
    // Atualiza o estado com base nas detecções do frame atual
    void atualizarDeteccoes(const std::vector<std::pair<cv::Rect, std::string>>& novasDeteccoes);
    
    // Verifica se alguma lata rastreada está ausente por muito tempo
    void verificarAlertasDeFalta();

    // Funções de utilidade
    cv::Mat criarJanelaEstoque() const;
    const cv::Rect& getAreaPrateleira() const;

private:
    void enviarNotificacaoTelegram(const std::string& mensagem);
    std::string formatarMensagemEstoque() const;
    double calcularIoU(const cv::Rect& a, const cv::Rect& b) const;

    cv::Rect prateleiraArea;
    mutable std::mutex mtx;

    std::vector<LataRastreada> latasRastreadas;
    int proximoIdLata;

    // Tempo em segundos que um item precisa estar ausente para gerar um alerta
    const int SEGUNDOS_PARA_ALERTA = 30;
};
