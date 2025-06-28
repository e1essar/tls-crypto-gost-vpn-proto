#include "Server.h"
#include "Utils.h"
#include <openssl/ssl.h> // Для SSL/TLS
#include <openssl/err.h> // Для ошибок
#include <netinet/in.h> // Для сокетов
#include <unistd.h> // Для close
#include <cstdio> // Для printf
#include <cstring> // Для memset
#include <netdb.h> // Для getaddrinfo
#include <sys/socket.h> // Для getpeername
#include <arpa/inet.h> // Для inet_ntoa

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

    // Получаем IP клиента
    sockaddr_in peer_addr{};
    socklen_t peer_len = sizeof(peer_addr);
    getpeername(client, (sockaddr*)&peer_addr, &peer_len);
    std::string clientIp = inet_ntoa(peer_addr.sin_addr);

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

            // Добавляем/заменяем X-Forwarded-For
            std::string modRequest = addOrReplaceXForwardedFor(request, clientIp);

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

            send(targetSock, modRequest.data(), modRequest.size(), 0); // Отправляет модифицированный запрос хосту
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

// Вспомогательная функция для добавления/замены X-Forwarded-For
std::string addOrReplaceXForwardedFor(const std::string& request, const std::string& clientIp) {
    std::string out;
    size_t pos = 0;
    size_t xffPos = request.find("X-Forwarded-For:");
    size_t headersEnd = request.find("\r\n\r\n");
    if (headersEnd == std::string::npos) headersEnd = request.size();
    if (xffPos != std::string::npos && xffPos < headersEnd) {
        // Заменить существующий X-Forwarded-For
        size_t lineEnd = request.find("\r\n", xffPos);
        out = request.substr(0, xffPos);
        out += "X-Forwarded-For: " + clientIp + "\r\n";
        if (lineEnd != std::string::npos)
            out += request.substr(lineEnd + 2);
    } else {
        // Вставить X-Forwarded-For после Host
        size_t hostPos = request.find("Host:");
        if (hostPos != std::string::npos) {
            size_t hostEnd = request.find("\r\n", hostPos);
            if (hostEnd != std::string::npos) {
                out = request.substr(0, hostEnd + 2);
                out += "X-Forwarded-For: " + clientIp + "\r\n";
                out += request.substr(hostEnd + 2);
            } else {
                out = request + "X-Forwarded-For: " + clientIp + "\r\n";
            }
        } else {
            out = request + "X-Forwarded-For: " + clientIp + "\r\n";
        }
    }
    return out;
}

} // namespace tls