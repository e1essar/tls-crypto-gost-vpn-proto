#include "provider/ProviderLoader.h"
#include <openssl/err.h>
#include <cstdio>

namespace tls {

    OSSL_PROVIDER* ProviderLoader::loadProvider(const std::string& name) {
        OSSL_PROVIDER* provider = OSSL_PROVIDER_load(nullptr, name.c_str());
        if (!provider) {
            fprintf(stderr, "Failed to load provider: %s\n", name.c_str());
            ERR_print_errors_fp(stderr);
        }
        return provider;
    }

    void ProviderLoader::unloadProvider(OSSL_PROVIDER* provider) {
        if (provider)
            OSSL_PROVIDER_unload(provider);
    }

} // namespace tls
