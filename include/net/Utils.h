#pragma once
#include <openssl/ssl.h>
#include <string>
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>

namespace tls {

inline bool ssl_write_all(SSL* ssl, const void* buf, size_t len) {
    const auto* p = static_cast<const uint8_t*>(buf);
    size_t total = 0;
    while (total < len) {
        const int n = SSL_write(ssl, p + total, static_cast<int>(len - total));
        if (n <= 0) {
            const int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

inline bool ssl_read_all(SSL* ssl, void* buf, size_t len) {
    auto* p = static_cast<uint8_t*>(buf);
    size_t total = 0;
    while (total < len) {
        const int n = SSL_read(ssl, p + total, static_cast<int>(len - total));
        if (n <= 0) {
            const int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

inline bool sendWithLength(SSL* ssl, const uint8_t* data, size_t len) {
    if (len > 0xFFFFFFFFu) return false;
    const uint32_t lenNet = htonl(static_cast<uint32_t>(len));
    if (!ssl_write_all(ssl, &lenNet, sizeof(lenNet))) return false;
    return ssl_write_all(ssl, data, len);
}

inline bool receiveWithLength(SSL* ssl, std::string& data) {
    uint32_t lenNet = 0;
    if (!ssl_read_all(ssl, &lenNet, sizeof(lenNet))) return false;
    const uint32_t len = ntohl(lenNet);
    if (len > (16u * 1024u * 1024u)) return false;
    data.resize(len);
    if (len == 0) return true;
    return ssl_read_all(ssl, &data[0], len);
}

} // namespace tls
