#pragma once
#include "IKeyStore.h" 
#include <string> 

namespace tls {

class FileKeyStore : public IKeyStore { 
public:
    FileKeyStore() = default; 
    ~FileKeyStore() override; 

    bool loadCertificate(SSL_CTX* ctx, const std::string& certFile) override; 
    bool loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) override;
};

} // namespace tls