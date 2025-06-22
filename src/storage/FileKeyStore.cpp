#include "storage/FileKeyStore.h"
#include <openssl/err.h> // Для ошибок
#include <cstdio> // Для stderr

namespace tls {

FileKeyStore::~FileKeyStore() = default; // Пустой деструктор

bool FileKeyStore::loadCertificate(SSL_CTX* ctx, const std::string& certFile) {
    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) != 1) { // Загружает сертификат в PEM-формате
        ERR_print_errors_fp(stderr);
        return false;
    }
    return true;
}

bool FileKeyStore::loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) {
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) { // Загружает ключ в PEM-формате
        ERR_print_errors_fp(stderr);
        return false;
    }
    return true;
}

} // namespace tls