#include "Server.h"
#include "Utils.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <netdb.h> // Добавьте, если ещё не добавлено

namespace tls {

Server::Server(ICipherStrategy* cs, IKeyStore* ks, int port,
               const std::string& certFile, const std::string& keyFile)
 : _cs(cs), _ks(ks), _port(port),
   _certFile(certFile), _keyFile(keyFile)
{}

bool Server::run() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD* meth = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(meth);
    if (!_cs->configureContext(ctx)) return false;

    if (!_ks->loadCertificate(ctx, _certFile)) return false;
    if (!_ks->loadPrivateKey(ctx, _keyFile)) return false;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return false;
    }
    if (listen(sock, 1) < 0) { perror("listen"); return false; }

    printf("Server listening on port %d...\n", _port);
    int client = accept(sock, nullptr, nullptr);
    if (client < 0) { perror("accept"); return false; }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        while (true) {
            std::string request;
            if (!receiveWithLength(ssl, request)) break;
            printf("\n[Server] Received HTTP request from client:\n%s\n", request.c_str());

            std::string host = getHostFromRequest(request);
            if (host.empty()) {
                printf("[Server] No Host header found\n");
                break;
            }
            printf("[Server] Connecting to target host: %s\n", host.c_str());

            // DNS-разрешение
            struct addrinfo hints{}, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(host.c_str(), "80", &hints, &res) != 0) {
                printf("[Server] DNS resolution failed for %s\n", host.c_str());
                continue;
            }

            int targetSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (connect(targetSock, res->ai_addr, res->ai_addrlen) < 0) {
                perror("[Server] connect to target");
                close(targetSock);
                freeaddrinfo(res);
                continue;
            }

            send(targetSock, request.data(), request.size(), 0);
            std::string response = readHttpResponse(targetSock);
            close(targetSock);
            printf("[Server] Sending response to client (%zu bytes):\n%s\n", response.size(), response.c_str());

            if (!sendWithLength(ssl, response.data(), response.size())) break;
            freeaddrinfo(res); // Освобождаем память
        }
    }

    SSL_free(ssl);
    close(client);
    close(sock);
    SSL_CTX_free(ctx);
    return true;
}

} // namespace tls