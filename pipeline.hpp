#include "pipeline.hpp"
#include "processamento.hpp"
#include <thread>
#include <iostream>

using namespace cv;
using namespace std;
using namespace std::chrono;

// Definição das variáveis globais
ThreadSafeQueue<FrameData> rawFrameQueue(2);
ThreadSafeQueue<DetectionResult> resultQueue(2);
atomic<bool> shouldStop(false);

void threadCapturaVideo(VideoCapture& cap) {
    cout << "Thread de captura iniciada" << endl;
    Mat frame;
    int frame_count = 0;
    while (!shouldStop) {
        cap >> frame;
        if (frame.empty()) {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        rawFrameQueue.push(FrameData{frame.clone(), frame_count++});
    }
    cout << "Thread de captura finalizada" << endl;
}

// NOVO: Thread unificada para processar e detectar
void threadProcessamentoEDeteccao(GerenciadorPrateleira& prateleira) {
    cout << "Thread de Processamento e Deteccao iniciada" << endl;
    
    while (!shouldStop) {
        FrameData frameData;
        if (!rawFrameQueue.pop(frameData)) continue;

        const Rect& areaPrateleira = prateleira.getAreaPrateleira();
        Mat roi_prateleira = frameData.frame(areaPrateleira);
        Mat frameParaMostrar = frameData.frame.clone();

        // --- Pré-processamento e Detecção ---
        // (Esta é a lógica combinada das suas threads anteriores)
        Mat gray, edges;
        cvtColor(roi_prateleira, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, gray, Size(3, 3), 0);
        Canny(gray, edges, 50, 150);
        
        vector<vector<Point>> contornos;
        findContours(edges, contornos, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        
        // ALTERADO: Prepara um vetor para as detecções, em vez de um mapa
        vector<pair<Rect, string>> deteccoesAtuais;
        
        for(const auto& contorno : contornos) {
            Rect bbox = boundingRect(contorno);

            // Filtros de tamanho e proporção que você já usava
            if (bbox.height < 50 || bbox.width < 15) continue;
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.5) continue;

            Mat roi_lata = roi_prateleira(bbox);
            string marca = classificarMarcaPorCor(detectarCorPredominante(roi_lata));
            
            // ALTERADO: Adiciona a detecção (posição e marca) ao vetor
            deteccoesAtuais.push_back({bbox, marca});

            // Desenha na imagem para exibição
            Rect globalRect = bbox + areaPrateleira.tl();
            rectangle(frameParaMostrar, globalRect, Scalar(0, 255, 0), 2);
            putText(frameParaMostrar, marca, globalRect.tl() - Point(0,5), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1);
        }
        
        // Coloca o resultado completo na fila para a main
        resultQueue.push(DetectionResult{frameParaMostrar, deteccoesAtuais, frameData.frame_id});
    }
    cout << "Thread de Processamento e Deteccao finalizada" << endl;
}
