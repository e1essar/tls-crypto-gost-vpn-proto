#pragma once
#include <openssl/engine.h> // Для работы с движками OpenSSL
#include <string> // Для передачи ID движка

namespace tls {

class IEngineLoader { // Абстрактный класс для загрузки движков
public:
    virtual ~IEngineLoader() = default; // Виртуальный деструктор
    virtual ENGINE* loadEngine(const std::string& engineId) = 0; // Метод загрузки движка по ID
};

} // namespace tls