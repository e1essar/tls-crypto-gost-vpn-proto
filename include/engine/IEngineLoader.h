// tls-crypto-gost-vpn-proto-tls13\include\engine\IEngineLoader.h
#pragma once
#include <openssl/engine.h>
#include <string>

namespace tls {

class IEngineLoader {
public:
    virtual ~IEngineLoader() = default;
    virtual ENGINE* loadEngine(const std::string& engineId) = 0;
};

} // namespace tls