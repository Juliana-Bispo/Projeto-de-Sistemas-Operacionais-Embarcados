#pragma once

#include "ThreadSafeQueue.hpp"
#include "GerenciadorPrateleira.hpp"

// Estruturas de dados para o pipeline
struct FrameData { cv::Mat frame; int frame_id; };
struct ProcessedFrame { cv::Mat frame; std::vector<std::vector<cv::Point>> contours; int frame_id; };
struct DetectionResult { cv::Mat processedFrame; std::map<std::string, int> marcasDetectadas; int frame_id; };

// Declaração das filas como 'extern' para que múltiplos arquivos .cpp possam conhecê-las
// sem que o linker reclame de múltiplas definições.
extern ThreadSafeQueue<FrameData> rawFrameQueue;
extern ThreadSafeQueue<ProcessedFrame> preprocessedQueue;
extern ThreadSafeQueue<DetectionResult> resultQueue;
extern std::atomic<bool> shouldStop;

// Declaração das funções das threads
void threadCapturaVideo(cv::VideoCapture& cap);
void threadPreprocessamento();
void threadDetecao(GerenciadorPrateleira& prateleira);