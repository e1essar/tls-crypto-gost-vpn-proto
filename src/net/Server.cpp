#include "Server.h"
#include "Utils.h"
#include <openssl/ssl.h> // Для SSL/TLS
#include <openssl/err.h> // Для ошибок
#include <netinet/in.h> // Для сокетов
#include <unistd.h> // Для close
#include <cstdio> // Для printf
#include <cstring> // Для memset
#include <netdb.h> // Для getaddrinfo

namespace tls {

Server::Server(ICipherStrategy* cs, IKeyStore* ks, int port,
               const std::string& certFile, const std::string& keyFile)
 : _cs(cs), _ks(ks), _port(port), _certFile(certFile), _keyFile(keyFile) {}

bool Server::run() {
    SSL_library_init(); // Инициализирует OpenSSL
    OpenSSL_add_all_algorithms(); // Загружает алгоритмы
    SSL_load_error_strings(); // Загружает строки ошибок

    const SSL_METHOD* meth = TLS_server_method(); // Использует TLS для сервера
    SSL_CTX* ctx = SSL_CTX_new(meth); // Создает контекст TLS
    if (!_cs->configureContext(ctx)) return false; // Настраивает шифрование

    if (!_ks->loadCertificate(ctx, _certFile)) return false; // Загружает сертификат
    if (!_ks->loadPrivateKey(ctx, _keyFile)) return false; // Загружает ключ

    int sock = socket(AF_INET, SOCK_STREAM, 0); // Создает TCP-сокет
    if (sock < 0) { perror("socket"); return false; }

    sockaddr_in addr{}; // Структура для адреса
    addr.sin_family = AF_INET; // IPv4
    addr.sin_port   = htons(_port); // Порт
    addr.sin_addr.s_addr = INADDR_ANY; // Любой адрес

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) { // Привязывает сокет
        perror("bind"); return false;
    }
    if (listen(sock, 1) < 0) { perror("listen"); return false; } // Слушает соединения

    printf("Server listening on port %d...\n", _port);
    int client = accept(sock, nullptr, nullptr); // Принимает клиента
    if (client < 0) { perror("accept"); return false; }

    SSL* ssl = SSL_new(ctx); // Создает SSL-объект
    SSL_set_fd(ssl, client); // Привязывает сокет
    if (SSL_accept(ssl) <= 0) { // Выполняет TLS-handshake
        ERR_print_errors_fp(stderr);
    } else {
        while (true) {
            std::string request; // Запрос от клиента
            if (!receiveWithLength(ssl, request)) break; // Получает запрос
            printf("\n[Server] Received HTTP request from client:\n%s\n", request.c_str());

            std::string host = getHostFromRequest(request); // Извлекает хост
            if (host.empty()) {
                printf("[Server] No Host header found\n");
                break;
            }
            printf("[Server] Connecting to target host: %s\n", host.c_str());

            struct addrinfo hints{}, *res; // Для DNS-разрешения
            memset(&hints, 0, sizeof(hints)); // Обнуляет структуру
            hints.ai_family = AF_INET; // IPv4
            hints.ai_socktype = SOCK_STREAM; // TCP
            if (getaddrinfo(host.c_str(), "80", &hints, &res) != 0) { // Разрешает имя хоста
                printf("[Server] DNS resolution failed for %s\n", host.c_str());
                continue;
            }

            int targetSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Создает сокет для хоста
            if (connect(targetSock, res->ai_addr, res->ai_addrlen) < 0) { // Подключается к хосту
                perror("[Server] connect to target");
                close(targetSock);
                freeaddrinfo(res);
                continue;
            }

            send(targetSock, request.data(), request.size(), 0); // Отправляет запрос хосту
            std::string response = readHttpResponse(targetSock); // Читает ответ
            close(targetSock); // Закрывает сокет хоста
            printf("[Server] Sending response to client (%zu bytes):\n%s\n", response.size(), response.c_str());

            if (!sendWithLength(ssl, response.data(), response.size())) break; // Отправляет ответ клиенту
            freeaddrinfo(res); // Освобождает память
        }
    }

    SSL_free(ssl); // Освобождает SSL
    close(client); // Закрывает сокет клиента
    close(sock); // Закрывает сокет сервера
    SSL_CTX_free(ctx); // Освобождает контекст
    return true; // Успех
}

} // namespace tls