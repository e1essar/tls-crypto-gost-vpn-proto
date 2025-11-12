// tls-crypto-gost-vpn-proto-tls13\include\net\Client.h
#pragma once
#include "../crypto/ICipherStrategy.h" 
#include "../storage/IKeyStore.h" 
#include <string> 

namespace tls {

class Client { 
public:
    Client(ICipherStrategy* cs, IKeyStore* ks,
           const std::string& host, int port,
           const std::string& tunName = "");
    bool run();

private:
    ICipherStrategy* _cs; 
    IKeyStore* _ks; 
    std::string _host;
    int _port;
    std::string _tunName;
};

} // namespace tls
