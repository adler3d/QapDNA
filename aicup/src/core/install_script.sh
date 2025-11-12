#!/bin/bash

set -e

export DEBIAN_FRONTEND=noninteractive

# Обновление пакетов
apt-get update -y

# Сначала устанавливаем git и базовые зависимости
apt-get install -y -q \
    git \
    curl \
    wget \
    python3 \
    python3-pip

# Установка Node.js (официальный репозиторий)
curl -fsSL https://deb.nodesource.com/setup_lts.x | bash -
apt-get install -y nodejs

# Установка Docker
curl -fsSL https://get.docker.com | bash -

# Установка компиляторов (без лишних зависимостей)
apt-get install -y -q \
    g++ \
    clang

# Установка Emscripten через git (без системных пакетов)
cd /opt
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest

# Добавляем emsdk в PATH
echo 'source /opt/emsdk/emsdk_env.sh' >> /etc/profile
echo "alias ll='ls -lht --color=auto'" >> /etc/profile

# Добавление пользователя в группу docker
usermod -aG docker $USER

echo "=== Install Done ==="
echo "Installed versions:"
g++ --version | head -1
clang --version | head -1
node --version
docker --version