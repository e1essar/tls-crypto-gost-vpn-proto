// tls-crypto-gost-vpn-proto-tls13\include\net\Server.h
#pragma once
#include "../crypto/ICipherStrategy.h" 
#include "../storage/IKeyStore.h"
#include <string> 

namespace tls {

    class Server {
    public:
        Server(ICipherStrategy* cs, IKeyStore* ks, int port, const std::string& certFile, const std::string& keyFile);
        bool run();

    private:
        ICipherStrategy* _cs;
        IKeyStore* _ks;
        int _port;
        std::string _certFile;
        std::string _keyFile;
    };

} // namespace tls