// include/crypto/GostCipher.h
#pragma once
#include "ICipherStrategy.h"
#include "../engine/IEngineLoader.h"
#include <openssl/engine.h>
#include <string>
#include <vector>

namespace tls {

class GostCipher : public ICipherStrategy {
public:
    // algorithm == "any" поддерживает все GOST‑suite
    explicit GostCipher(IEngineLoader* loader, const std::string& algorithm = "any");
    ~GostCipher() override;

    bool configureContext(SSL_CTX* ctx) override;
    // полный список cipher‑suite
    static std::vector<std::string> supportedSuites();

private:
    IEngineLoader* _loader;
    ENGINE* _engine = nullptr;
    std::string _algorithm;
};

} // namespace tls
