#pragma once
#include <openssl/ssl.h> // Для работы с SSL_CTX
#include <string> // Для путей к файлам

namespace tls {

class IKeyStore { // Абстрактный класс для хранилища ключей
public:
    virtual ~IKeyStore() = default; // Виртуальный деструктор
    virtual bool loadCertificate(SSL_CTX* ctx, const std::string& certFile) = 0; // Загрузка сертификата
    virtual bool loadPrivateKey(SSL_CTX* ctx, const std::string& keyFile) = 0; // Загрузка приватного ключа
};

} // namespace tls