#!/bin/bash

# ==============================================================================
# Script Lançador para o Projeto de Detecção de Latas (Versão com CMake)
# ==============================================================================
# Este script foi gerado para ser executado a partir da pasta:
# /home/julio-cesar/projeto_SOE/pf/
#
# Ele assume que o código-fonte (C++ e CMakeLists.txt) está no diretório atual.
# ==============================================================================

# Sai imediatamente se qualquer comando falhar
set -e

# --- CONFIGURAÇÃO ---
# O nome do executável final, conforme definido no CMakeLists.txt
EXECUTAVEL="detector_final"
# A pasta onde a compilação ocorrerá
BUILD_DIR="build"

# --- INÍCIO DO SCRIPT ---

echo -e "\033[1;34m--- Lançador do Projeto do Detector de Latas ---\033[0m"

# --- 1. INSTALAÇÃO DE DEPENDÊNCIAS ---
echo -e "\n\033[1;32m[1/4] Verificando e instalando dependências...\033[0m"
# Atualiza a lista de pacotes e instala as ferramentas necessárias
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libopencv-dev curl

# --- 2. VERIFICAÇÃO MANUAL DA CÂMERA ---
echo -e "\n\033[1;33m[AVISO] Verificação Manual Necessária:\033[0m"
echo "Este script não pode habilitar a câmera do Raspberry Pi automaticamente."
echo "Certifique-se de que a câmera está habilitada em 'sudo raspi-config'."
echo "(Vá para 'Interface Options' -> 'Legacy Camera' -> 'Enable' e reinicie)"
read -p "A câmera já está habilitada? (s/n) " -n 1 -r
echo # Move para a próxima linha
if [[ ! $REPLY =~ ^[Ss]$ ]]
then
    echo -e "\033[1;31mPor favor, habilite a câmera e rode o script novamente. Abortando.\033[0m"
    exit 1
fi

# --- 3. COMPILAR O PROJETO COM CMAKE ---
echo -e "\n\033[1;32m[2/4] Limpando e compilando o projeto com CMake...\033[0m"

# Remove a pasta de build antiga para garantir uma compilação limpa
if [ -d "$BUILD_DIR" ]; then
    echo "Removendo pasta de compilação antiga: $BUILD_DIR/"
    rm -rf "$BUILD_DIR"
fi

# Cria a pasta de build, entra nela, configura e compila
echo "Criando pasta de compilação: $BUILD_DIR/"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configurando o projeto com 'cmake ..'"
cmake ..

echo "Compilando o projeto com 'make'..."
make

# --- 4. EXECUTAR O PROGRAMA ---
echo -e "\n\033[1;32m[3/4] Compilação concluída. Executando o programa...\033[0m"
echo -e "\033[1;33mPressione a tecla ESC na janela do programa para fechar e finalizar o script.\033[0m"

# Executa o programa a partir da pasta 'build'
./"$EXECUTAVEL"

# Retorna para o diretório original
cd ..
echo -e "\n\033[1;32m[4/4] Programa finalizado. Script concluído.\033[0m"
