FROM ubuntu:22.04

# 1. Обновление и установка зависимостей
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    pkg-config \
    openssl \
    libengine-gost-openssl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 2. Копируем весь проект внутрь контейнера
WORKDIR /app
COPY . .

# 3. Создаем папку сборки и собираем проект
RUN mkdir build && cd build && cmake .. && make

# 4. Открываем порт (по умолчанию 4433)
EXPOSE 4433

# 5. Стартуем сервер (можно заменить на client при необходимости)
CMD ["./build/server"]
