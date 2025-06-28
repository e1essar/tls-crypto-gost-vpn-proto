#pragma once
#include "ICipherStrategy.h" // Подключает интерфейс стратегии шифрования
#include "../engine/IEngineLoader.h" // Подключает интерфейс загрузчика движков
#include <openssl/engine.h> // Для работы с движками OpenSSL
#include <string> // Для работы со строками
#include <vector> // Для хранения списка шифров

namespace tls {

class GostCipher : public ICipherStrategy { // Наследуется от ICipherStrategy
public:
    // Конструктор: принимает загрузчик движков и алгоритм ("any" — все ГОСТ-шифры)
    explicit GostCipher(IEngineLoader* loader, const std::string& algorithm = "any");
    ~GostCipher() override; // Переопределяет деструктор для освобождения ресурсов

    bool configureContext(SSL_CTX* ctx) override; // Реализация настройки SSL_CTX
    static std::vector<std::string> supportedSuites(); // Список поддерживаемых ГОСТ-шифров

private:
    IEngineLoader* _loader; // Указатель на загрузчик движков
    ENGINE* _engine = nullptr; // Указатель на движок OpenSSL (nullptr по умолчанию)
    std::string _algorithm; // Выбранный алгоритм шифрования
};

} // namespace tls