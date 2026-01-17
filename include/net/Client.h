#pragma once
#include "../crypto/ICipherStrategy.h" 
#include "../storage/IKeyStore.h" 
#include <string> 

namespace tls {

class Client { 
public:
    Client(ICipherStrategy* cs, IKeyStore* ks,
           const std::string& host, int port,
           const std::string& tunName = "",
           const std::string& caFile = "",
           const std::string& serverName = "",
           bool verifyPeer = false);
    bool run();

private:
    ICipherStrategy* _cs; 
    IKeyStore* _ks; 
    std::string _host;
    int _port;
    std::string _tunName;
    std::string _caFile;
    std::string _serverName;
    bool _verifyPeer;
};

} // namespace tls
