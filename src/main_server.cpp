#include "net/Server.h"
#include "crypto/GostCipher.h"
#include "engine/IEngineLoader.h"
#include "engine/EngineLoader.cpp"
#include "storage/IKeyStore.h"
#include "storage/FileKeyStore.h"

int main() {
    tls::EngineLoader loader;
    tls::FileKeyStore ks;
    tls::GostCipher gost(&loader);
    tls::Server srv(&gost, &ks, 4433);
    srv.run();
    return 0;
}

