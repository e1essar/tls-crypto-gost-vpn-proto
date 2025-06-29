#include "Client.h"
#include "Utils.h"
#include <openssl/ssl.h> // Для SSL/TLS
#include <openssl/err.h> // Для ошибок
#include <arpa/inet.h> // Для сокетов
#include <unistd.h> // Для close
#include <cstdio> // Для printf
#include <cstring> // Для memset

namespace tls {

Client::Client(ICipherStrategy* cs, IKeyStore* ks,
               const std::string& host, int port)
 : _cs(cs), _ks(ks), _host(host), _port(port) {} // Инициализация членов класса

bool Client::run() {
    SSL_library_init(); // Инициализирует OpenSSL
    OpenSSL_add_all_algorithms(); // Загружает все алгоритмы
    SSL_load_error_strings(); // Загружает строки ошибок

    const SSL_METHOD* meth = TLS_client_method(); // Использует TLS для клиента
    SSL_CTX* ctx = SSL_CTX_new(meth); // Создает контекст TLS
    if (!_cs->configureContext(ctx)) return false; // Настраивает шифрование

    // Включаем верификацию сертификата сервера
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    if (SSL_CTX_load_verify_locations(ctx, "certs/cert.pem", nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "Ошибка загрузки доверенного сертификата (certs/cert.pem)\n");
        SSL_CTX_free(ctx);
        return false;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0); // Создает TCP-сокет
    sockaddr_in addr{}; // Структура для адреса сервера
    addr.sin_family = AF_INET; // IPv4
    addr.sin_port = htons(_port); // Порт в сетевом порядке
    inet_pton(AF_INET, _host.c_str(), &addr.sin_addr); // Преобразует IP-адрес
    connect(sock, (sockaddr*)&addr, sizeof(addr)); // Подключается к серверу

    SSL* ssl = SSL_new(ctx); // Создает SSL-объект
    SSL_set_fd(ssl, sock); // Привязывает сокет к SSL
    if (SSL_connect(ssl) <= 0) { // Выполняет TLS-handshake
        ERR_print_errors_fp(stderr);
        return false;
    }

    int listener = socket(AF_INET, SOCK_STREAM, 0); // Создает локальный сокет
    sockaddr_in localAddr{}; // Структура для локального адреса
    localAddr.sin_family = AF_INET; // IPv4
    localAddr.sin_port = htons(8080); // Порт 8080
    localAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Локальный адрес
    bind(listener, (sockaddr*)&localAddr, sizeof(localAddr)); // Привязывает сокет
    listen(listener, 5); // Слушает до 5 соединений
    printf("Client listening on localhost:8080 for HTTP requests...\n");

    while (true) {
        int localSock = accept(listener, nullptr, nullptr); // Принимает соединение от браузера
        if (localSock < 0) break;

        char buf[8192]; // Буфер для запроса
        int n = read(localSock, buf, sizeof(buf)); // Читает HTTP-запрос
        std::string request;
        if (n > 0) {
            request.assign(buf, n); // Сохраняет запрос
            printf("\n[Client] Received HTTP request from browser:\n%s\n", request.c_str());
        }

        // Логируем отправку зашифрованного пакета серверу
        printf("[Client] Sending encrypted packet to server (%zu bytes over TLS)\n", request.size());
        if (!sendWithLength(ssl, request.data(), request.size())) { // Отправляет запрос серверу
            close(localSock);
            break;
        }

        std::string response; // Ответ от сервера
        // Логируем ожидание зашифрованного ответа
        printf("[Client] Waiting for encrypted response from server...\n");
        if (!receiveWithLength(ssl, response)) { // Получает ответ
            close(localSock);
            break;
        }
        printf("[Client] Received encrypted response from server (%zu bytes over TLS)\n", response.size());
        printf("[Client] Decrypted response from server:\n%s\n", response.c_str());

        write(localSock, response.data(), response.size()); // Отправляет ответ браузеру
        close(localSock); // Закрывает соединение с браузером
    }

    SSL_free(ssl); // Освобождает SSL-объект
    close(sock); // Закрывает сокет сервера
    SSL_CTX_free(ctx); // Освобождает контекст
    close(listener); // Закрывает локальный сокет
    return true; // Успех
}

} // namespace tls