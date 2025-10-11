#pragma once
#include <openssl/provider.h> // Новый API для провайдеров
#include <string>

namespace tls {

    class IProviderLoader {
    public:
        virtual ~IProviderLoader() = default;

        // Загружает провайдер по имени (например, "gost" или "default")
        virtual OSSL_PROVIDER* loadProvider(const std::string& name) = 0;

        // Выгружает провайдер
        virtual void unloadProvider(OSSL_PROVIDER* provider) = 0;
    };

} // namespace tls
