#pragma once
#include "IProviderLoader.h"
#include <openssl/provider.h>
#include <string>

namespace tls {

    class ProviderLoader : public IProviderLoader {
    public:
        OSSL_PROVIDER* loadProvider(const std::string& name) override;
        void unloadProvider(OSSL_PROVIDER* provider) override;
    };

}
