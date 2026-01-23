# tls-crypto-gost-vpn-proto

Прототип защищенного канала поверх **TLS 1.3** на C++ (OpenSSL 3.x), где для рукопожатия и шифрования используются **ГОСТ‑наборы шифров TLS 1.3** через **GOST provider** (`gostprov`/`gost`).

Идея: **клиент** и **сервер** поднимают TUN‑интерфейсы и пересылают **сырые IP‑пакеты** между ними, инкапсулируя их в TLS‑канал. На серверной стороне возможно включение NAT/форвардинга, чтобы клиентский трафик выходил в интернет через сервер.

---

## Архитектура

- **TLS‑Server (`server`)**
  - Слушает TCP‑порт.
  - Поднимает TLS 1.3 (ГОСТ ciphersuites).
  - Получает из TLS кадры вида: `[len32][ip_packet_bytes...]`.
  - Пишет содержимое в **TUN**.
  - Читает пакеты из TUN и отправляет обратно в TLS тем же форматом.

- **TLS‑Client (`client`)**
  - Подключается к серверу по TCP.
  - Поднимает TLS 1.3 (ГОСТ ciphersuites).
  - Поднимает локальный **TUN**.
  - Читает IP‑пакеты из TUN → отправляет в TLS.
  - Получает пакеты из TLS → пишет в TUN.

- **Crypto layer**
  - `GostCipher` настраивает TLS‑контекст: TLS1.3 only + список ГОСТ ciphersuites.
  - `ProviderLoader` загружает провайдеры OpenSSL (`default` + `gostprov` или `gost`).

- **Key storage**
  - `FileKeyStore` загружает сертификат/ключ из PEM.

---

## Требования

- Linux (используется `/dev/net/tun`, `ioctl(TUNSETIFF)`).
- Права: **root** или capabilities (минимум `CAP_NET_ADMIN` для работы с TUN и маршрутизацией).
- CMake **3.16+**.
- Компилятор C++ (GCC/Clang), стандарт **C++11**.
- OpenSSL **3.x**.
- Для запуска именно в режиме ГОСТ TLS 1.3 нужен **GOST provider** (обычно модуль `gostprov.so` или `gost.so`).

> Проект представлен в демо-режиме, поэтому некоторые моменты выключены в целях отладки

---

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Бинарники появятся в `build/server` и `build/client`.

---

## Подготовка OpenSSL + GOST provider

В репозитории есть скрипт, который автоматизирует сборку и окружение (может занять время и потребует зависимостей):

```bash
./scripts/setup.sh
# затем откройте новый shell или:
source ~/.gost-env.sh
```

Скрипт пытается создать `~/.gost-env.sh` и прописать переменные окружения:

- `OPENSSL_ROOT` – префикс OpenSSL
- `OPENSSL_MODULES` – каталог с `gostprov.so`/`gost.so`
- `LD_LIBRARY_PATH` – чтобы подхватились libcrypto/libssl из этого префикса

Проверка, что провайдер реально грузится:

```bash
openssl list -providers -provider-path "$OPENSSL_MODULES" -provider gostprov -provider default -verbose
```

---

## Сертификаты

В `certs/` лежит демонстрационный **самоподписанный** сертификат и приватный ключ.

- `certs/cert.pem`
- `certs/key.pem`

Для реальных сценариев ключ в репозитории хранить нельзя — здесь он оставлен **только для воспроизводимости демонстрации**.

---

## Запуск демо (туннель через TLS)

На практике удобнее запускать на **двух ВМ** или двух хостах (клиент/сервер). Скрипты в `scripts/` делают почти всё автоматически: поднимают TUN, настраивают IP, включают форвардинг и NAT на сервере.

### 1) Сервер

```bash
sudo ./scripts/server.sh up
```

Полезные переменные окружения (переопределяют дефолты):

- `SERVER_BIN` (по умолчанию `./build/server`)
- `CERT_PATH`, `KEY_PATH` (по умолчанию `./certs/...`)
- `PORT` (4433)
- `CIPHER` (`any` или конкретная suite)
- `TUN_IF` (tun0)
- `SRV_IP` (10.8.0.1/24)
- `NET_VPN` (10.8.0.0/24)
- `WAN` (внешний интерфейс; если авто‑детект не сработал)

### 2) Клиент

```bash
sudo SERVER_ADDR=<IP_сервера> ./scripts/client.sh up
```

Полезные переменные:

- `CLIENT_BIN` (по умолчанию `./build/client`)
- `SERVER_ADDR` (обязательно указать IP сервера)
- `PORT`, `CIPHER`, `TUN_IF`
- `CLT_IP` (10.8.0.2/24)
- `NET_VPN` (10.8.0.0/24)

### 3) Проверка

На клиенте:

```bash
sudo ./scripts/client.sh ping
```

Ожидаемо:
- ping `10.8.0.1` проходит (сервер внутри VPN‑подсети)
- при корректном NAT/форвардинге сервером возможен ping внешних адресов

---

## Параметры `server` и `client`

### server

```bash
./build/server \
  --port 4433 \
  --cipher any \
  --cert certs/cert.pem \
  --key  certs/key.pem \
  --tun  tun0
```

### client

```bash
./build/client \
  --host 10.0.0.1 \
  --port 4433 \
  --cipher any \
  --tun tun0
```

## Поддерживаемые ГОСТ ciphersuites

Список зашит в `GostCipher::supportedSuites()`:

- `TLS_GOSTR341112_256_WITH_MAGMA_MGM_L`
- `TLS_GOSTR341112_256_WITH_MAGMA_MGM_S`
- `TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_L`
- `TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_S`

`--cipher any` включает их все (через `:` в `SSL_CTX_set_ciphersuites`).

---

## Структура проекта

```
.
├── CMakeLists.txt
├── include/
│   ├── crypto/        # ICipherStrategy + GostCipher
│   ├── net/           # Client/Server + Tun + framing Utils
│   ├── provider/      # IProviderLoader + ProviderLoader (OpenSSL providers)
│   └── storage/       # IKeyStore + FileKeyStore
├── src/
│   ├── crypto/
│   ├── net/
│   ├── provider/
│   ├── storage/
│   ├── main_client.cpp
│   └── main_server.cpp
├── scripts/
│   ├── setup.sh       # первичная установка и подгрузка зависимостей
│   ├── server.sh      # поднять серверную часть (TUN+NAT)
│   ├── client.sh      # поднять клиентскую часть (TUN+routes)
│   └── load.sh        # загрузка окружений после перезапуска системы
└── certs/
    ├── cert.pem
    └── key.pem
```

> В каталоге `src/engine` / `include/engine` есть legacy‑пример работы через OpenSSL ENGINE; текущий запуск ориентирован на OpenSSL 3 providers.

---

## Лицензия

Проект предназначен для образовательных целей (дипломная работа).
