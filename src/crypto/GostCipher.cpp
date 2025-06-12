#include "GostCipher.h"
#include <openssl/err.h>
#include <cstdio>

namespace tls {

GostCipher::GostCipher(IEngineLoader* loader)
  : _loader(loader)
{}

GostCipher::~GostCipher() {
    if (_engine) {
        ENGINE_finish(_engine);
        ENGINE_free(_engine);
    }
}

bool GostCipher::configureContext(SSL_CTX* ctx) {
    _engine = _loader->loadEngine("gost");
    if (!_engine) return false;
    ENGINE_set_default(_engine, ENGINE_METHOD_ALL);

    // Принудительно GOST-шифросуты
    if (SSL_CTX_set_cipher_list(ctx, "GOST2012-GOST8912-GOST8912") != 1) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    // Логируем в callback negotiated cipher
    SSL_CTX_set_info_callback(ctx, [](const SSL* ssl, int where, int) {
        if (where & SSL_CB_HANDSHAKE_DONE) {
            printf("Negotiated cipher: %s\n", SSL_get_cipher(ssl));
        }
    });
    return true;
}

} // namespace tls
