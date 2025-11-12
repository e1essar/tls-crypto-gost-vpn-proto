// tls-crypto-gost-vpn-proto-tls13\include\net\Utils.h
#pragma once
#include <openssl/ssl.h>
#include <string>
#include <arpa/inet.h>
#include <cctype>
#include <cstdint>
#include <unistd.h>

namespace tls {

// простая фреймизация: [4 байта длины в сети] + payload
inline bool sendWithLength(SSL* ssl, const uint8_t* data, size_t len) {
    uint32_t lenNet = htonl(static_cast<uint32_t>(len));
    if (SSL_write(ssl, &lenNet, 4) != 4) return false;
    size_t total = 0;
    while (total < len) {
        int n = SSL_write(ssl, data + total, static_cast<int>(len - total));
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

inline bool receiveWithLength(SSL* ssl, std::string& data) {
    uint32_t lenNet = 0;
    int r = SSL_read(ssl, &lenNet, 4);
    if (r != 4) return false;
    uint32_t len = ntohl(lenNet);
    if (len > (16 * 1024 * 1024)) return false; // анти-DoS: ограничим до 16М
    data.resize(len);

    size_t total = 0;
    while (total < len) {
        int n = SSL_read(ssl, &data[total], static_cast<int>(len - total));
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

} // namespace tls
