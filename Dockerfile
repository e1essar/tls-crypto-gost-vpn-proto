# ---------- БАЗОВЫЙ ОБРАЗ ----------
FROM ubuntu:22.04

# Отключаем диалоги APT
ENV DEBIAN_FRONTEND=noninteractive

# ---------- УСТАНАВЛИВАЕМ ЗАВИСИМОСТИ ----------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential \
      git \
      cmake \
      pkg-config \
      libssl-dev \
      openssl \
      ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ---------- КЛОНИРУЕМ И СБИРАЕМ gost-engine ----------
RUN git clone https://github.com/gost-engine/engine.git /opt/engine && \
    cd /opt/engine && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DOPENSSL_ROOT_DIR=/usr \
      -DOPENSSL_ENGINES_DIR=/usr/lib/ssl/engines \
      .. && \
    cmake --build . --target install

# ---------- НАСТРАИВАЕМ openssl.conf для GOST ----------
RUN printf '\
openssl_conf = openssl_init\n\
\n\
[openssl_init]\n\
engines = engine_section\n\
\n\
[engine_section]\n\
gost = gost_section\n\
\n\
[gost_section]\n\
engine_id = gost\n\
dynamic_path = /usr/lib/ssl/engines/gost.so\n\
default_algorithms = ALL\n' \
> /etc/ssl/openssl.cnf

# ---------- WORKDIR и CMD ----------
WORKDIR /opt/engine
CMD ["/bin/bash"]
