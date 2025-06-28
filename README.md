# TLS-CRYPTO-GOST-VPN-PROTO
TLS-CRYPTO-GOST-VPN-PROTO - прототип клиент‑серверного приложения на C++ с использованием OpenSSL и GOST‑криптографии через движок gost-engine.

## Описание
- Server — TLS‑сервер, возвращающий клиенту посланные данные (echo).
- Client — TLS‑клиент, отправляющий строки с stdin на сервер и выводящий ответы.
- GostCipher — стратегия шифрования для GOST через OpenSSL ENGINE.
- FileKeyStore — загрузка сертификата и приватного ключа из файлов PEM.

Приложение использует интерфейсы ICipherStrategy и IKeyStore, что упрощает добавление новых алгоритмов и хранилищ ключей.

## Возможности
- Поддержка нескольких GOST cipher‑suite (автоматически все по умолчанию или выбор конкретного).
- Настраиваемые параметры через командную строку: порт, адрес, пути к сертификату и ключу, выбор cipher.
- Логирование согласованного шифра после Handshake.
- RAII‑подход к OpenSSL-ресурсам и базовая обработка ошибок.

## 🗂 Структура проекта
```
tls-crypto-gost-vpn-proto/
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
### 📋 Требования
- CMake 3.10+
- Компилятор C++11 (GCC, Clang)
- OpenSSL с поддержкой GOST ENGINE
- Linux (POSIX sockets)

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

### 🚀 Быстрый запуск
- В отдельной вкладке ```./build/server --port 4433```
- В другой вкладке ```./build/client --host 127.0.0.1 --port 4433 --cipher GOST2012-KUZNYECHIK-KUZNYECHIKOMAC```
- Настроить в браузере proxy на localhost:8080
- Отправить запрос на example.com

### Тестирование
#### Сервер
```bash
└─$ ./server --port 4433
Starting server on port 4433 with cipher 'any'
Configuring GOST cipher list: GOST2012-MAGMA-MAGMAOMAC:GOST2012-KUZNYECHIK-KUZNYECHIKOMAC:LEGACY-GOST2012-GOST8912-GOST8912:IANA-GOST2012-GOST8912-GOST8912:GOST2001-GOST89-GOST89
Server listening on port 4433...
Negotiated cipher: GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Client: Hello
Hello
Client: How are you?
Good
Client: Ok
```
#### Клиент
```bash
└─$ ./client --host 127.0.0.1 --port 4433 --cipher GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Starting client to 127.0.0.1:4433 with cipher 'GOST2012-KUZNYECHIK-KUZNYECHIKOMAC'
Configuring GOST cipher list: GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Negotiated cipher: GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Hello
Server: Hello
How are you?
Server: How are you?
Ok 
Server: Ok

```

## Поддерживаемые GOST cipher-suite
- GOST2012-MAGMA-MAGMAOMAC
- GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
- LEGACY-GOST2012-GOST8912-GOST8912
- IANA-GOST2012-GOST8912-GOST8912
- GOST2001-GOST89-GOST89

Можно расширить список в GostCipher::supportedSuites().

## 🐳 Сборка и запуск в Docker (в разработке...)
### 🔨 Сборка + запуск
```bash
docker-compose up --build
```

## 🛠 Патчинг
### ✅ Что уже реализовано:
- TLS-соединение с использованием ГОСТ-алгоритмов
- EngineLoader через OpenSSL GOST-ENGINE
- Реализация ICipherStrategy (ГОСТ)
- Хранилище ключей IKeyStore (файловое)

### 💡 Планируется:
- Добавление VPN-функциональности (туннелирование)
- Настройка и тестирование отправки пакетов через TLS-туннель
- Добавление GUI
- Расширение модульности алгоритмов шифрования
- Интерактивное управление сертификатами / соединением
- Расширение поддержки ciphersuites
- Реализация хранилища ключей DbKeyStore
- Автоматическое обновление ключей


## Ошибки и фиксы
### 1. Изменение конфига openssl для gost-engine
Открой файл /etc/ssl/openssl.cnf (или смотри OPENSSL_CONF):
Добавить в конец:
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
