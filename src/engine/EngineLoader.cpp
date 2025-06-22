#include "engine/EngineLoader.h"
#include <openssl/engine.h> // Для работы с движками
#include <openssl/err.h> // Для вывода ошибок
#include <cstdio> // Для stderr

namespace tls {

ENGINE* EngineLoader::loadEngine(const std::string& engineId) { // Загрузка движка
    ENGINE_load_builtin_engines(); // Загружает встроенные движки OpenSSL
    ENGINE* e = ENGINE_by_id(engineId.c_str()); // Находит движок по ID
    if (!e || !ENGINE_init(e)) { // Проверка на ошибки
        fprintf(stderr, "Engine %s load error\n", engineId.c_str());
        ERR_print_errors_fp(stderr);
        return nullptr;
    }
    return e; // Возвращает указатель на движок
}

} // namespace tls