#include "net/Tun.h"
#include "net/Utils.h"
#include "net/Client.h"
#include "crypto/GostCipher.h"
#include "storage/FileKeyStore.h"
#include "provider/ProviderLoader.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
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

static int tcp_connect(const std::string& host, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1) {
        perror("inet_pton"); close(s); return -1;
    }
    if (connect(s, (sockaddr*)&a, sizeof(a)) < 0) {
        perror("connect"); close(s); return -1;
    }
    return s;
}


Client::Client(ICipherStrategy* cs, IKeyStore* ks,
               const std::string& host, int port,
               const std::string& tunName,
               const std::string& caFile,
               const std::string& serverName,
               bool verifyPeer)
: _cs(cs), _ks(ks), _host(host), _port(port), _tunName(tunName),
  _caFile(caFile), _serverName(serverName), _verifyPeer(verifyPeer) {}

bool Client::run() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { ERR_print_errors_fp(stderr); return false; }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

    if (!_cs || !_cs->configureContext(ctx)) {
        SSL_CTX_free(ctx);
        return false;
    }

    if (_verifyPeer) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        if (!_caFile.empty()) {
            if (SSL_CTX_load_verify_locations(ctx, _caFile.c_str(), nullptr) != 1) {
                fprintf(stderr, "[client] Failed to load CA file: %s\n", _caFile.c_str());
                ERR_print_errors_fp(stderr);
                SSL_CTX_free(ctx);
                return false;
            }
        } else {
            if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
                fprintf(stderr, "[client] Failed to load default verify paths\n");
                ERR_print_errors_fp(stderr);
                SSL_CTX_free(ctx);
                return false;
            }
        }
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    int s = tcp_connect(_host, _port);
    if (s < 0) { SSL_CTX_free(ctx); return false; }

    SSL* ssl = SSL_new(ctx);
    if (!ssl) { ERR_print_errors_fp(stderr); close(s); SSL_CTX_free(ctx); return false; }

    SSL_set_fd(ssl, s);

    if (!_serverName.empty()) {
        SSL_set_tlsext_host_name(ssl, _serverName.c_str());
        if (_verifyPeer) {
            SSL_set1_host(ssl, _serverName.c_str());
        }
    }
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl); close(s); SSL_CTX_free(ctx); return false;
    }

    printf("[client] TLS connected\n");
    printf("[client][TLS] version=%s cipher=%s\n", SSL_get_version(ssl), SSL_get_cipher_name(ssl));

    Tun tun(_tunName);
    printf("[client] TUN ready: %s\n", tun.ifname().c_str());

    std::atomic<bool> running{true};

    std::thread t1([&]{
        std::vector<uint8_t> buf(20000);
        while (running.load()) {
            ssize_t n = tun.readPacket(buf.data(), buf.size());
            if (n <= 0) {
                perror("[client] read(TUN)");
                running = false; break;
            }
            log_ip_packet(buf.data(), (size_t)n, "C TUN->TLS");
            if (!sendWithLength(ssl, buf.data(), (size_t)n)) {
                fprintf(stderr, "[client] sendWithLength failed\n");
                running = false; break;
            }
        }
    });

    std::thread t2([&]{
        std::string frame;
        while (running.load()) {
            if (!receiveWithLength(ssl, frame)) {
                fprintf(stderr, "[client] receiveWithLength failed\n");
                running = false; break;
            }
            log_ip_packet(reinterpret_cast<const uint8_t*>(frame.data()), frame.size(), "C TLS->TUN");
            if (tun.writePacket(reinterpret_cast<const uint8_t*>(frame.data()), frame.size()) != (ssize_t)frame.size()) {
                perror("[client] write(TUN)");
                running = false; break;
            }
        }
    });

    t1.join();
    t2.join();

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(s);
    SSL_CTX_free(ctx);
    return true;
}

} // namespace tls
