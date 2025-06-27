#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <vector>
#include <future>

using namespace cv;
using namespace std;

// Configurações globais
const size_t MAX_QUEUE_SIZE = 10;
const int NUM_PROCESSING_THREADS = 4; // Número de threads de processamento (idealmente = núcleos CPU)
const int PROCESSING_PIPELINE_STAGES = 3; // Pré-processamento, detecção, pós-processamento

// Estrutura para gerenciar o estoque de refrigerantes (mantida igual)
struct GerenciadorEstoque {
    map<string, int> quantidadeAtual;
    map<string, int> capacidadeMaxima;
    mutable mutex mtx;
    
    GerenciadorEstoque() {
        capacidadeMaxima["Guaraná"] = 1;
        capacidadeMaxima["Coca-Cola"] = 1;
        capacidadeMaxima["Pepsi"] = 1;
        capacidadeMaxima["Fanta Laranja"] = 1;
        //capacidadeMaxima["Fanta Uva"] = 1;
        
        for (auto& par : capacidadeMaxima) {
            quantidadeAtual[par.first] = 0;
        }
    }
    
    void adicionarRefrigerante(const string& marca) {
        lock_guard<mutex> lock(mtx);
        if (marca != "Desconhecida" && capacidadeMaxima.find(marca) != capacidadeMaxima.end()) {
            if (quantidadeAtual[marca] < capacidadeMaxima[marca]) {
                quantidadeAtual[marca]++;
            }
        }
    }
    
    void removerRefrigerante(const string& marca) {
        lock_guard<mutex> lock(mtx);
        if (marca != "Desconhecida" && quantidadeAtual.find(marca) != quantidadeAtual.end()) {
            if (quantidadeAtual[marca] > 0) {
                quantidadeAtual[marca]--;
            }
        }
    }
    
    string getStatusEstoque(const string& marca) const {
        lock_guard<mutex> lock(mtx);
        if (capacidadeMaxima.find(marca) == capacidadeMaxima.end()) {
            return "Marca não cadastrada";
        }
        
        int atual = quantidadeAtual.at(marca);
        int maximo = capacidadeMaxima.at(marca);
        
        if (atual == maximo) {
            return "Estoque completo";
        } else if (atual < maximo * 0.3) {
            return "Precisa de reposição: " + to_string(atual) + "/" + to_string(maximo);
        } else {
            return "Quantidade atual: " + to_string(atual) + "/" + to_string(maximo);
        }
    }
    
    Mat criarJanelaEstoque() const {
        lock_guard<mutex> lock(mtx);
        Mat janela = Mat::zeros(400, 500, CV_8UC3);
        janela.setTo(Scalar(50, 50, 50));
        
        putText(janela, "CONTROLE DE ESTOQUE", Point(50, 30), 
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
        
        int y = 70;
        for (auto& par : capacidadeMaxima) {
            string marca = par.first;
            string status = getStatusEstoqueUnsafe(marca);
            
            Scalar cor;
            if (status == "Estoque completo") {
                cor = Scalar(0, 255, 0);
            } else if (status.find("Precisa de reposição") != string::npos) {
                cor = Scalar(0, 0, 255);
            } else {
                cor = Scalar(0, 255, 255);
            }
            
            putText(janela, marca + ":", Point(20, y), 
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);
            putText(janela, status, Point(20, y + 20), 
                    FONT_HERSHEY_SIMPLEX, 0.5, cor, 1);
            
            int barWidth = 200;
            int barHeight = 10;
            int atual = quantidadeAtual.at(marca);
            int maximo = capacidadeMaxima.at(marca);
            float porcentagem = (float)atual / maximo;
            
            rectangle(janela, Point(250, y - 10), Point(250 + barWidth, y), 
                     Scalar(100, 100, 100), -1);
            
            int preenchimento = (int)(barWidth * porcentagem);
            Scalar corBarra = (porcentagem > 0.7) ? Scalar(0, 255, 0) : 
                             (porcentagem > 0.3) ? Scalar(0, 255, 255) : Scalar(0, 0, 255);
            rectangle(janela, Point(250, y - 10), Point(250 + preenchimento, y), 
                     corBarra, -1);
            
            y += 60;
        }
        
        putText(janela, "Teclas: 'r' = reset estoque, ESC = sair", 
                Point(20, 380), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(200, 200, 200), 1);
        
        return janela;
    }
    
    void resetarEstoque() {
        lock_guard<mutex> lock(mtx);
        for (auto& par : quantidadeAtual) {
            par.second = 0;
        }
    }

private:
    string getStatusEstoqueUnsafe(const string& marca) const {
        if (capacidadeMaxima.find(marca) == capacidadeMaxima.end()) {
            return "Marca não cadastrada";
        }
        
        int atual = quantidadeAtual.at(marca);
        int maximo = capacidadeMaxima.at(marca);
        
        if (atual == maximo) {
            return "Estoque completo";
        } else if (atual < maximo * 0.3) {
            return "Precisa de reposição: " + to_string(atual) + "/" + to_string(maximo);
        } else {
            return "Quantidade atual: " + to_string(atual) + "/" + to_string(maximo);
        }
    }
};

// Estruturas para pipeline de processamento
struct FrameData {
    Mat frame;
    chrono::steady_clock::time_point timestamp;
    int frame_id;
    
