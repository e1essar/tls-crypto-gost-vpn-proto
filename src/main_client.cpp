#include "net/Client.h"
#include "crypto/GostCipher.h"
#include "engine/IEngineLoader.h"
#include "engine/EngineLoader.cpp"
#include "storage/IKeyStore.h"
#include "storage/FileKeyStore.h"

int main() {
    tls::EngineLoader loader;
    tls::FileKeyStore ks;
    tls::GostCipher gost(&loader);
    tls::Client cli(&gost, &ks, "127.0.0.1", 4433);
    cli.run();
    return 0;
}
