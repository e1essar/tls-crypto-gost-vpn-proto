#include "net/Client.h"
#include "crypto/GostCipher.h"
#include "provider/ProviderLoader.h"
#include "storage/FileKeyStore.h"

#include <getopt.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int         port = 4433;
    std::string algorithm = "any";
    std::string tunName = "";

    static struct option longopts[] = {
        {"host",   required_argument, nullptr, 'h'},
        {"port",   required_argument, nullptr, 'p'},
        {"cipher", required_argument, nullptr, 'c'},
        {"tun",    required_argument, nullptr, 't'},
        {nullptr,  0,                 nullptr,   0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:c:t:", longopts, nullptr)) != -1) {
        switch (opt) {
            case 'h': host      = optarg; break;
            case 'p': port      = std::stoi(optarg); break;
            case 'c': algorithm = optarg; break;
            case 't': tunName   = optarg; break;
            default:
                std::cerr
                    << "Usage: " << argv[0]
                    << " [--host ip] [--port n] [--cipher name] [--tun ifname]\n";
                return 1;
        }
    }

    printf("Starting client to %s:%d with cipher '%s'%s%s\n",
           host.c_str(), port, algorithm.c_str(),
           tunName.empty() ? "" : " (tun=",
           tunName.empty() ? "" : (tunName + ")").c_str());

    tls::ProviderLoader loader;
    tls::FileKeyStore   ks;
    tls::GostCipher     gost(&loader, algorithm);

    tls::Client cli(&gost, &ks, host, port, tunName);
    return cli.run() ? 0 : 1;
}
