// tls-crypto-gost-vpn-proto-tls13\src\net\Server.cpp
#include "Server.h"
#include "Utils.h"
#include <openssl/ssl.h> 
#include <openssl/err.h> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <cstdio> 
#include <cstring> 
#include <netdb.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
namespace tls {

Server::Server(ICipherStrategy* cs, IKeyStore* ks, int port,
               const std::string& certFile, const std::string& keyFile)
 : _cs(cs), _ks(ks), _port(port), _certFile(certFile), _keyFile(keyFile) {}

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

    sockaddr_in peer_addr{};
    socklen_t peer_len = sizeof(peer_addr);
    getpeername(client, (sockaddr*)&peer_addr, &peer_len);
    std::string clientIp = inet_ntoa(peer_addr.sin_addr);

    SSL* ssl = SSL_new(ctx); 
    SSL_set_fd(ssl, client);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        while (true) {
            std::string request; 
            if (!receiveWithLength(ssl, request)) break;
            printf("\n[Server] Received encrypted HTTP request from client (%zu bytes over TLS)\n", request.size());
            printf("[Server] Decrypted HTTP request from client:\n%s\n", request.c_str());

            std::string host = getHostFromRequest(request); 
            if (host.empty()) {
                printf("[Server] No Host header found\n");
                break;
            }
            printf("[Server] Connecting to target host: %s\n", host.c_str());

            // Добавляем/заменяем X-Forwarded-For
            // std::string modRequest = addOrReplaceXForwardedFor(request, clientIp);
            std::string modRequest = request;

            struct addrinfo hints{}, *res; 
            memset(&hints, 0, sizeof(hints)); 
            hints.ai_family = AF_INET; 
            hints.ai_socktype = SOCK_STREAM;

            std::string hostname = host;
            std::string port = "80";

            // Разбить host:port, если есть
            auto pos = host.find(':');
            if (pos != std::string::npos) {
                hostname = host.substr(0, pos);
                port = host.substr(pos + 1);
            }

            if (getaddrinfo(hostname.c_str(), port.c_str(), &hints, &res) != 0) {
                printf("[Server] DNS resolution failed for %s\n", hostname.c_str());
                continue;
            }

            int targetSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol); 
            if (connect(targetSock, res->ai_addr, res->ai_addrlen) < 0) { 
                perror("[Server] connect to target");
                close(targetSock);
                freeaddrinfo(res);
                continue;
            }

            printf("[Server] Sending HTTP request to target server (%zu bytes, not encrypted):\n%s\n", modRequest.size(), modRequest.c_str());
            send(targetSock, modRequest.data(), modRequest.size(), 0); 
            std::string response = readHttpResponse(targetSock); 
            close(targetSock); 
            printf("[Server] Received HTTP response from target server (%zu bytes, not encrypted)\n", response.size());
            printf("[Server] Sending encrypted response to client (%zu bytes over TLS)\n", response.size());

            if (!sendWithLength(ssl, response.data(), response.size())) break; 
            freeaddrinfo(res); 
        }
    }

    SSL_free(ssl); 
    close(client);
    close(sock); 
    SSL_CTX_free(ctx);
    return true; 
}

std::string addOrReplaceXForwardedFor(const std::string& request, const std::string& clientIp) {
    std::string out;
    size_t pos = 0;
    size_t xffPos = request.find("X-Forwarded-For:");
    size_t headersEnd = request.find("\r\n\r\n");
    if (headersEnd == std::string::npos) headersEnd = request.size();
    if (xffPos != std::string::npos && xffPos < headersEnd) {
        // Заменить существующий X-Forwarded-For
        size_t lineEnd = request.find("\r\n", xffPos);
        out = request.substr(0, xffPos);
        out += "X-Forwarded-For: " + clientIp + "\r\n";
        if (lineEnd != std::string::npos)
            out += request.substr(lineEnd + 2);
    } else {
        // Вставить X-Forwarded-For после Host
        size_t hostPos = request.find("Host:");
        if (hostPos != std::string::npos) {
            size_t hostEnd = request.find("\r\n", hostPos);
            if (hostEnd != std::string::npos) {
                out = request.substr(0, hostEnd + 2);
                out += "X-Forwarded-For: " + clientIp + "\r\n";
                out += request.substr(hostEnd + 2);
            } else {
                out = request + "X-Forwarded-For: " + clientIp + "\r\n";
            }
        } else {
            out = request + "X-Forwarded-For: " + clientIp + "\r\n";
        }
    }
    return out;
}

} // namespace tls