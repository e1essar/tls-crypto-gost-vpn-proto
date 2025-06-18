// src/net/Client.cpp
#include "Client.h"
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

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE,nullptr);

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

    char buf[256];
    while (fgets(buf, sizeof(buf), stdin)) {
        SSL_write(ssl, buf, strlen(buf));
        int n = SSL_read(ssl, buf, sizeof(buf)-1);
        if (n <= 0) break;
        buf[n] = 0;
        printf("Server: %s", buf);
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return true;
}

} // namespace tls
