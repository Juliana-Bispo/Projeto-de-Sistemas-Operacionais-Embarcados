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

// ===================================================================
// CONFIGURAÇÕES GLOBAIS E DE OTIMIZAÇÃO
// ===================================================================
const int FRAME_WIDTH = 320; // OTIMIZAÇÃO: Resolução menor para performance no RPi
const int FRAME_HEIGHT = 240;
const int NUM_PROCESSING_THREADS = 4; // Ideal para os 4 núcleos do Raspberry Pi 3
const size_t MAX_QUEUE_SIZE = 2;   // OTIMIZAÇÃO: Fila curta para baixa latência

// ===================================================================
// ESTRUTURAS DE DADOS PARA GERENCIAMENTO DA PRATELEIRA
// ===================================================================

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
        // ATENÇÃO: PASSO MAIS IMPORTANTE!
        // Calibre estas posições (x, y, largura, altura) para corresponder
        // aos espaços físicos na sua prateleira, vistos pela câmera.
        slots.emplace_back(1, Rect(40, 50, 65, 150));   // Slot 1
        slots.emplace_back(2, Rect(120, 50, 65, 150));  // Slot 2
        slots.emplace_back(3, Rect(200, 50, 65, 150));  // Slot 3
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
        Mat janela = Mat::zeros(250, 400, CV_8UC3);
        janela.setTo(Scalar(50, 50, 50));
        putText(janela, "STATUS DA PRATELEIRA", Point(40, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        int y = 70;
        for (const auto& slot : slots) {
            string statusTexto = slot.ocupado ? slot.marcaAtual : "EM FALTA";
            Scalar cor = slot.ocupado ? (slot.marcaAtual == "Desconhecida" ? Scalar(0, 255, 255) : Scalar(0, 255, 0)) : Scalar(0, 0, 255);
            putText(janela, "Slot " + to_string(slot.id) + ":", Point(20, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);
            putText(janela, statusTexto, Point(120, y), FONT_HERSHEY_SIMPLEX, 0.6, cor, 2);
            y += 40;
        }
        putText(janela, "ESC para sair", Point(20, 230), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(200, 200, 200), 1);
        return janela;
    }
};

// ===================================================================
// FUNÇÕES DE PROCESSAMENTO DE IMAGEM
// ===================================================================

string detectarCorPredominante(const Mat& bgr_roi) {
    if (bgr_roi.empty()) return "Desconhecida";
    
    Mat hsv;
    cvtColor(bgr_roi, hsv, COLOR_BGR2HSV);

    vector<Mat> hsv_channels;
    split(hsv, hsv_channels);
    // Opcional: CLAHE pode ser pesado. Descomente se a iluminação for muito irregular.
    // Ptr<CLAHE> clahe = createCLAHE();
    // clahe->setClipLimit(2.0);
    // clahe->apply(hsv_channels[2], hsv_channels[2]);
    // merge(hsv_channels, hsv);

    Mat brilhoMask;
    inRange(hsv_channels[2], 40, 255, brilhoMask);

    map<string, int> contadorDePixels;

    Mat maskVermelho1, maskVermelho2;
    inRange(hsv, Scalar(0, 70, 50), Scalar(10, 255, 255), maskVermelho1);
    inRange(hsv, Scalar(170, 70, 50), Scalar(180, 255, 255), maskVermelho2);
    contadorDePixels["Vermelho"] = countNonZero(maskVermelho1 | maskVermelho2 & brilhoMask);

    Mat maskVerde;
    inRange(hsv, Scalar(35, 50, 50), Scalar(85, 255, 255), maskVerde);
    contadorDePixels["Verde"] = countNonZero(maskVerde & brilhoMask);

    Mat maskAzul;
    inRange(hsv, Scalar(90, 50, 50), Scalar(130, 255, 255), maskAzul);
    contadorDePixels["Azul"] = countNonZero(maskAzul & brilhoMask);
    
    Mat maskLaranja;
    inRange(hsv, Scalar(11, 100, 50), Scalar(25, 255, 255), maskLaranja);
    contadorDePixels["Laranja"] = countNonZero(maskLaranja & brilhoMask);

    string corDominante = "Desconhecida";
    int maxPix = 0;
    for (auto const& [cor, contagem] : contadorDePixels) {
        if (contagem > maxPix) {
            maxPix = contagem;
            corDominante = cor;
        }
    }

    if (maxPix < (bgr_roi.total() * 0.10)) {
        return "Desconhecida";
    }

    return corDominante;
}

string classificarMarcaPorCor(const string& cor) {
    if (cor == "Verde") return "Guaraná";
    if (cor == "Vermelho") return "Coca-Cola";
    if (cor == "Azul") return "Pepsi";
    if (cor == "Laranja") return "Fanta Laranja";
    return "Desconhecida";
}

// ===================================================================
// ESTRUTURAS DO PIPELINE E FILA THREAD-SAFE (VERSÃO CORRETA E COMPLETA)
// ===================================================================

struct FrameData { Mat frame; int frame_id; };
struct ProcessedFrame { Mat frame; vector<vector<Point>> contours; int frame_id; };
struct DetectionResult { Mat processedFrame; vector<Slot> shelfStatus; int frame_id; };

template<typename T>
class ThreadSafeQueue {
private:
    queue<T> queue_;
    mutable mutex mutex_;
    condition_variable cond_;
    atomic<bool> shutdown_{false};
    size_t max_size_;

public:
    ThreadSafeQueue(size_t max_size = 0) : max_size_(max_size) {}

    void push(T item) {
        lock_guard<mutex> lock(mutex_);
        if (shutdown_) return;

        if (max_size_ > 0 && queue_.size() >= max_size_) {
            queue_.pop(); 
        }
        
        queue_.push(move(item));
        cond_.notify_one();
    }

    bool pop(T& item) {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty()) {
            return false;
        }
        
        item = move(queue_.front());
        queue_.pop();
        return true;
    }

    bool try_pop(T& item) {
        lock_guard<mutex> lock(mutex_);
        if (queue_.empty() || shutdown_) {
            return false;
        }
        item = move(queue_.front());
        queue_.pop();
        return true;
    }

    void shutdown() {
        {
            lock_guard<mutex> lock(mutex_);
            shutdown_ = true;
        }
        cond_.notify_all();
    }
};

