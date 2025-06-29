# TLS-CRYPTO-GOST-VPN-PROTO
TLS-CRYPTO-GOST-VPN-PROTO — прототип клиент-серверного приложения на C++ с использованием OpenSSL и ГОСТ-криптографии через движок `gost-engine`. Приложение реализует защищённое TLS-соединение с поддержкой российских криптографических алгоритмов (Магма, Кузнечик) и прокси-функциональность для HTTP-запросов.

## Описание
- **Server**: TLS-сервер, принимающий зашифрованные HTTP-запросы от клиента, перенаправляющий их на целевой хост (порт 80) и возвращающий ответы через TLS.
- **Client**: TLS-клиент, действующий как локальный HTTP-прокси (слушает на `localhost:8080`), отправляющий запросы серверу через TLS и возвращающий ответы браузеру.
- **GostCipher**: Реализация интерфейса `ICipherStrategy` для настройки TLS с ГОСТ-алгоритмами через OpenSSL ENGINE.
- **FileKeyStore**: Реализация интерфейса `IKeyStore` для загрузки сертификатов и ключей из PEM-файлов.
- **EngineLoader**: Реализация интерфейса `IEngineLoader` для загрузки движка `gost-engine`.

Приложение использует модульную архитектуру с интерфейсами `ICipherStrategy` и `IKeyStore`, что упрощает добавление новых алгоритмов шифрования и хранилищ ключей.

## Возможности
- Поддержка ГОСТ-шифров (Магма, Кузнечик и др.) через `gost-engine`.
- Выбор конкретного шифра или всех доступных (`--cipher any`).
- Настраиваемые параметры через командную строку: хост, порт, шифр, пути к сертификату и ключу.
- Логирование TLS-handshake и согласованного шифра.
- Прокси-функциональность: клиент пересылает HTTP-запросы через TLS, сервер перенаправляет их на целевые хосты.
- Добавление заголовка `X-Forwarded-For` с IP клиента.
- RAII-подход к управлению ресурсами OpenSSL и базовая обработка ошибок.

## 🗂 Структура проекта
```
tls-crypto-gost-vpn-proto/
├── include/               # Заголовочные файлы
│   ├── crypto/            # Интерфейсы и классы для шифрования (GostCipher)
│   ├── engine/            # Интерфейсы и загрузка OpenSSL ENGINE
│   ├── net/               # Клиент и сервер
│   └── storage/           # Интерфейс и реализация хранилища ключей
├── src/                   # Исходный код
│   ├── main_client.cpp    # Запуск TLS-клиента
│   ├── main_server.cpp    # Запуск TLS-сервера
│   ├── crypto/            # Реализация ГОСТ-шифрования
│   ├── engine/            # Реализация загрузки движков
│   ├── net/               # Реализация клиента и сервера
│   └── storage/           # Реализация файлового хранилища ключей
├── certs/                 # Сертификаты и ключи (PEM)
│   ├── cert.pem           # Сертификат сервера
│   └── key.pem            # Приватный ключ
├── CMakeLists.txt         # Скрипт для сборки через CMake
├── Dockerfile             # Docker-образ (в разработке)
├── func_test.log          # логи тестирования функциональности клиента и сервера
```

## 🔧 Сборка вручную (без Docker)
### 📋 Требования
- CMake 3.10+
- Компилятор C++11 (GCC, Clang)
- OpenSSL с установленным `gost-engine`
- Linux (POSIX sockets)

### 🔑 Генерация ГОСТ-ключа и сертификата
```bash
openssl genpkey -engine gost -algorithm GOST2012_256 -pkeyopt paramset:A -out certs/key.pem
openssl req -engine gost -new -x509 -key certs/key.pem -out certs/cert.pem -days 365 -subj "/CN=localhost"
```

### 📦 Сборка
```bash
mkdir build
cd build
cmake ..
make
```

### 🚀 Быстрый запуск
- В отдельной вкладке ```./build/server --port 4433 --cert certs/cert.pem --key certs/key.pem```
- В другой вкладке ```./build/client --host 127.0.0.1 --port 4433 --cipher GOST2012-KUZNYECHIK-KUZNYECHIKOMAC```
- Настроить в браузере proxy на localhost:8080
- Отправить запрос на example.com

### Тестирование
#### Сервер
```bash
$ ./build/server --port 4433
Starting server on port 4433 with cipher 'any'
Configuring GOST cipher list: GOST2012-MAGMA-MAGMAOMAC:GOST2012-KUZNYECHIK-KUZNYECHIKOMAC:...
Server listening on port 4433...
Negotiated cipher: GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
[Server] Received encrypted HTTP request from client (123 bytes over TLS)
[Server] Decrypted HTTP request from client: GET / HTTP/1.1...
[Server] Connecting to target host: example.com
[Server] Sending HTTP request to target server (143 bytes, not encrypted)
[Server] Received HTTP response from target server (2048 bytes, not encrypted)
[Server] Sending encrypted response to client (2048 bytes over TLS)
```
#### Клиент
```bash
$ ./build/client --host 127.0.0.1 --port 4433 --cipher GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Starting client to 127.0.0.1:4433 with cipher 'GOST2012-KUZNYECHIK-KUZNYECHIKOMAC'
Configuring GOST cipher list: GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Negotiated cipher: GOST2012-KUZNYECHIK-KUZNYECHIKOMAC
Client listening on localhost:8080 for HTTP requests...
[Client] Received HTTP request from browser: GET / HTTP/1.1...
[Client] Sending encrypted packet to server (123 bytes over TLS)
[Client] Waiting for encrypted response from server...
[Client] Received encrypted response from server (2048 bytes over TLS)
[Client] Decrypted response from server: HTTP/1.1 200 OK...
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
- TLS-соединение с ГОСТ-алгоритмами через gost-engine.
- Модульная архитектура с интерфейсами ICipherStrategy и IKeyStore.
- HTTP-прокси с передачей запросов через TLS.
- Поддержка заголовка X-Forwarded-For.
- Логирование и обработка ошибок OpenSSL.

### 💡 Планируется:
- Полноценная VPN-функциональность (туннелирование трафика).
- Многопоточность для обработки нескольких клиентов.
- Поддержка других протоколов (не только HTTP).
- Реализация хранилища ключей на основе базы данных (DbKeyStore).
- Динамическое обновление сертификатов и ключей.
- Расширение поддержки cipher-suite.
- Добавление GUI для управления соединением.


## Ошибки и фиксы
### 1. Изменение конфига openssl после установки gost-engine
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
### 2. Ошибка загрузки сертификата
Если возникает ошибка при загрузке certs/cert.pem, убедитесь, что файл существует и имеет корректный PEM-формат.
