#include <opencv2/opencv.hpp>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>
#include <condition_variable>

using namespace cv;
using namespace std;

// --- ESTRUTURAS DE DADOS ---

struct Slot {
    int id;
    Rect posicao;
    string marcaAtual = "Vazio";
    bool ocupado = false;
    chrono::steady_clock::time_point ultimaAtualizacao;

    Slot(int _id, Rect _pos) : id(_id), posicao(_pos), ultimaAtualizacao(chrono::steady_clock::now()) {}
};

struct GerenciadorPrateleira {
    vector<Slot> slots;
    mutable mutex mtx;

    GerenciadorPrateleira() {
        slots.emplace_back(1, Rect(50, 100, 100, 200));
        slots.emplace_back(2, Rect(200, 100, 100, 200));
        slots.emplace_back(3, Rect(350, 100, 100, 200));
        slots.emplace_back(4, Rect(500, 100, 100, 200));
    }

    void atualizarStatusPrateleira(const vector<Slot>& novosStatus) {
        lock_guard<mutex> lock(mtx);
        for (const auto& novoStatus : novosStatus) {
            for (auto& slotExistente : slots) {
                if (slotExistente.id == novoStatus.id) {
                    slotExistente.marcaAtual = novoStatus.marcaAtual;
                    slotExistente.ocupado = novoStatus.ocupado;
                    slotExistente.ultimaAtualizacao = chrono::steady_clock::now();
                    break;
                }
            }
        }
    }

    Mat criarJanelaEstoque() const {
        lock_guard<mutex> lock(mtx);
        Mat janela = Mat::zeros(350, 450, CV_8UC3);
        janela.setTo(Scalar(50, 50, 50));
        putText(janela, "STATUS DA PRATELEIRA", Point(50, 30), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
        int y = 80;
        for (const auto& slot : slots) {
            string statusTexto = slot.ocupado ? slot.marcaAtual : "EM FALTA";
            Scalar cor = slot.ocupado ? (slot.marcaAtual == "Desconhecida" ? Scalar(0, 255, 255) : Scalar(0, 255, 0)) : Scalar(0, 0, 255);
            putText(janela, "Slot " + to_string(slot.id) + ":", Point(20, y), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
            putText(janela, statusTexto, Point(150, y), FONT_HERSHEY_SIMPLEX, 0.7, cor, 2);
            y += 40;
        }
        putText(janela, "ESC para sair", Point(20, 330), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(200, 200, 200), 1);
        return janela;
    }
};

// --- ESTRUTURAS E FUNÇÕES DE DETECÇÃO (ATUALIZADO) ---

// Coloque aqui as duas funções: detectarCorPredominante e classificarMarcaPorCor
// (Código fornecido no início desta resposta)

string detectarCorPredominante(const Mat& bgr_roi);
string classificarMarcaPorCor(const string& cor);

// --- PIPELINE MULTITHREADING (sem alterações na estrutura) ---

struct FrameData { Mat frame; int frame_id; };
struct ProcessedFrame { Mat frame; vector<vector<Point>> contours; int frame_id; };
struct DetectionResult { Mat processedFrame; vector<Slot> shelfStatus; int frame_id; };

template<typename T>
class ThreadSafeQueue { /* ... seu código da fila ... */ };
// (O código da sua ThreadSafeQueue original pode ser colado aqui sem modificações)

atomic<bool> shouldStop(false);
ThreadSafeQueue<FrameData> rawFrameQueue;
ThreadSafeQueue<ProcessedFrame> preprocessedQueue;
ThreadSafeQueue<DetectionResult> resultQueue;
const int NUM_PROCESSING_THREADS = 4;

void threadCapturaVideo(VideoCapture& cap) { /* ... seu código ... */ }
void threadPreprocessamento() { /* ... seu código ... */ }


void threadDetecao(GerenciadorPrateleira& prateleira) {
    cout << "Thread de detecção iniciada" << endl;
    
    while (!shouldStop) {
        ProcessedFrame pframe;
        if (!preprocessedQueue.pop(pframe)) continue;
        
        Mat processedFrame = pframe.frame.clone();
        vector<Slot> statusAtualPrateleira = prateleira.slots;

        for (auto& slot : statusAtualPrateleira) {
            slot.ocupado = false;
            slot.marcaAtual = "Vazio";
        }

        for (const auto& contorno : pframe.contours) {
            Rect bbox = boundingRect(contorno);

            if (bbox.height < 100 || bbox.width < 30) continue;
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.0) continue;

            Point centroContorno = Point(bbox.x + bbox.width / 2, bbox.y + bbox.height / 2);

            for (auto& slot : statusAtualPrateleira) {
                if (slot.posicao.contains(centroContorno)) {
                    // --- PONTO CHAVE DA ATUALIZAÇÃO ---
                    // 1. O recorte (roi) é o bounding box COMPLETO do contorno.
                    Mat roi = pframe.frame(bbox);

                    // 2. A função de detecção de cor agora analisa esta área inteira.
                    string cor = detectarCorPredominante(roi);
                    string marca = classificarMarcaPorCor(cor);

                    slot.ocupado = true;
                    slot.marcaAtual = marca;

                    rectangle(processedFrame, bbox, Scalar(0, 255, 0), 2);
                    putText(processedFrame, marca, Point(bbox.x, bbox.y - 10), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
                    
                    break;
                }
            }
        }
        
        for (const auto& slot : statusAtualPrateleira) {
            Scalar corSlot = slot.ocupado ? Scalar(255, 255, 0) : Scalar(0, 0, 255);
            rectangle(processedFrame, slot.posicao, corSlot, 2, LINE_AA);
            putText(processedFrame, "Slot " + to_string(slot.id), Point(slot.posicao.x, slot.posicao.y + 20), FONT_HERSHEY_SIMPLEX, 0.6, corSlot, 2);
        }

        resultQueue.push(DetectionResult{processedFrame, statusAtualPrateleira, pframe.frame_id});
    }
    
    cout << "Thread de detecção finalizada" << endl;
}


int main() {
    // ... seu código do main, que já estava correto para esta arquitetura ...
    // (Lembre-se de colar o código completo da ThreadSafeQueue, threadCapturaVideo, etc.)
    return 0;
}