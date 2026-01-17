// tls-crypto-gost-vpn-proto-tls13\src\crypto\GostCipher.cpp
#include "crypto/GostCipher.h"
#include <openssl/err.h>
#include <cstdio>

namespace tls {

std::vector<std::string> GostCipher::supportedSuites() {
    // Оставляем ТОЛЬКО TLS 1.3 имена
    return {
        "TLS_GOSTR341112_256_WITH_MAGMA_MGM_L",
        "TLS_GOSTR341112_256_WITH_MAGMA_MGM_S",
        "TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_L",
        "TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_S"
    };
}

GostCipher::GostCipher(IProviderLoader* loader, const std::string& algorithm)
    : _loader(loader), _algorithm(algorithm.empty() ? "any" : algorithm) {}

GostCipher::~GostCipher() {
    if (_gost)    _loader->unloadProvider(_gost);
    if (_default) _loader->unloadProvider(_default);
}

bool GostCipher::configureContext(SSL_CTX* ctx) {
    _default = _loader->loadProvider("default");
    _gost    = _loader->loadProvider("gostprov");

    if (!_gost) {
        fprintf(stderr, "Failed to load GOST provider\n");
        return false;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // SSL_CTX_set_security_level(ctx, 0);

    std::string cipherList;
    auto suites = supportedSuites();

    if (_algorithm == "any") {
        for (size_t i = 0; i < suites.size(); ++i) {
            cipherList += suites[i];
            if (i + 1 < suites.size()) cipherList += ":";
        }
    } else {
        bool found = false;
        for (auto& s : suites) {
            if (s == _algorithm) {
                cipherList = s;
                found = true;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Unsupported cipher suite: %s\n", _algorithm.c_str());
            return false;
        }
    }

    printf("Configuring GOST TLS1.3 ciphersuites: %s\n", cipherList.c_str());
    if (SSL_CTX_set_ciphersuites(ctx, cipherList.c_str()) != 1) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    return true;
}

} // namespace tls
