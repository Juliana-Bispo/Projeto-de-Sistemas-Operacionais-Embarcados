#include <iostream>
#include <thread>
#include <vector>
#include "GerenciadorPrateleira.hpp"
#include "pipeline.hpp"

using namespace cv;
using namespace std;

const int FRAME_WIDTH = 320;
const int FRAME_HEIGHT = 240;
const int NUM_PROCESSING_THREADS = 4;

int main() {
    VideoCapture cap(0, CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir câmera" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap.set(CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    cap.set(CAP_PROP_FPS, 30);

    namedWindow("Detector de Latas - RPi", WINDOW_NORMAL);
    namedWindow("Controle de Estoque", WINDOW_NORMAL);
    
    GerenciadorPrateleira prateleira;
    
    cout << "Iniciando threads..." << endl;
    
    thread threadCaptura(threadCapturaVideo, ref(cap));
    thread threadPreproc(threadPreprocessamento);
    
    vector<thread> detectionThreads;
    for (int i = 0; i < NUM_PROCESSING_THREADS; ++i) {
        detectionThreads.emplace_back(threadDetecao, ref(prateleira));
    }
    
    cout << "Sistema iniciado. Pressione ESC para sair." << endl;

    while (true) {
        DetectionResult result;
        if (resultQueue.try_pop(result)) {
            prateleira.atualizarContagem(result.marcasDetectadas);
            imshow("Detector de Latas - RPi", result.processedFrame);
        } else {
            this_thread::sleep_for(chrono::milliseconds(10));
        }
        
        Mat janelaEstoque = prateleira.criarJanelaEstoque();
        imshow("Controle de Estoque", janelaEstoque);
        
        int key = waitKey(30);
        if (key == 27) { // ESC
            break;
        }
    }

    cout << "Finalizando threads..." << endl;
    
    shouldStop = true;
    rawFrameQueue.shutdown();
    preprocessedQueue.shutdown();
    resultQueue.shutdown();
    
    threadCaptura.join();
    threadPreproc.join();
    for (auto& t : detectionThreads) {
        t.join();
    }

    cap.release();
    destroyAllWindows();
    
    cout << "Programa finalizado." << endl;
    return 0;
}