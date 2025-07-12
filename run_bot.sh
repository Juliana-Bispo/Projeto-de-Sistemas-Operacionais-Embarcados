#!/bin/bash

# ==============================================================================
# Script Lançador para o Projeto de Detecção de Latas
# ==============================================================================
# Este script foi gerado para ser executado a partir da pasta:
# /home/julio-cesar/projeto_SOE/pf/
#
# Ele assume que o código-fonte do projeto está na subpasta "Entrega_Final_SOE".
# ==============================================================================

# Sai imediatamente se qualquer comando falhar
set -e

# --- CONFIGURAÇÃO ---
# O nome da pasta que contém o código C++ e o Makefile
PROJETO_PASTA="pf"
# O nome do executável final, conforme definido no Makefile
EXECUTAVEL="detector_final"

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

# --- 3. VERIFICAR E COMPILAR O PROJETO ---
echo -e "\n\033[1;32m[2/4] Verificando a pasta do projeto e compilando...\033[0m"

# Verifica se a pasta do projeto existe no diretório atual
if [ ! -d "$PROJETO_PASTA" ]; then
    echo -e "\033[1;31mERRO: A pasta do projeto '$PROJETO_PASTA' não foi encontrada! \033[0m"
    echo "Certifique-se de que o script 'run_bot.sh' e a pasta '$PROJETO_PASTA' estão no mesmo diretório."
    exit 1
fi

# Entra na pasta do projeto
cd "$PROJETO_PASTA/teste_atualizado"

echo "Limpando compilações antigas com 'make clean'..."
make clean

echo "Compilando o projeto com 'make'..."
make

# --- 4. EXECUTAR O PROGRAMA ---
echo -e "\n\033[1;32m[3/4] Compilação concluída. Executando o programa...\033[0m"
echo -e "\033[1;33mPressione a tecla ESC na janela do programa para fechar e finalizar o script.\033[0m"

# Executa o programa
./"$EXECUTAVEL"

echo -e "\n\033[1;32m[4/4] Programa finalizado. Script concluído.\033[0m"
