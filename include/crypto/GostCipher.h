#pragma once
#include "ICipherStrategy.h"
#include "../engine/IEngineLoader.h"
#include <openssl/engine.h>

namespace tls {

class GostCipher : public ICipherStrategy {
public:
    explicit GostCipher(IEngineLoader* loader);
    ~GostCipher() override;

    bool configureContext(SSL_CTX* ctx) override;

private:
    IEngineLoader* _loader;
    ENGINE* _engine = nullptr;
};

} // namespace tls

