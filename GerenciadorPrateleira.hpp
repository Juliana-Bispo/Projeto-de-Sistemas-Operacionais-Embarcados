#pragma once

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

class GerenciadorPrateleira {
public:
    // Construtor
    GerenciadorPrateleira();
    
    // Destrutor
    ~GerenciadorPrateleira();

    // Métodos públicos
    void atualizarContagem(const std::map<std::string, int>& novasDeteccoes);
    cv::Mat criarJanelaEstoque() const;
    const cv::Rect& getAreaPrateleira() const;
    
    // Novos métodos para controle do envio periódico
    void iniciarEnvioPeriodico();
    void pararEnvioPeriodico();
    void definirIntervaloEnvio(int segundos); // Permite alterar o intervalo (padrão: 30s)

private:
    // Função privada para enviar a mensagem
    void enviarNotificacaoTelegram(const std::string& mensagem);
    
    // Nova função para envio periódico
    void threadEnvioPeriodico();
    
    // Função para criar mensagem de status completo
    std::string criarMensagemStatus() const;

    // Mapa para controlar o estado da notificação para cada marca
    std::map<std::string, bool> notificacaoEnviada; 

    cv::Rect prateleiraArea; 
    int capacidadeTotal;
    std::map<std::string, int> contagemMarcas;
    mutable std::mutex mtx;
    
    // Novos membros para controle da thread periódica
    std::thread threadPeriodica;
    std::atomic<bool> executandoEnvioPeriodico;
    std::chrono::seconds intervaloEnvio;
    mutable std::mutex mtxUltimoStatus; // Para controlar acesso ao último status enviado
    std::string ultimoStatusEnviado; // Para evitar enviar mensagens duplicadas
};
