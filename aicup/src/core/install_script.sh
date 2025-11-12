#!/bin/bash

set -e

export DEBIAN_FRONTEND=noninteractive

# Предварительная настройка debconf
{
echo "iptables-persistent iptables-persistent/autosave_v4 boolean false"
echo "iptables-persistent iptables-persistent/autosave_v6 boolean false"
} | debconf-set-selections

# Установка
apt-get update -y
apt-get install -y -q --no-install-recommends \
    g++ \
    clang++ \
    nodejs \
    npm \
    docker.io \
    docker-compose \
    git \
    python3

# Emscripten
git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk
cd /opt/emsdk
./emsdk install latest
./emsdk activate latest

# Права для docker
usermod -aG docker $USER

echo "all components installed!"