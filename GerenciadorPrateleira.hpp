#pragma once

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <vector>
#include <mutex>

class GerenciadorPrateleira {
public:
    // Construtor
    GerenciadorPrateleira();

    // Métodos públicos
    void atualizarContagem(const std::map<std::string, int>& novasDeteccoes);
    cv::Mat criarJanelaEstoque() const;
    const cv::Rect& getAreaPrateleira() const; // Para a thread de detecção acessar a área

private:
    cv::Rect prateleiraArea; 
    int capacidadeTotal;
    std::map<std::string, int> contagemMarcas;
    mutable std::mutex mtx; // 'mutable' permite que seja travado em métodos 'const'
};