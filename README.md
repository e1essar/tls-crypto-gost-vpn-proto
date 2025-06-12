# TLS-GOST-APP
TLS-сервер и клиент с поддержкой ГОСТ-криптографии на базе OpenSSL 3 и ENGINE API
Используется собственный криптографический движок (engine), реализующий ГОСТ-алгоритмы.

## 🗂 Структура проекта
```
tls-gost-app/
├── include/               # Заголовочные файлы
│   ├── crypto/            # Интерфейсы и обёртки для шифрования (ГОСТ)
│   ├── engine/            # Интерфейсы и загрузка OpenSSL engine
│   └── storage/           # Интерфейс хранилища ключей
│
├── src/                   # Исходники
│   ├── main_server.cpp    # Основной TLS-сервер
│   ├── main_client.cpp    # Простой TLS-клиент
│   ├── engine/            # Реализация загрузки engine
│   ├── crypto/            # Реализация ГОСТ-шифрования через ENGINE
│   └── storage/           # Простое файловое хранилище ключей
│
├── certs/                 # TLS-сертификаты и ключи (PEM)
│   ├── cert.pem
│   └── key.pem
│
├── examples/              # Скрипты запуска без docker
│   ├── start_server.sh
│   └── start_client.sh
│
├── CMakeLists.txt         # CMake-сборка
├── Dockerfile             # Docker-образ
└── docker-compose.yml     # Композиция клиент + сервер
```

## 🔧 Сборка вручную (без docker)
### 📋 Зависимости
- cmake >= 3.16
- g++ с поддержкой C++17
- openssl >= 3.0 с включённым ENGINE API
- libssl-dev, libcrypto++-dev (если нужно)

### 🔧 Генерация GOST‑ключа и сертификата (certs/)
```bash
openssl genpkey -engine gost -algorithm GOST2012_256 -pkeyopt paramset:A -out key.pem
openssl req -engine gost -new -x509 -key key.pem -out cert.pem -days 365 -subj "/CN=localhost"
```

### 📦 Сборка
```bash
mkdir build
cd build
cmake ..
make
```

После сборки появятся бинарники:
```
./build/server
./build/client
```

### 🚀 Быстрый запуск
- В отдельной вкладке ```./build/server```
- В другой вкладке ```./build/client 127.0.0.1 4433```

## 🐳 Сборка и запуск в Docker
### 🔨 Сборка + запуск
```bash
docker-compose up --build
```
### В процессе:
- Сервер поднимается на tls-server:4433
- Клиент подключается и производит TLS handshake с ГОСТ-криптографией

### 🧪 Пример вывода
```
[*] Connecting to gost-server:4433...
[*] TLS handshake successful
[*] Cipher in use: GOST2001-GOST89-GOST89
```

## 🛠 Патчи
### ✅ Что уже реализовано:
- TLS-соединение с использованием ГОСТ-алгоритмов
- Собственный EngineLoader через OpenSSL ENGINE API
- Простая реализация ICipherStrategy (ГОСТ)
- Хранилище ключей IKeyStore (файловое)

### 💡 Что можно добавить:
- UI или CLI	Интерактивное управление сертификатами / соединением
- Поддержка ГОСТ TLS 1.2	Расширить поддержку ciphersuites
- Асинхронный TLS-сервер	Использовать boost::asio или libevent
- Хранилище в БД	Реализация DbKeyStore (PostgreSQL, SQLite и т.п.)
- Автоматическое обновление ключей	Подключить генератор сертификатов, CRL
- Механизмы авторизации	JWT / Cert-based Access Control

## Ошибки и фиксы
### 1.
Открой файл /etc/ssl/openssl.cnf (или смотри OPENSSL_CONF):
Добавь в конец:
```
openssl_conf = openssl_init

[openssl_init]
engines = engine_section

[engine_section]
gost = gost_section

[gost_section]
engine_id = gost
dynamic_path = /usr/lib/x86_64-linux-gnu/engines-3/gost.so
default_algorithms = ALL
```
