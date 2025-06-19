// include/net/Utils.h
#pragma once
#include <openssl/ssl.h>
#include <string>
#include <arpa/inet.h>

namespace tls {

bool sendWithLength(SSL* ssl, const char* data, size_t len) {
    uint32_t lenNet = htonl(len); // Переводим длину в сетевой порядок байтов
    if (SSL_write(ssl, &lenNet, 4) != 4) return false;
    if (SSL_write(ssl, data, len) != static_cast<int>(len)) return false;
    return true;
}

bool receiveWithLength(SSL* ssl, std::string& data) {
    uint32_t lenNet;
    if (SSL_read(ssl, &lenNet, 4) != 4) return false;
    size_t len = ntohl(lenNet); // Переводим обратно в хост-порядок
    data.resize(len);
    size_t total = 0;
    while (total < len) {
        int n = SSL_read(ssl, &data[total], len - total);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

std::string getHostFromRequest(const std::string& request) {
    size_t hostPos = request.find("Host:");
    if (hostPos == std::string::npos) return "";
    size_t start = hostPos + 5;
    while (start < request.size() && std::isspace(request[start])) start++;
    size_t end = request.find_first_of("\r\n", start);
    if (end == std::string::npos) return "";
    return request.substr(start, end - start);
}

std::string readHttpResponse(int sock) {
    std::string response;
    char buf[1024];
    while (true) {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, n);
        if (response.find("\r\n\r\n") != std::string::npos) {
            size_t clPos = response.find("Content-Length:");
            if (clPos != std::string::npos) {
                size_t start = clPos + 15;
                while (start < response.size() && std::isspace(response[start])) start++;
                size_t end = response.find_first_of("\r\n", start);
                if (end != std::string::npos) {
                    int contentLength = std::stoi(response.substr(start, end - start));
                    size_t headerEnd = response.find("\r\n\r\n") + 4;
                    int bodyRead = response.size() - headerEnd;
                    while (bodyRead < contentLength) {
                        n = recv(sock, buf, sizeof(buf), 0);
                        if (n <= 0) break;
                        response.append(buf, n);
                        bodyRead += n;
                    }
                }
            }
            break;
        }
    }
    return response;
}

} // namespace tls