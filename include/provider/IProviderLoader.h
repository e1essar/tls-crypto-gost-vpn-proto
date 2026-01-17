#pragma once
#include <openssl/provider.h>
#include <string>

namespace tls {

    class IProviderLoader {
    public:
        virtual ~IProviderLoader() = default;

        virtual OSSL_PROVIDER* loadProvider(const std::string& name) = 0;

        virtual void unloadProvider(OSSL_PROVIDER* provider) = 0;
    };

} // namespace tls