    FrameData() = default;
    FrameData(const Mat& f, int id) : frame(f.clone()), timestamp(chrono::steady_clock::now()), frame_id(id) {}
};

struct ProcessedFrame {
    Mat frame;
    Mat edges;
    vector<vector<Point>> contours;
    int frame_id;
};

struct DetectionResult {
    Mat processedFrame;
    map<string, int> detectedBrands;
    chrono::steady_clock::time_point timestamp;
    int frame_id;
};

// Fila thread-safe com condition variables
template<typename T>
class ThreadSafeQueue {
private:
    queue<T> queue_;
    mutable mutex mutex_;
    condition_variable cond_;
    atomic<bool> shutdown_{false};
    
public:
    void push(T item) {
        lock_guard<mutex> lock(mutex_);
        if (shutdown_) return;
        queue_.push(move(item));
        cond_.notify_one();
    }
    
    bool try_pop(T& item) {
        lock_guard<mutex> lock(mutex_);
        if (queue_.empty() || shutdown_) return false;
        item = move(queue_.front());
        queue_.pop();
        return true;
    }
    
    bool pop(T& item) {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });
        if (shutdown_) return false;
        item = move(queue_.front());
        queue_.pop();
        return true;
    }
    
    void shutdown() {
        shutdown_ = true;
        cond_.notify_all();
    }
    
    bool empty() const {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }
};

// Variáveis globais para comunicação entre threads
ThreadSafeQueue<FrameData> rawFrameQueue;
ThreadSafeQueue<ProcessedFrame> preprocessedQueue;
ThreadSafeQueue<DetectionResult> resultQueue;
atomic<bool> shouldStop(false);
atomic<int> frameCounter(0);

// Funções de processamento (mantidas iguais)
string detectarCorPredominante(const Mat& bgr_roi) {
    if (bgr_roi.empty()) return "Desconhecido";
    
    Mat hsv;
    cvtColor(bgr_roi, hsv, COLOR_BGR2HSV);

    vector<Mat> hsv_channels;
    split(hsv, hsv_channels);

    if (hsv_channels.size() != 3) {
        return "Desconhecido";
    }

    Ptr<CLAHE> clahe = createCLAHE();
    clahe->setClipLimit(4.0);
    clahe->apply(hsv_channels[2], hsv_channels[2]);

    merge(hsv_channels, hsv);
    GaussianBlur(hsv, hsv, Size(5, 5), 0);

    Mat brilhoMask;
    inRange(hsv_channels[2], 40, 255, brilhoMask);

    map<string, int> contador;

    vector<pair<string, Scalar>> coresInferiores = {
        {"Verde", Scalar(35, 50, 40)},
        {"Vermelho1", Scalar(0, 70, 40)},
        {"Vermelho2", Scalar(170, 70, 40)},
        {"Azul", Scalar(90, 40, 40)},
        {"Laranja", Scalar(11, 100, 50)},
        {"Roxo", Scalar(130, 40, 40)}
    };

    vector<pair<string, Scalar>> coresSuperiores = {
        {"Verde", Scalar(85, 255, 255)},
        {"Vermelho1", Scalar(10, 255, 255)},
        {"Vermelho2", Scalar(180, 255, 255)},
        {"Azul", Scalar(130, 255, 255)},
        {"Laranja", Scalar(25, 255, 255)},
        {"Roxo", Scalar(160, 255, 255)}
    };

    for (size_t i = 0; i < coresInferiores.size(); ++i) {
        Mat mask;
        inRange(hsv, coresInferiores[i].second, coresSuperiores[i].second, mask);
        bitwise_and(mask, brilhoMask, mask);
        int count = countNonZero(mask);
        contador[coresInferiores[i].first] += count;
    }

    contador["Vermelho"] = contador["Vermelho1"] + contador["Vermelho2"];

    string corDominante = "Desconhecido";
    int maxPix = 0;
    for (auto& par : contador) {
        if (par.second > maxPix && par.first != "Vermelho1" && par.first != "Vermelho2") {
            maxPix = par.second;
            corDominante = par.first;
        }
    }

    return corDominante;
}

string classificarMarcaPorCor(const string& cor) {
    if (cor == "Verde") return "Guaraná";
    if (cor == "Vermelho") return "Coca-Cola";
    if (cor == "Azul") return "Pepsi";
    if (cor == "Laranja") return "Fanta Laranja";
    //if (cor == "Roxo") return "Fanta Uva";
    return "Desconhecida";
}

