// tls-crypto-gost-vpn-proto-tls13\src\main_server.cpp
#include "net/Server.h"
#include "crypto/GostCipher.h"
#include "provider/ProviderLoader.h"
#include "storage/FileKeyStore.h"
#include <getopt.h>
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 4433;
    std::string algo = "any";
    std::string cert = "certs/cert.pem";
    std::string key  = "certs/key.pem";
    std::string tunName = "";

    static option opts[] = {
        {"port", required_argument, nullptr, 'p'},
        {"cipher", required_argument, nullptr, 'c'},
        {"cert", required_argument, nullptr, 't'},
        {"key", required_argument, nullptr, 'k'},
        {"tun", required_argument, nullptr, 'n'},
        {nullptr, 0, nullptr, 0}
    };
    int o;
    while ((o = getopt_long(argc, argv, "p:c:t:k:n:", opts, nullptr)) != -1) {
        switch (o) {
            case 'p': port = std::stoi(optarg); break;
            case 'c': algo = optarg; break;
            case 't': cert = optarg; break;
            case 'k': key  = optarg; break;
            case 'n': tunName = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [--port n] [--cipher name] [--cert cert.pem] [--key key.pem] [--tun ifname]\n";
                return 1;
        }
    }

    tls::ProviderLoader loader;
    tls::FileKeyStore ks;
    tls::GostCipher gost(&loader, algo);
    tls::Server app(&gost, &ks, port, cert, key, tunName);
    return app.run() ? 0 : 2;
}
