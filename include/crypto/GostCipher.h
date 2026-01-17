#pragma once
#include "ICipherStrategy.h" 
#include "../provider/IProviderLoader.h"
#include <openssl/provider.h> 
#include <string>
#include <vector>

namespace tls {

    class GostCipher : public ICipherStrategy {
    public:
        explicit GostCipher(IProviderLoader* loader, const std::string& algorithm = "any");
        ~GostCipher() override; 

        bool configureContext(SSL_CTX* ctx) override; 
        static std::vector<std::string> supportedSuites(); 

    private:
        IProviderLoader* _loader; 
        OSSL_PROVIDER* _default = nullptr; 
        OSSL_PROVIDER* _gost = nullptr;    
        std::string _algorithm;
    };

} // namespace tls