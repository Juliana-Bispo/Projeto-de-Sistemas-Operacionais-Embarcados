#include <iostream>
#include <thread>
#include <vector>
#include "GerenciadorPrateleira.hpp"
#include "pipeline.hpp"
#include <chrono>

using namespace cv;
using namespace std;

int main() {
    VideoCapture cap(0); // Usa o backend padrão
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir câmera" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    namedWindow("Detector de Latas - RPi", WINDOW_NORMAL);
    namedWindow("Controle de Estoque", WINDOW_NORMAL);
    
    GerenciadorPrateleira prateleira;
    
    cout << "Iniciando threads..." << endl;
    
    // ALTERADO: Lógica de threads simplificada para maior eficiência
    thread threadCaptura(threadCapturaVideo, ref(cap));
    thread threadProcDetec(threadProcessamentoEDeteccao, ref(prateleira)); // Nova thread unificada
    
    cout << "Sistema iniciado. Pressione ESC para sair." << endl;

    while (true) {
        DetectionResult result;
        if (resultQueue.try_pop(result)) {
            // ALTERADO: Chama a nova função de atualização do gerenciador
            prateleira.atualizarDeteccoes(result.detections);
            imshow("Detector de Latas - RPi", result.processedFrame);
        }
        
        // NOVO: Chama a verificação de alertas continuamente no loop principal
        // É esta linha que ativa a lógica de "esperar 30 segundos"
        prateleira.verificarAlertasDeFalta();

        Mat janelaEstoque = prateleira.criarJanelaEstoque();
        imshow("Controle de Estoque", janelaEstoque);
        
        int key = waitKey(30);
        if (key == 27) { // ESC
            shouldStop = true; // Sinaliza para as threads pararem
            break;
        }
    }

    cout << "Finalizando threads..." << endl;
    
    rawFrameQueue.shutdown();
    resultQueue.shutdown();
    
    threadCaptura.join();
    threadProcDetec.join();

    cap.release();
    destroyAllWindows();
    
    cout << "Programa finalizado." << endl;
    return 0;
}
