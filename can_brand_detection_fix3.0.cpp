#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

using namespace cv;
using namespace std;

// Estrutura para gerenciar o estoque de refrigerantes
struct GerenciadorEstoque {
    map<string, int> quantidadeAtual;      // Quantidade atual de cada marca
    map<string, int> capacidadeMaxima;     // Capacidade máxima para cada marca
    
    // Construtor que define capacidades máximas padrão
    GerenciadorEstoque() {
        capacidadeMaxima["Guaraná"] = 50;
        capacidadeMaxima["Coca-Cola"] = 100;
        capacidadeMaxima["Pepsi"] = 75;
        capacidadeMaxima["Fanta Laranja"] = 60;
        capacidadeMaxima["Fanta Uva"] = 40;
        
        // Inicializa quantidades atuais como zero
        for (auto& par : capacidadeMaxima) {
            quantidadeAtual[par.first] = 0;
        }
    }
    
    // Adiciona uma unidade ao estoque da marca
    void adicionarRefrigerante(const string& marca) {
        if (marca != "Desconhecida" && capacidadeMaxima.find(marca) != capacidadeMaxima.end()) {
            if (quantidadeAtual[marca] < capacidadeMaxima[marca]) {
                quantidadeAtual[marca]++;
            }
        }
    }
    
    // Remove uma unidade do estoque da marca
    void removerRefrigerante(const string& marca) {
        if (marca != "Desconhecida" && quantidadeAtual.find(marca) != quantidadeAtual.end()) {
            if (quantidadeAtual[marca] > 0) {
                quantidadeAtual[marca]--;
            }
        }
    }
    
    // Retorna status do estoque para uma marca específica
    string getStatusEstoque(const string& marca) {
        if (capacidadeMaxima.find(marca) == capacidadeMaxima.end()) {
            return "Marca não cadastrada";
        }
        
        int atual = quantidadeAtual[marca];
        int maximo = capacidadeMaxima[marca];
        
        if (atual == maximo) {
            return "Estoque completo";
        } else if (atual < maximo * 0.3) {  // Menos de 30% = precisa reposição
            return "Precisa de reposição: " + to_string(atual) + "/" + to_string(maximo);
        } else {
            return "Quantidade atual: " + to_string(atual) + "/" + to_string(maximo);
        }
    }
    
