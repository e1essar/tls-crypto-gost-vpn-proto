#pragma once
#include "ICipherStrategy.h" // Подключает интерфейс стратегии шифрования
#include "../provider/IProviderLoader.h" // Подключает интерфейс загрузчика провайдера
#include <openssl/provider.h> // Для работы с провайдерами OpenSSL
#include <string> // Для работы со строками
#include <vector> // Для хранения списка шифров

namespace tls {

    class GostCipher : public ICipherStrategy { // Наследуется от ICipherStrategy
    public:
        // Конструктор: принимает загрузчик движков и алгоритм ("any" — все ГОСТ-шифры)
        explicit GostCipher(IProviderLoader* loader, const std::string& algorithm = "any");
        ~GostCipher() override; // Переопределяет деструктор для освобождения ресурсов

        bool configureContext(SSL_CTX* ctx) override; // Реализация настройки SSL_CTX
        static std::vector<std::string> supportedSuites(); // Список поддерживаемых ГОСТ-шифров

    private:
        IProviderLoader* _loader; // Указатель на загрузчик движков
        OSSL_PROVIDER* _default = nullptr; // базовый провайдер
        OSSL_PROVIDER* _gost = nullptr;    // ГОСТ-провайдер
        std::string _algorithm; // Выбранный алгоритм шифрования
    };

} // namespace tls