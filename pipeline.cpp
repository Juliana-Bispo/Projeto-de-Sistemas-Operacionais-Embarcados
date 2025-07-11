#include "pipeline.hpp"
#include "processamento.hpp" // Precisa das funções de processamento
#include <thread>
#include <iostream>

using namespace cv;
using namespace std;

// Definição das variáveis globais (sem 'extern')
ThreadSafeQueue<FrameData> rawFrameQueue(2);
ThreadSafeQueue<ProcessedFrame> preprocessedQueue(2);
ThreadSafeQueue<DetectionResult> resultQueue(2);
atomic<bool> shouldStop(false);

void threadCapturaVideo(VideoCapture& cap) {
    cout << "Thread de captura iniciada" << endl;
    Mat frame;
    while (!shouldStop) {
        cap >> frame;
        if (frame.empty()) {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        rawFrameQueue.push(FrameData{frame.clone(), 0});
    }
    cout << "Thread de captura finalizada" << endl;
}

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

void threadDetecao(GerenciadorPrateleira& prateleira) {
    cout << "Thread de detecção iniciada" << endl;
    while (!shouldStop) {
        ProcessedFrame pframe;
        if (!preprocessedQueue.pop(pframe)) continue;
        
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
        resultQueue.push(DetectionResult{processedFrame, deteccoesNesteFrame, pframe.frame_id});
    }
    cout << "Thread de detecção finalizada" << endl;
}
