// src/crypto/GostCipher.cpp
#include "crypto/GostCipher.h"
#include <openssl/err.h>
#include <cstdio>

namespace tls {

// берём полный список из `openssl ciphers | grep -i gost`
std::vector<std::string> GostCipher::supportedSuites() {
    return {
        "GOST2012-MAGMA-MAGMAOMAC",
        "GOST2012-KUZNYECHIK-KUZNYECHIKOMAC",
        "LEGACY-GOST2012-GOST8912-GOST8912",
        "IANA-GOST2012-GOST8912-GOST8912",
        "GOST2001-GOST89-GOST89"
    };
}

GostCipher::GostCipher(IEngineLoader* loader, const std::string& algorithm)
  : _loader(loader)
  , _algorithm(algorithm.empty() ? "any" : algorithm)
{}

GostCipher::~GostCipher() {
    if (_engine) {
        ENGINE_finish(_engine);
        ENGINE_free(_engine);
    }
}

bool GostCipher::configureContext(SSL_CTX* ctx) {
    _engine = _loader->loadEngine("gost");
    if (!_engine) {
        fprintf(stderr, "Failed to load GOST engine\n");
        return false;
    }
    ENGINE_set_default(_engine, ENGINE_METHOD_ALL);

    auto all = supportedSuites();
    std::string cipherList;

    if (_algorithm == "any") {
        for (size_t i = 0; i < all.size(); ++i) {
            cipherList += all[i];
            if (i + 1 < all.size()) cipherList += ":";
        }
    } else {
        bool found = false;
        for (auto& s : all) {
            if (s == _algorithm) {
                cipherList = s;
                found = true;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Unsupported GOST cipher: %s\n", _algorithm.c_str());
            fprintf(stderr, "Supported suites:\n");
            for (auto& s : all) fprintf(stderr, "  %s\n", s.c_str());
            return false;
        }
    }

    printf("Configuring GOST cipher list: %s\n", cipherList.c_str());
    if (SSL_CTX_set_cipher_list(ctx, cipherList.c_str()) != 1) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    SSL_CTX_set_info_callback(ctx, [](const SSL* ssl, int where, int) {
        if (where & SSL_CB_HANDSHAKE_DONE) {
            printf("Negotiated cipher: %s\n", SSL_get_cipher(ssl));
        }
    });
    return true;
}

} // namespace tls