atomic<bool> shouldStop(false);
ThreadSafeQueue<FrameData> rawFrameQueue(MAX_QUEUE_SIZE);
ThreadSafeQueue<ProcessedFrame> preprocessedQueue(MAX_QUEUE_SIZE);
ThreadSafeQueue<DetectionResult> resultQueue(MAX_QUEUE_SIZE);

// ===================================================================
// THREADS DO PIPELINE
// ===================================================================

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
        vector<Slot> statusAtualPrateleira = prateleira.slots;

        for (auto& slot : statusAtualPrateleira) {
            slot.ocupado = false;
            slot.marcaAtual = "Vazio";
        }

        for (const auto& contorno : pframe.contours) {
            Rect bbox = boundingRect(contorno);

            if (bbox.height < 50 || bbox.width < 15) continue;
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.5) continue;

            Point centroContorno = Point(bbox.x + bbox.width / 2, bbox.y + bbox.height / 2);

            for (auto& slot : statusAtualPrateleira) {
                if (slot.posicao.contains(centroContorno)) {
                    Mat roi = pframe.frame(bbox);
                    string cor = detectarCorPredominante(roi);
                    string marca = classificarMarcaPorCor(cor);

                    slot.ocupado = true;
                    slot.marcaAtual = marca;

                    rectangle(processedFrame, bbox, Scalar(0, 255, 0), 2);
                    putText(processedFrame, marca, Point(bbox.x, bbox.y - 5), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1);
                    
                    break;
                }
            }
        }
        
        for (const auto& slot : statusAtualPrateleira) {
            Scalar corSlot = slot.ocupado ? Scalar(255, 255, 0) : Scalar(0, 0, 255);
            rectangle(processedFrame, slot.posicao, corSlot, 1);
            putText(processedFrame, "S" + to_string(slot.id), Point(slot.posicao.x + 2, slot.posicao.y + 12), FONT_HERSHEY_SIMPLEX, 0.4, corSlot, 1);
        }

        resultQueue.push(DetectionResult{processedFrame, statusAtualPrateleira, pframe.frame_id});
    }
    cout << "Thread de detecção finalizada" << endl;
}

// ===================================================================
// FUNÇÃO PRINCIPAL
// ===================================================================

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
            prateleira.atualizarStatusPrateleira(result.shelfStatus);
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
