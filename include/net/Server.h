#pragma once
#include "../crypto/ICipherStrategy.h"
#include "../storage/IKeyStore.h"

namespace tls {

class Server {
public:
    Server(ICipherStrategy* cs, IKeyStore* ks, int port);
    bool run();

private:
    ICipherStrategy* _cs;
    IKeyStore* _ks;
    int _port;
};

} // namespace tls
