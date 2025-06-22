#pragma once
#include "IKeyStore.h" // Подключает интерфейс
#include <string> // Для путей к файлам

namespace tls {

class FileKeyStore : public IKeyStore { // Наследуется от IKeyStore
public:
    FileKeyStore() = default; // Конструктор по умолчанию
    ~FileKeyStore() override; // Переопределяет деструктор

    bool loadCertificate(SSL_CTX* ctx, const std::string& certFile) override; // Загрузка сертификата
    bool loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) override; // Загрузка ключа
};

} // namespace tls