#pragma once 
#include <openssl/ssl.h> 

namespace tls {

class ICipherStrategy { 
public:
    virtual ~ICipherStrategy() = default; 
    virtual bool configureContext(SSL_CTX* ctx) = 0;
};

} // namespace tls