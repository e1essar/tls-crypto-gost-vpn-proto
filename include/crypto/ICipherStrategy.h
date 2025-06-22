#pragma once // Предотвращает множественное включение файла
#include <openssl/ssl.h> // Подключает OpenSSL для работы с SSL/TLS

namespace tls { // Пространство имен tls для изоляции кода проекта

class ICipherStrategy { // Абстрактный базовый класс для стратегии шифрования
public:
    virtual ~ICipherStrategy() = default; // Виртуальный деструктор для корректного удаления через указатель на базовый класс
    virtual bool configureContext(SSL_CTX* ctx) = 0; // Чисто виртуальный метод для настройки SSL_CTX (контекста TLS)
};

} // namespace tls