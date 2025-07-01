#pragma once
#include "../crypto/ICipherStrategy.h" 
#include "../storage/IKeyStore.h" 
#include <string> 

namespace tls {

class Client { 
public:
    Client(ICipherStrategy* cs, IKeyStore* ks, const std::string& host, int port); 
    bool run();

private:
    ICipherStrategy* _cs; 
    IKeyStore* _ks; 
    std::string _host;
    int _port;
};

} // namespace tls