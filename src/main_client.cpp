#include "net/Client.h"
#include "crypto/GostCipher.h"
#include "provider/ProviderLoader.h"
#include "storage/FileKeyStore.h"

#include <getopt.h>
#include <iostream>
#include <string>
#include <csignal>

static void ignore_sigpipe() {
    std::signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char* argv[]) {
    ignore_sigpipe();

    std::string host = "127.0.0.1";
    int         port = 4433;
    std::string algorithm = "any";
    std::string tunName = "";
    std::string caFile = "";
    std::string serverName = "";
    bool verifyPeer = false;

    static struct option longopts[] = {
        {"host",   required_argument, nullptr, 'h'},
        {"port",   required_argument, nullptr, 'p'},
        {"cipher", required_argument, nullptr, 'c'},
        {"tun",    required_argument, nullptr, 't'},
        {"ca", required_argument, nullptr, 'a'},
        {"servername", required_argument, nullptr, 's'},
        {"verify", no_argument, nullptr, 'v'},
        {"insecure", no_argument, nullptr, 'i'},
        {nullptr,  0,                 nullptr,   0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:c:t:a:s:vi", longopts, nullptr)) != -1) {
        switch (opt) {
            case 'h': host      = optarg; break;
            case 'p': port      = std::stoi(optarg); break;
            case 'c': algorithm = optarg; break;
            case 't': tunName   = optarg; break;
            case 'a': caFile    = optarg; verifyPeer = true; break;
            case 's': serverName = optarg; break;
            case 'v': verifyPeer = true; break;
            case 'i': verifyPeer = false; break;
            default:
                std::cerr
                    << "Usage: " << argv[0]
                    << " [--host ip] [--port n] [--cipher name] [--tun ifname]"
                    << " [--ca ca.pem] [--servername name] [--verify|--insecure]\n";
                return 1;
        }
    }

    printf("Starting client to %s:%d with cipher '%s'%s%s\n",
           host.c_str(), port, algorithm.c_str(),
           tunName.empty() ? "" : " (tun=",
           tunName.empty() ? "" : (tunName + ")").c_str());

    try {
        tls::ProviderLoader loader;
        tls::FileKeyStore   ks;
        tls::GostCipher     gost(&loader, algorithm);

        tls::Client cli(&gost, &ks, host, port, tunName, caFile, serverName, verifyPeer);
        return cli.run() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[client] fatal: " << e.what() << "\n";
        return 2;
    }
}
