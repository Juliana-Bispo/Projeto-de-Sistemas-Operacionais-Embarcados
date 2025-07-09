#include "pipeline.hpp"
#include "processamento.hpp"
#include <thread>
#include <iostream>

using namespace cv;
using namespace std;
using namespace std::chrono; // Adicionado para usar milliseconds

// Definição das variáveis globais (sem 'extern')
ThreadSafeQueue<FrameData> rawFrameQueue(2);
ThreadSafeQueue<ProcessedFrame> preprocessedQueue(2);
ThreadSafeQueue<DetectionResult> resultQueue(2);
atomic<bool> shouldStop(false);

// A thread 'threadCapturaVideo' continua a mesma do seu arquivo
void threadCapturaVideo(VideoCapture& cap) {
    cout << "Thread de captura iniciada" << endl;
    Mat frame;
    while (!shouldStop) {
        cap >> frame;
        if (frame.empty()) {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        // ATENÇÃO: A nova lógica não precisa mais da ROI aqui.
        // Simplificamos para o FrameData original.
        rawFrameQueue.push(FrameData{frame.clone(), 0});
    }
    cout << "Thread de captura finalizada" << endl;
}

// A thread 'threadPreprocessamento' agora deve processar a imagem inteira
void threadPreprocessamento() {
    cout << "Thread de pré-processamento iniciada" << endl;
    while (!shouldStop) {
        FrameData frameData;
        if (!rawFrameQueue.pop(frameData)) continue;
        
        Mat gray;
        cvtColor(frameData.frame, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, gray, Size(3, 3), 0);
        
        Mat edges;
        Canny(gray, edges, 50, 150);
        
        vector<vector<Point>> contours;
        findContours(edges, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        
        preprocessedQueue.push(ProcessedFrame{frameData.frame, contours, frameData.frame_id});
    }
    cout << "Thread de pré-processamento finalizada" << endl;
}


// A thread 'threadDetecao' foi completamente substituída por esta versão
void threadDetecao(GerenciadorPrateleira& prateleira) {
    cout << "Thread de detecção iniciada" << endl;
    
    while (!shouldStop) {
        // Passo 1: A thread verifica se já se passaram 20 segundos
        if (!prateleira.tempoDeVerificar()) {
            // Se não for a hora, ela dorme por um tempo para não usar 100% da CPU
            this_thread::sleep_for(milliseconds(500)); 
            continue; // Volta ao início do loop
        }

        // Se passou dos 20 segundos, o código continua a partir daqui
        cout << "[INFO] Ciclo de verificação de 20 segundos iniciado." << endl;
        prateleira.registrarVerificacao(); // Anota o tempo desta verificação para contar os próximos 20s

        ProcessedFrame pframe;
        // Tenta pegar um quadro da fila, mas não bloqueia se estiver vazia
        if (!preprocessedQueue.try_pop(pframe)) { 
            continue;
        }
        
        // A lógica de detecção de contornos a partir daqui é a mesma de antes
        Mat processedFrame = pframe.frame.clone();
        map<string, int> deteccoesNesteFrame;
        
        const Rect& area = prateleira.getAreaPrateleira();
        rectangle(processedFrame, area, Scalar(255, 0, 255), 2);

        for (const auto& contorno : pframe.contours) {
            Rect bbox = boundingRect(contorno);
            Point centroContorno = Point(bbox.x + bbox.width / 2, bbox.y + bbox.height / 2);
            if (!area.contains(centroContorno)) {
                continue;
            }

            if (bbox.height < 50 || bbox.width < 15) continue;
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.5) continue;

            Mat roi = pframe.frame(bbox);
            string cor = detectarCorPredominante(roi);
            string marca = classificarMarcaPorCor(cor);
            
            deteccoesNesteFrame[marca]++;

            rectangle(processedFrame, bbox, Scalar(0, 255, 0), 2);
            putText(processedFrame, marca, Point(bbox.x, bbox.y - 5), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1);
        }
        
        // Envia o resultado da detecção para a thread main, que chamará o 'atualizarContagem'
        resultQueue.push(DetectionResult{processedFrame, deteccoesNesteFrame, pframe.frame_id});
    }
    cout << "Thread de detecção finalizada" << endl;
}