// Thread para captura de vídeo
void threadCapturaVideo(VideoCapture& cap) {
    Mat frame;
    cout << "Thread de captura iniciada" << endl;
    
    while (!shouldStop) {
        cap >> frame;
        if (frame.empty()) {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        
        int current_id = ++frameCounter;
        rawFrameQueue.push(FrameData(frame, current_id));
        
        this_thread::sleep_for(chrono::milliseconds(33)); // ~30 FPS
    }
    
    cout << "Thread de captura finalizada" << endl;
}

// Thread para pré-processamento
void threadPreprocessamento() {
    cout << "Thread de pré-processamento iniciada" << endl;
    
    while (!shouldStop) {
        FrameData frameData;
        if (!rawFrameQueue.pop(frameData)) continue;
        
        // Pré-processamento
        Mat gray;
        cvtColor(frameData.frame, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, gray, Size(5, 5), 0);
        
        Mat edges;
        Canny(gray, edges, 50, 150);
        
        vector<vector<Point>> contours;
        findContours(edges, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        
        preprocessedQueue.push(ProcessedFrame{frameData.frame.clone(), edges, contours, frameData.frame_id});
    }
    
    cout << "Thread de pré-processamento finalizada" << endl;
}

// Thread para detecção de objetos
void threadDetecao() {
    cout << "Thread de detecção iniciada" << endl;
    
    while (!shouldStop) {
        ProcessedFrame pframe;
        if (!preprocessedQueue.pop(pframe)) continue;
        
        map<string, int> deteccoesAtuais;
        Mat processedFrame = pframe.frame.clone();

        for (const auto& contorno : pframe.contours) {
            Rect bbox = boundingRect(contorno);

            if (bbox.height < 100 || bbox.width < 30) continue;
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.0) continue;

            Rect tercoInferior(bbox.x, bbox.y + (2 * bbox.height / 3), bbox.width, bbox.height / 3);
            Rect frameRect(0, 0, pframe.frame.cols, pframe.frame.rows);
            Rect validROI = tercoInferior & frameRect;
            if (validROI.area() < tercoInferior.area() * 0.8) continue;

            Mat roi = pframe.frame(validROI);
            string cor = detectarCorPredominante(roi);
            string marca = classificarMarcaPorCor(cor);

            if (marca != "Desconhecida") {
                deteccoesAtuais[marca]++;
            }

            rectangle(processedFrame, bbox, Scalar(0, 255, 0), 2);
            putText(processedFrame, marca, Point(bbox.x, bbox.y - 10), 
                    FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        }
        
        resultQueue.push(DetectionResult{processedFrame, deteccoesAtuais, chrono::steady_clock::now(), pframe.frame_id});
    }
    
    cout << "Thread de detecção finalizada" << endl;
}

int main() {
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir câmera" << endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(CAP_PROP_FPS, 30);

    namedWindow("Lata Detection", WINDOW_NORMAL);
    namedWindow("Controle de Estoque", WINDOW_NORMAL);
    
    GerenciadorEstoque estoque;
    
    map<string, int> ultimaDeteccao;
    int framesSemDeteccao = 0;
    
    cout << "Iniciando threads..." << endl;
    
    // Inicia thread de captura
    thread threadCaptura(threadCapturaVideo, ref(cap));
    
    // Inicia threads de pré-processamento (1 thread)
    thread threadPreproc(threadPreprocessamento);
    
    // Inicia múltiplas threads de detecção
    vector<thread> detectionThreads;
    for (int i = 0; i < NUM_PROCESSING_THREADS; ++i) {
        detectionThreads.emplace_back(threadDetecao);
    }
    
    cout << "Sistema iniciado. Pressione ESC para sair." << endl;

    // Loop principal
    while (true) {
        DetectionResult result;
        bool hasResult = resultQueue.try_pop(result);
        
        if (hasResult) {
            bool houveDeteccao = !result.detectedBrands.empty();
            
            if (houveDeteccao) {
                framesSemDeteccao = 0;
                for (auto& par : result.detectedBrands) {
                    if (ultimaDeteccao[par.first] != par.second) {
                        cout << "Nova detecção: " << par.first << " (quantidade: " << par.second << ")" << endl;
                        int diferenca = par.second - ultimaDeteccao[par.first];
                        for (int i = 0; i < diferenca; i++) {
                            estoque.adicionarRefrigerante(par.first);
                        }
                    }
                }
                ultimaDeteccao = result.detectedBrands;
            } else {
                framesSemDeteccao++;
                if (framesSemDeteccao > 30) {
                    ultimaDeteccao.clear();
                }
            }

            imshow("Lata Detection", result.processedFrame);
        }
        
        Mat janelaEstoque = estoque.criarJanelaEstoque();
        imshow("Controle de Estoque", janelaEstoque);
        
        int key = waitKey(30);
        if (key == 27) break;
        if (key == 'r' || key == 'R') {
            estoque.resetarEstoque();
            cout << "Estoque resetado!" << endl;
        }
        if (key == '1') estoque.removerRefrigerante("Guaraná");
        if (key == '2') estoque.removerRefrigerante("Coca-Cola");
        if (key == '3') estoque.removerRefrigerante("Pepsi");
        if (key == '4') estoque.removerRefrigerante("Fanta Laranja");
        //if (key == '5') estoque.removerRefrigerante("Fanta Uva");
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
