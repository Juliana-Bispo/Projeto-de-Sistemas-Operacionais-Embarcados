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
#include <curl/curl.h>          //config do bot

using namespace cv;
using namespace std;

//====================================================================
// CONFIGURAÇÕES DO BOT DO TELEGRAM
//====================================================================

void enviarAlertaTelegram(const string& mensagem) {
    string token = "8032466567:AAHzaRC_RHM7peoVXQBwYdSv6MG57MkOZtA";       
    string chat_id = "996099722";   

    string url = "https://api.telegram.org/bot" + token +
                 "/sendMessage?chat_id=" + chat_id +
                 "&text=" + curl_easy_escape(nullptr, mensagem.c_str(), mensagem.size());

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            cerr << "Erro ao enviar alerta: " << curl_easy_strerror(res) << endl;
        }
    }
}


// ===================================================================
// CONFIGURAÇÕES GLOBAIS E DE OTIMIZAÇÃO
// ===================================================================
const int FRAME_WIDTH = 320; 
const int FRAME_HEIGHT = 240;
const int NUM_PROCESSING_THREADS = 4;
const size_t MAX_QUEUE_SIZE = 2;

// ===================================================================
// ESTRUTURAS DE DADOS PARA GERENCIAMENTO DA PRATELEIRA
// ===================================================================

struct GerenciadorPrateleira {
    Rect prateleiraArea; 
    int capacidadeTotal;
    map<string, int> contagemMarcas;
    mutable mutex mtx;

    GerenciadorPrateleira() {
        // ATUALIZAÇÃO: Área da prateleira foi aumentada. Ajuste conforme necessário.
        prateleiraArea = Rect(20, 40, 280, 180); // (x, y, largura, altura)
        capacidadeTotal = 4;
        
        // ATUALIZAÇÃO: "Guaraná" foi alterado para "Guarana"
        contagemMarcas["Guarana"] = 0;
        contagemMarcas["Coca-Cola"] = 0;
        contagemMarcas["Pepsi"] = 0;
        contagemMarcas["Fanta Laranja"] = 0;
        contagemMarcas["Desconhecida"] = 0;
    }

    void atualizarContagem(const map<string, int>& novasDeteccoes) {
        lock_guard<mutex> lock(mtx);
        for (auto& par : contagemMarcas) {
            par.second = 0;
        }
        for (auto const& [marca, contagem] : novasDeteccoes) {
            if (contagemMarcas.count(marca)) {
                contagemMarcas[marca] = contagem;
            }
        }
    }

    Mat criarJanelaEstoque() const {
        lock_guard<mutex> lock(mtx);
        Mat janela = Mat::zeros(300, 400, CV_8UC3);
        janela.setTo(Scalar(50, 50, 50));
        putText(janela, "ESTOQUE NA PRATELEIRA", Point(40, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        
        int y = 70;
        int totalLatas = 0;
        for (auto const& [marca, contagem] : contagemMarcas) {
            if (marca != "Desconhecida" && contagem > 0) {
                 putText(janela, marca + ":", Point(20, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);
                 putText(janela, to_string(contagem), Point(200, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 255), 2);
                 y += 30;
                 totalLatas += contagem;
            }
        }
        
        y += 20;
        string statusGeral = "Total: " + to_string(totalLatas) + "/" + to_string(capacidadeTotal);
        Scalar corStatus = (totalLatas >= capacidadeTotal) ? Scalar(0, 255, 0) : Scalar(255, 255, 0);
        putText(janela, statusGeral, Point(20, y), FONT_HERSHEY_SIMPLEX, 0.7, corStatus, 2);

        if(contagemMarcas.at("Desconhecida") > 0) {
            y += 30;
            putText(janela, "Latas nao identificadas: " + to_string(contagemMarcas.at("Desconhecida")), Point(20, y), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,255), 1);
        }

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
    map<string, int> contadorDePixels;

    Mat maskVermelho1, maskVermelho2;
    inRange(hsv, Scalar(0, 100, 100), Scalar(10, 255, 255), maskVermelho1);
    inRange(hsv, Scalar(160, 100, 100), Scalar(179, 255, 255), maskVermelho2);
    contadorDePixels["Vermelho"] = countNonZero(maskVermelho1 | maskVermelho2);

    Mat maskVerde;
    inRange(hsv, Scalar(35, 40, 40), Scalar(85, 255, 255), maskVerde);
    contadorDePixels["Verde"] = countNonZero(maskVerde);

    Mat maskAzul;
    inRange(hsv, Scalar(95, 60, 40), Scalar(130, 255, 255), maskAzul);
    contadorDePixels["Azul"] = countNonZero(maskAzul);
    
    Mat maskLaranja;
    inRange(hsv, Scalar(5, 100, 100), Scalar(25, 255, 255), maskLaranja);
    contadorDePixels["Laranja"] = countNonZero(maskLaranja);

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
    // ATUALIZAÇÃO: "Guaraná" foi alterado para "Guarana"
    if (cor == "Verde") return "Guarana";
    if (cor == "Vermelho") return "Coca-Cola";
    if (cor == "Azul") return "Pepsi";
    if (cor == "Laranja") return "Fanta Laranja";
    return "Desconhecida";
}

// ===================================================================
// ESTRUTURAS DO PIPELINE E FILA THREAD-SAFE
// ===================================================================

struct FrameData { Mat frame; int frame_id; };
struct ProcessedFrame { Mat frame; vector<vector<Point>> contours; int frame_id; };
struct DetectionResult { Mat processedFrame; map<string, int> marcasDetectadas; int frame_id; };

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
        map<string, int> deteccoesNesteFrame;
        rectangle(processedFrame, prateleira.prateleiraArea, Scalar(255, 0, 255), 2);

        for (const auto& contorno : pframe.contours) {
            Rect bbox = boundingRect(contorno);
            Point centroContorno = Point(bbox.x + bbox.width / 2, bbox.y + bbox.height / 2);
            if (!prateleira.prateleiraArea.contains(centroContorno)) {
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

auto tempoUltimoAviso = chrono::steady_clock::now();
const chrono::seconds intervaloAviso(30);

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
            prateleira.atualizarContagem(result.marcasDetectadas);

            auto agora = chrono::steady_clock::now();
            static map<string, bool> alertaEnviado;

            if ((agora - tempoUltimoAviso) >= intervaloAviso) {
                for (const auto& [marca, qtd] : result.marcasDetectadas) {
                    if (marca != "Desconhecida") {
                        if (qtd == 0 && !alertaEnviado[marca]) {
                            string msg = "⚠️ *ATENÇÃO*: Falta o refrigerante *" + marca + "* na prateleira!";
                            enviarAlertaTelegram(msg);
                            alertaEnviado[marca] = true;
                        } else if (qtd > 0 && alertaEnviado[marca]) {
                            alertaEnviado[marca] = false;
                        }
                    }
                }
                tempoUltimoAviso = agora; // Atualiza relógio
            }


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
