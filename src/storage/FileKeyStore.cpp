// src/storage/FileKeyStore.cpp
#include "storage/FileKeyStore.h"
#include <openssl/err.h>
#include <cstdio>

namespace tls {

// Теперь у нас есть «key function» для vtable
FileKeyStore::~FileKeyStore() = default;

bool FileKeyStore::loadCertificate(SSL_CTX* ctx, const std::string& certFile) {
    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    return true;
}

bool FileKeyStore::loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) {
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    return true;
}

} // namespace tls
