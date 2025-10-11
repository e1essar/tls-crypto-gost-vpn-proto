#include "crypto/GostCipher.h" // Подключает заголовок
#include <openssl/err.h> // Для вывода ошибок OpenSSL
#include <cstdio> // Для printf и stderr

namespace tls {

    std::vector<std::string> GostCipher::supportedSuites() { // Список ГОСТ-шифров
        return {
            "TLS_GOSTR341112_256_WITH_MAGMA_MGM_L",
            "TLS_GOSTR341112_256_WITH_MAGMA_MGM_S",
            "TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_L",
            "TLS_GOSTR341112_256_WITH_KUZNYECHIK_MGM_S",
            // TLS1.2 legacy names (if you also want TLS1.2 style cipher-list)
            "GOST2012-MAGMA-MAGMAOMAC",
            "GOST2012-KUZNYECHIK-KUZNYECHIKOMAC"
        };
    }

    GostCipher::GostCipher(IProviderLoader* loader, const std::string& algorithm)
        : _loader(loader), _algorithm(algorithm.empty() ? "any" : algorithm) {
    }

    GostCipher::~GostCipher() {
        if (_gost) _loader->unloadProvider(_gost);
        if (_default) _loader->unloadProvider(_default);
    }

    bool GostCipher::configureContext(SSL_CTX* ctx) {
        _default = _loader->loadProvider("default");
        _gost = _loader->loadProvider("gostprov");

        if (!_gost) {
            fprintf(stderr, "Failed to load GOST provider\n");
            return false;
        }

        std::string cipherList;
        auto suites = supportedSuites();

        if (_algorithm == "any") {
            for (size_t i = 0; i < suites.size(); ++i) {
                cipherList += suites[i];
                if (i + 1 < suites.size()) cipherList += ":";
            }
        }
        else {
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

        printf("Configuring GOST cipher list: %s\n", cipherList.c_str());
        if (SSL_CTX_set_ciphersuites(ctx, cipherList.c_str()) != 1) {
            ERR_print_errors_fp(stderr);
            return false;
        }

        return true;
    }

} // namespace tls