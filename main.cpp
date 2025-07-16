#include <iostream>
#include <vector>
#include <map>
#include <opencv2/opencv.hpp>
#include "GerenciadorPrateleira.hpp"
#include "processamento.hpp"

// Função para agrupar retângulos que se sobrepõem. Essencial para contar cada lata apenas uma vez.
void agruparRetangulos(std::vector<cv::Rect>& caixas) {
    if (caixas.empty()) return;

    std::vector<cv::Rect> caixasAgrupadas;
    std::vector<bool> processado(caixas.size(), false);

    for (size_t i = 0; i < caixas.size(); ++i) {
        if (processado[i]) continue;

        cv::Rect retanguloUnido = caixas[i];
        processado[i] = true;

        for (size_t j = i + 1; j < caixas.size(); ++j) {
            if (processado[j]) continue;
            
            // Se os retângulos se sobrepõem, una-os
            if ((retanguloUnido & caixas[j]).area() > 0) {
                retanguloUnido |= caixas[j]; // O operador '|' cria a união dos dois retângulos
                processado[j] = true;
                
                // Reinicia a verificação interna para fundir com o novo retângulo maior
                j = i; 
            }
        }
        caixasAgrupadas.push_back(retanguloUnido);
    }
    caixas = caixasAgrupadas;
}


int main() {
    // Tenta abrir a câmera padrão. Mude o '0' se tiver múltiplas câmeras.
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "ERRO: Nao foi possivel abrir a camera." << std::endl;
        return -1;
    }

    GerenciadorPrateleira gerenciador;

    cv::Mat frame;
    while (cap.read(frame)) {
        if (frame.empty()) break;

        // ETAPA 1: Pré-processamento e Detecção
        // =======================================

        // Clona o frame original para desenhar os resultados depois
        cv::Mat frameResultado = frame.clone();
        
        // Pega a área de interesse definida no gerenciador
        const cv::Rect& areaPrateleira = gerenciador.getAreaPrateleira();
        cv::rectangle(frameResultado, areaPrateleira, cv::Scalar(255, 255, 0), 2); // Desenha a área da prateleira
        cv::Mat roi = frame(areaPrateleira);

        // Converte a ROI para o espaço de cores HSV para melhor detecção de cor
        cv::Mat hsv;
        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);

        // Aplica um desfoque para reduzir ruído e evitar múltiplas detecções na mesma lata
        cv::GaussianBlur(hsv, hsv, cv::Size(7, 7), 1.5);

        // Combina máscaras de cor para encontrar TODAS as latas de uma vez
        // Os ranges de cor podem precisar de ajustes finos
        cv::Mat maskVermelho1, maskVermelho2, maskVermelho;
        cv::inRange(hsv, cv::Scalar(0, 120, 70), cv::Scalar(10, 255, 255), maskVermelho1);
        cv::inRange(hsv, cv::Scalar(170, 120, 70), cv::Scalar(180, 255, 255), maskVermelho2);
        maskVermelho = maskVermelho1 | maskVermelho2; // Coca-Cola

        cv::Mat maskVerde;
        cv::inRange(hsv, cv::Scalar(35, 50, 50), cv::Scalar(85, 255, 255), maskVerde); // Guarana

        cv::Mat maskAzul;
        cv::inRange(hsv, cv::Scalar(95, 80, 50), cv::Scalar(130, 255, 255), maskAzul); // Pepsi

        cv::Mat maskLaranja;
        cv::inRange(hsv, cv::Scalar(5, 100, 100), cv::Scalar(25, 255, 255), maskLaranja); // Fanta

        // Combina todas as máscaras em uma só
        cv::Mat mascaraTotal = maskVermelho | maskVerde | maskAzul | maskLaranja;

        // Usa operações morfológicas para fechar buracos e remover ruídos
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(mascaraTotal, mascaraTotal, cv::MORPH_CLOSE, kernel, cv::Point(-1,-1), 2);

        // ETAPA 2: Filtragem e Agrupamento
        // ==================================

        std::vector<std::vector<cv::Point>> contornos;
        cv::findContours(mascaraTotal, contornos, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::vector<cv::Rect> retangulosDetectados;
        for (const auto& contorno : contornos) {
            cv::Rect retangulo = cv::boundingRect(contorno);
            
            double area = retangulo.area();
            float proporcao = (float)retangulo.height / retangulo.width;

            // Filtra por área e proporção para remover detecções falsas
            // **Ajuste estes valores para sua necessidade!**
            if (area > 1000 && area < 50000 && proporcao > 1.0 && proporcao < 4.0) {
                 retangulosDetectados.push_back(retangulo);
            }
        }
        
        // **A ETAPA MAIS IMPORTANTE: Agrupa os retângulos que se sobrepõem**
        agruparRetangulos(retangulosDetectados);

        // ETAPA 3: Classificação e Atualização
        // =======================================

        std::map<std::string, int> contagemAtual;
        for (const auto& ret : retangulosDetectados) {
            // Pega a ROI do frame original (colorido) para classificar a cor
            cv::Mat roiLata = roi(ret); 

            std::string cor = detectarCorPredominante(roiLata);
            std::string marca = classificarMarcaPorCor(cor);

            contagemAtual[marca]++;

            // Desenha o resultado na tela para depuração
            cv::rectangle(frameResultado, ret.tl() + areaPrateleira.tl(), ret.br() + areaPrateleira.tl(), cv::Scalar(0, 255, 0), 2);
            cv::putText(frameResultado, marca, ret.tl() + areaPrateleira.tl() + cv::Point(5, -10), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        }

        gerenciador.atualizarContagem(contagemAtual);

        // ETAPA 4: Exibição
        // ===================

        cv::Mat janelaEstoque = gerenciador.criarJanelaEstoque();
        cv::imshow("Estoque", janelaEstoque);
        cv::imshow("Deteccao de Latas", frameResultado);

        if (cv::waitKey(30) >= 0) break;
    }

    return 0;
}
