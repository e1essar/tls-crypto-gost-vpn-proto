#include "net/Client.h"
#include "crypto/GostCipher.h"
#include "engine/EngineLoader.h"
#include "storage/FileKeyStore.h"
#include <getopt.h>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 4433;
    std::string algorithm = "any";

    static struct option longopts[] = {
        {"host",   required_argument, nullptr, 'h'},
        {"port",   required_argument, nullptr, 'p'},
        {"cipher", required_argument, nullptr, 'c'},
        {nullptr,  0,                 nullptr,   0 }
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:c:", longopts, nullptr)) != -1) {
        switch (opt) {
            case 'h': host      = optarg; break;
            case 'p': port      = std::stoi(optarg); break;
            case 'c': algorithm = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [--host host] [--port port] [--cipher suite]\n";
                return 1;
        }
    }

    printf("Starting client to %s:%d with cipher '%s'\n", host.c_str(), port, algorithm.c_str());

    tls::EngineLoader loader;
    tls::FileKeyStore ks;
    tls::GostCipher gost(&loader, algorithm);
    tls::Client cli(&gost, &ks, host, port);
    return cli.run() ? 0 : 1;
}