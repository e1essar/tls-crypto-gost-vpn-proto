#include "engine/EngineLoader.h"
#include <openssl/engine.h>
#include <openssl/err.h>
#include <cstdio>

namespace tls {

ENGINE* EngineLoader::loadEngine(const std::string& engineId) {
    ENGINE_load_builtin_engines();
    ENGINE* e = ENGINE_by_id(engineId.c_str());
    if (!e || !ENGINE_init(e)) {
        fprintf(stderr, "Engine %s load error\n", engineId.c_str());
        ERR_print_errors_fp(stderr);
        return nullptr;
    }
    return e;
}

}
