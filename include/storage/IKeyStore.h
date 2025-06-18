// include/storage/IKeyStore.h
#pragma once
#include <openssl/ssl.h>
#include <string>

namespace tls {

class IKeyStore {
public:
    virtual ~IKeyStore() = default;
    virtual bool loadCertificate(SSL_CTX* ctx, const std::string& certFile) = 0;
    virtual bool loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) = 0;
};

} // namespace tls
