#include "net/Server.h"
#include "crypto/GostCipher.h"
#include "engine/EngineLoader.h"
#include "storage/FileKeyStore.h"
#include <getopt.h>
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 4433;
    std::string algorithm = "any";
    std::string certFile = "../certs/cert.pem";
    std::string keyFile  = "../certs/key.pem";

    static struct option longopts[] = {
        {"port",   required_argument, nullptr, 'p'},
        {"cipher", required_argument, nullptr, 'c'},
        {"cert",   required_argument, nullptr, 't'},
        {"key",    required_argument, nullptr, 'k'},
        {nullptr,   0,               nullptr,   0 }
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "p:c:t:k:", longopts, nullptr)) != -1) {
        switch (opt) {
            case 'p': port      = std::stoi(optarg); break;
            case 'c': algorithm = optarg; break;
            case 't': certFile  = optarg; break;
            case 'k': keyFile   = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [--port port] [--cipher suite]"
                          << " [--cert cert.pem] [--key key.pem]\n";
                return 1;
        }
    }

    printf("Starting server on port %d with cipher '%s'\n", port, algorithm.c_str());

    tls::EngineLoader loader;
    tls::FileKeyStore ks;
    tls::GostCipher gost(&loader, algorithm);
    tls::Server srv(&gost, &ks, port, certFile, keyFile);
    return srv.run() ? 0 : 1;
}