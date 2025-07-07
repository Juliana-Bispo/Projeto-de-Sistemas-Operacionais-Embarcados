# Makefile para o projeto de detecção

# Compilador e flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -mcpu=cortex-a53
LDFLAGS = -lpthread

# Pacote OpenCV
OPENCV = `pkg-config --cflags --libs opencv4`

# Arquivos fonte (.cpp) e objeto (.o)
SRCS = main.cpp GerenciadorPrateleira.cpp processamento.cpp pipeline.cpp
OBJS = $(SRCS:.cpp=.o)

# Nome do executável final
TARGET = detector_final

# Regra principal: criar o executável
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS) $(OPENCV)

# Regra para compilar cada .cpp em um .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(OPENCV)

# Regra para limpar os arquivos gerados
clean:
	rm -f $(OBJS) $(TARGET)