    // Cria imagem com informações do estoque para exibição
    Mat criarJanelaEstoque() {
        Mat janela = Mat::zeros(400, 500, CV_8UC3);
        janela.setTo(Scalar(50, 50, 50));  // Fundo cinza escuro
        
        // Título
        putText(janela, "CONTROLE DE ESTOQUE", Point(50, 30), 
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
        
        int y = 70;
        for (auto& par : capacidadeMaxima) {
            string marca = par.first;
            string status = getStatusEstoque(marca);
            
            // Define cor do texto baseado no status
            Scalar cor;
            if (status == "Estoque completo") {
                cor = Scalar(0, 255, 0);  // Verde
            } else if (status.find("Precisa de reposição") != string::npos) {
                cor = Scalar(0, 0, 255);  // Vermelho
            } else {
                cor = Scalar(0, 255, 255);  // Amarelo
            }
            
            // Exibe marca e status
            putText(janela, marca + ":", Point(20, y), 
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 1);
            putText(janela, status, Point(20, y + 20), 
                    FONT_HERSHEY_SIMPLEX, 0.5, cor, 1);
            
            // Barra de progresso visual
            int barWidth = 200;
            int barHeight = 10;
            int atual = quantidadeAtual[marca];
            int maximo = capacidadeMaxima[marca];
            float porcentagem = (float)atual / maximo;
            
            // Barra de fundo
            rectangle(janela, Point(250, y - 10), Point(250 + barWidth, y), 
                     Scalar(100, 100, 100), -1);
            
            // Barra de progresso preenchida
            int preenchimento = (int)(barWidth * porcentagem);
            Scalar corBarra = (porcentagem > 0.7) ? Scalar(0, 255, 0) : 
                             (porcentagem > 0.3) ? Scalar(0, 255, 255) : Scalar(0, 0, 255);
            rectangle(janela, Point(250, y - 10), Point(250 + preenchimento, y), 
                     corBarra, -1);
            
            y += 60;
        }
        
        // Instruções
        putText(janela, "Teclas: 'r' = reset estoque, ESC = sair", 
                Point(20, 380), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(200, 200, 200), 1);
        
        return janela;
    }
    
    // Reseta todo o estoque para zero
    void resetarEstoque() {
        for (auto& par : quantidadeAtual) {
            par.second = 0;
        }
    }
};

// Função que detecta a cor dominante em uma região de interesse usando HSV
string detectarCorPredominante(const Mat& bgr_roi) {
    Mat hsv;
    // Converte BGR para HSV (melhor para detecção de cores)
    cvtColor(bgr_roi, hsv, COLOR_BGR2HSV);

    vector<Mat> hsv_channels;
    split(hsv, hsv_channels);  // Separa canais: H (matiz), S (saturação), V (brilho)

    if (hsv_channels.size() != 3) {
        return "Desconhecido";
    }

    // Aplica equalização adaptativa para melhorar contraste no canal de brilho
    Ptr<CLAHE> clahe = createCLAHE();
    clahe->setClipLimit(4.0);
    clahe->apply(hsv_channels[2], hsv_channels[2]);  // Equaliza o canal V (brilho)

    merge(hsv_channels, hsv);

    // Aplica filtro Gaussiano para reduzir ruído
    GaussianBlur(hsv, hsv, Size(5, 5), 0);

    // Cria máscara para filtrar pixels muito escuros (brilho entre 40-255)
    Mat brilhoMask;
    inRange(hsv_channels[2], 40, 255, brilhoMask);

    // Contador para armazenar quantidade de pixels de cada cor
    map<string, int> contador;

    // Define limites inferiores HSV para cada cor
    vector<pair<string, Scalar>> coresInferiores = {
        {"Verde", Scalar(35, 50, 40)},
        {"Vermelho1", Scalar(0, 70, 40)},      // Vermelho parte 1 (0-10°)
        {"Vermelho2", Scalar(170, 70, 40)},    // Vermelho parte 2 (170-180°)
        {"Azul", Scalar(90, 40, 40)},
        {"Laranja", Scalar(11, 100, 50)},
        {"Roxo", Scalar(130, 40, 40)}
    };

    // Define limites superiores HSV para cada cor
    vector<pair<string, Scalar>> coresSuperiores = {
        {"Verde", Scalar(85, 255, 255)},
        {"Vermelho1", Scalar(10, 255, 255)},
        {"Vermelho2", Scalar(180, 255, 255)},
        {"Azul", Scalar(130, 255, 255)},
        {"Laranja", Scalar(25, 255, 255)},
        {"Roxo", Scalar(160, 255, 255)}
    };

    // Para cada cor, conta pixels que estão dentro dos limites HSV
    for (size_t i = 0; i < coresInferiores.size(); ++i) {
        Mat mask;
        inRange(hsv, coresInferiores[i].second, coresSuperiores[i].second, mask);
        bitwise_and(mask, brilhoMask, mask);  // Aplica filtro de brilho
        int count = countNonZero(mask);
        contador[coresInferiores[i].first] += count;
    }

    // Combina as duas partes do vermelho (devido ao wrap-around no HSV)
    contador["Vermelho"] = contador["Vermelho1"] + contador["Vermelho2"];

    // Encontra a cor com maior número de pixels
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

// Função que mapeia cor detectada para marca de refrigerante
string classificarMarcaPorCor(const string& cor) {
    if (cor == "Verde") return "Guaraná";
    if (cor == "Vermelho") return "Coca-Cola";
    if (cor == "Azul") return "Pepsi";
    if (cor == "Laranja") return "Fanta Laranja";
    if (cor == "Roxo") return "Fanta Uva";
    return "Desconhecida";
}

int main() {
    // Inicializa captura de vídeo da câmera (índice 0)
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Erro ao abrir câmera" << endl;
        return -1;
    }

    // Cria janelas para exibir resultado
    namedWindow("Lata Detection", WINDOW_NORMAL);
    namedWindow("Controle de Estoque", WINDOW_NORMAL);
    
    // Inicializa gerenciador de estoque
    GerenciadorEstoque estoque;
    
    // Variáveis para controle de detecção (evita contagem múltipla)
    map<string, int> ultimaDeteccao;
    int framesSemDeteccao = 0;

    Mat frame;
    while (true) {
        cap >> frame;  // Captura frame da câmera
        if (frame.empty()) break;

        // Prepara imagem para detecção de bordas
        Mat gray, edges;
        cvtColor(frame, gray, COLOR_BGR2GRAY);  // Converte para escala de cinza
        GaussianBlur(gray, gray, Size(5, 5), 0);  // Remove ruído
        Canny(gray, edges, 50, 150);  // Detecta bordas usando algoritmo Canny

        // Encontra contornos na imagem de bordas
        vector<vector<Point>> contornos;
        findContours(edges, contornos, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        map<string, int> deteccoesAtuais;
        bool houveDeteccao = false;

        // Analisa cada contorno encontrado
        for (const auto& contorno : contornos) {
            Rect bbox = boundingRect(contorno);  // Cria retângulo delimitador

            // Filtros para identificar objetos similares a latas
            if (bbox.height < 100 || bbox.width < 30) continue;  // Tamanho mínimo
            float proporcao = (float)bbox.height / bbox.width;
            if (proporcao < 1.5 || proporcao > 4.0) continue;  // Proporção altura/largura

            // Define região do terço inferior da lata (onde geralmente está a marca)
            Rect tercoInferior(bbox.x, bbox.y + (2 * bbox.height / 3), bbox.width, bbox.height / 3);
            Rect frameRect(0, 0, frame.cols, frame.rows);
            Rect validROI = tercoInferior & frameRect;  // Garante que está dentro da imagem
            if (validROI.area() < tercoInferior.area() * 0.8) continue;

            // Extrai região de interesse e detecta cor dominante
            Mat roi = frame(validROI);
            string cor = detectarCorPredominante(roi);
            string marca = classificarMarcaPorCor(cor);

            if (marca != "Desconhecida") {
                deteccoesAtuais[marca]++;
                houveDeteccao = true;
            }

            // Desenha retângulo ao redor da lata detectada
            rectangle(frame, bbox, Scalar(0, 255, 0), 2);
            // Exibe nome da marca detectada
            putText(frame, marca, Point(bbox.x, bbox.y - 10), 
                    FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        }

        // Lógica para adicionar ao estoque (evita contagem múltipla)
        if (houveDeteccao) {
            framesSemDeteccao = 0;
            // Só adiciona se houve mudança significativa nas detecções
            for (auto& par : deteccoesAtuais) {
                if (ultimaDeteccao[par.first] != par.second) {
                    cout << "Nova detecção: " << par.first << " (quantidade: " << par.second << ")" << endl;
                    // Adiciona diferença ao estoque
                    int diferenca = par.second - ultimaDeteccao[par.first];
                    for (int i = 0; i < diferenca; i++) {
                        estoque.adicionarRefrigerante(par.first);
                    }
                }
            }
            ultimaDeteccao = deteccoesAtuais;
        } else {
            framesSemDeteccao++;
            // Limpa detecções antigas após alguns frames sem detecção
            if (framesSemDeteccao > 30) {
                ultimaDeteccao.clear();
            }
        }

        // Atualiza e exibe janela de estoque
        Mat janelaEstoque = estoque.criarJanelaEstoque();
        imshow("Controle de Estoque", janelaEstoque);

        // Exibe frame com detecções
        imshow("Lata Detection", frame);
        
        // Controle de teclado
        int key = waitKey(30);
        if (key == 27) break;  // ESC para sair
        if (key == 'r' || key == 'R') {  // 'r' para resetar estoque
            estoque.resetarEstoque();
            cout << "Estoque resetado!" << endl;
        }
        // Teclas para simular remoção manual do estoque
        if (key == '1') estoque.removerRefrigerante("Guaraná");
        if (key == '2') estoque.removerRefrigerante("Coca-Cola");
        if (key == '3') estoque.removerRefrigerante("Pepsi");
        if (key == '4') estoque.removerRefrigerante("Fanta Laranja");
        if (key == '5') estoque.removerRefrigerante("Fanta Uva");
    }

    // Libera recursos
    cap.release();
    destroyAllWindows();
    return 0;
}
