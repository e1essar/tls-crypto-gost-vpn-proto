#include "Client.h"
#include "Utils.h"
#include <openssl/ssl.h> 
#include <openssl/err.h> 
#include <arpa/inet.h> 
#include <unistd.h>
#include <cstdio> 
#include <cstring>

namespace tls {

Client::Client(ICipherStrategy* cs, IKeyStore* ks,
               const std::string& host, int port)
 : _cs(cs), _ks(ks), _host(host), _port(port) {}

bool Client::run() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD* meth = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(meth);
    if (!_cs->configureContext(ctx)) return false;

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    if (SSL_CTX_load_verify_locations(ctx, "certs/cert.pem", nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "Ошибка загрузки доверенного сертификата (certs/cert.pem)\n");
        SSL_CTX_free(ctx);
        return false;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0); 
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    inet_pton(AF_INET, _host.c_str(), &addr.sin_addr);
    connect(sock, (sockaddr*)&addr, sizeof(addr));

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(8080);
    localAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(listener, (sockaddr*)&localAddr, sizeof(localAddr));
    listen(listener, 5);
    printf("Client listening on localhost:8080 for HTTP requests...\n");

    while (true) {
        int localSock = accept(listener, nullptr, nullptr); 
        if (localSock < 0) break;

        char buf[8192];
        int n = read(localSock, buf, sizeof(buf));
        std::string request;
        if (n > 0) {
            request.assign(buf, n);
            printf("\n[Client] Received HTTP request from browser:\n%s\n", request.c_str());
        }

        printf("[Client] Sending encrypted packet to server (%zu bytes over TLS)\n", request.size());
        if (!sendWithLength(ssl, request.data(), request.size())) { 
            close(localSock);
            break;
        }

        std::string response;
        printf("[Client] Waiting for encrypted response from server...\n");
        if (!receiveWithLength(ssl, response)) {
            close(localSock);
            break;
        }
        printf("[Client] Received encrypted response from server (%zu bytes over TLS)\n", response.size());
        printf("[Client] Decrypted response from server:\n%s\n", response.c_str());

        write(localSock, response.data(), response.size());
        close(localSock);
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    close(listener);
    return true;
}

} // namespace tls