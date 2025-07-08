#pragma once

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

class GerenciadorPrateleira {
public:
    // Construtor
    GerenciadorPrateleira();

    // Métodos públicos
    void atualizarContagem(const std::map<std::string, int>& novasDeteccoes);
    cv::Mat criarJanelaEstoque() const;
    const cv::Rect& getAreaPrateleira() const; // Para a thread de detecção acessar a área
    void verificarTempoNotificacao(); // Novo método

private:
    // --- INÍCIO DAS ALTERAÇÕES ---
    // Função privada para enviar a mensagem
    void enviarNotificacaoTelegram(const std::string& mensagem);
    void enviarAtualizacaoCompleta(); // Novo método para enviar o relatório completo

    // Mapa para controlar o estado da notificação para cada marca
    std::map<std::string, bool> notificacaoEnviada; 
    // --- FIM DAS ALTERAÇÕES ---

    cv::Rect prateleiraArea; 
    int capacidadeTotal;
    std::map<std::string, int> contagemMarcas;
    mutable std::mutex mtx; // 'mutable' permite que seja travado em métodos 'const'

    // Novos membros para controle de tempo
    std::chrono::time_point<std::chrono::system_clock> ultimaAtualizacao;
    const std::chrono::seconds intervaloAtualizacao{30}; // 30 segundos

};
