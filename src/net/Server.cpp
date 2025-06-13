// src/net/Server.cpp
#include "Server.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

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

    // Загружаем cert и key внутри run()
    if (!_ks->loadCertificate(ctx, _certFile)) return false;
    if (!_ks->loadPrivateKey(ctx,  _keyFile)) return false;

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
        char buf[256];
        int len;
        while ((len = SSL_read(ssl, buf, sizeof(buf)-1)) > 0) {
            buf[len] = 0;
            printf("Client: %s", buf);
            SSL_write(ssl, buf, len);
        }
    }

    SSL_free(ssl);
    close(client);
    close(sock);
    SSL_CTX_free(ctx);
    return true;
}

} // namespace tls
