#pragma once

#include "ThreadSafeQueue.hpp"
#include "GerenciadorPrateleira.hpp"

// Estruturas de dados para o pipeline
struct FrameData { cv::Mat frame; int frame_id; };

// ALTERADO: A estrutura ProcessedFrame não é mais necessária, pois a lógica foi unificada.

// ALTERADO: DetectionResult agora envia um vetor de pares (posição, marca)
struct DetectionResult {
    cv::Mat processedFrame;
    std::vector<std::pair<cv::Rect, std::string>> detections;
    int frame_id;
};

// Declaração das filas como 'extern'
extern ThreadSafeQueue<FrameData> rawFrameQueue;
extern ThreadSafeQueue<DetectionResult> resultQueue;
extern std::atomic<bool> shouldStop;

// Declaração das funções das threads
void threadCapturaVideo(cv::VideoCapture& cap);

// ALTERADO: Unificamos as threads de processamento e detecção em uma só
void threadProcessamentoEDeteccao(GerenciadorPrateleira& prateleira);
