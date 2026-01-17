#include "net/Tun.h"
#include "net/Server.h"
#include "net/Utils.h"
#include "crypto/GostCipher.h"
#include "storage/FileKeyStore.h"
#include "provider/ProviderLoader.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdio>
#include <netinet/ip.h>
#include <arpa/inet.h>

static void log_ip_packet(const uint8_t* data, size_t len, const char* tag) {
    if (len < sizeof(iphdr)) {
        printf("[%s] short/non-ip len=%zu\n", tag, len);
        return;
    }
    const iphdr* ip = reinterpret_cast<const iphdr*>(data);
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    in_addr s{ip->saddr}, d{ip->daddr};
    inet_ntop(AF_INET, &s, src, sizeof(src));
    inet_ntop(AF_INET, &d, dst, sizeof(dst));
    printf("[%s] IPv4 proto=%u %s -> %s len=%zu\n", tag, ip->protocol, src, dst, len);
}

namespace tls {

static int tcp_listen(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int on = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); close(s); return -1; }
    if (listen(s, 8) < 0) { perror("listen"); close(s); return -1; }
    return s;
}


Server::Server(ICipherStrategy* cs, IKeyStore* ks, int port,
               const std::string& certFile, const std::string& keyFile,
               const std::string& tunName)
: _cs(cs), _ks(ks), _port(port),
  _certFile(certFile), _keyFile(keyFile), _tunName(tunName) {}

bool Server::run() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    if (!_cs->configureContext(ctx)) { SSL_CTX_free(ctx); return false; }

    if (!_ks->loadCertificate(ctx, _certFile)) { SSL_CTX_free(ctx); return false; }
    if (!_ks->loadPrivateKey(ctx, _keyFile))   { SSL_CTX_free(ctx); return false; }

    int ls = tcp_listen(_port);
    if (ls < 0) { SSL_CTX_free(ctx); return false; }
    printf("[server] listening on %d\n", _port);

    int cs = accept(ls, nullptr, nullptr);
    if (cs < 0) { perror("accept"); close(ls); SSL_CTX_free(ctx); return false; }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, cs);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl); close(cs); close(ls); SSL_CTX_free(ctx); return false;
    }

    printf("[server] TLS accepted\n");
    printf("[server][TLS] version=%s cipher=%s\n", SSL_get_version(ssl), SSL_get_cipher_name(ssl));

    Tun tun(_tunName);
    printf("[server] TUN ready: %s\n", tun.ifname().c_str());

    std::atomic<bool> running{true};

    std::thread t1([&] {
        std::string frame;
        while (running.load()) {
            if (!receiveWithLength(ssl, frame)) {
                fprintf(stderr, "[server] recv fail\n"); running=false; break;
            }
            log_ip_packet(reinterpret_cast<const uint8_t*>(frame.data()), frame.size(), "S TLS->TUN");
            if (tun.writePacket(reinterpret_cast<const uint8_t*>(frame.data()), frame.size()) != (ssize_t)frame.size()) {
                perror("[server] write(TUN)"); running=false; break;
            }
        }
    });

    std::thread t2([&] {
        std::vector<uint8_t> buf(20000);
        while (running.load()) {
            ssize_t n = tun.readPacket(buf.data(), buf.size());
            if (n <= 0) { perror("[server] read(TUN)"); running=false; break; }
            log_ip_packet(buf.data(), (size_t)n, "S TUN->TLS");
            if (!sendWithLength(ssl, buf.data(), (size_t)n)) { fprintf(stderr, "[server] send fail\n"); running=false; break; }
        }
    });

    t1.join(); t2.join();
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(cs);
    close(ls);
    SSL_CTX_free(ctx);
    return true;
}

} // namespace tls
