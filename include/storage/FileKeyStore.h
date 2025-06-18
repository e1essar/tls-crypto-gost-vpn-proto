// include/storage/FileKeyStore.h
#pragma once
#include "IKeyStore.h"
#include <string>

namespace tls {

/// Хранилище ключей на файловой системе
class FileKeyStore : public IKeyStore {
public:
    FileKeyStore() = default;
    ~FileKeyStore() override;  // <-- объявляем деструктор, но не определяем здесь

    bool loadCertificate(SSL_CTX* ctx, const std::string& certFile) override;
    bool loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) override;
};

} // namespace tls
