#include "crypto/GostCipher.h" // Подключает заголовок
#include <openssl/err.h> // Для вывода ошибок OpenSSL
#include <cstdio> // Для printf и stderr

namespace tls {

std::vector<std::string> GostCipher::supportedSuites() { // Список ГОСТ-шифров
    return {
        "GOST2012-MAGMA-MAGMAOMAC", // ГОСТ 2012 с Магмой
        "GOST2012-KUZNYECHIK-KUZNYECHIKOMAC", // ГОСТ 2012 с Кузнечиком
        "LEGACY-GOST2012-GOST8912-GOST8912", // Устаревший ГОСТ
        "IANA-GOST2012-GOST8912-GOST8912", // ГОСТ по стандарту IANA
        "GOST2001-GOST89-GOST89" // ГОСТ 2001
    };
}

GostCipher::GostCipher(IEngineLoader* loader, const std::string& algorithm)
  : _loader(loader) // Инициализирует загрузчик
  , _algorithm(algorithm.empty() ? "any" : algorithm) // Если algorithm пустой, использует "any"
{}

GostCipher::~GostCipher() { // Деструктор
    if (_engine) { // Если движок был загружен
        ENGINE_finish(_engine); // Завершает работу движка
        ENGINE_free(_engine); // Освобождает память
    }
}

bool GostCipher::configureContext(SSL_CTX* ctx) { // Настройка контекста TLS
    _engine = _loader->loadEngine("gost"); // Загружает движок "gost"
    if (!_engine) { // Проверка на ошибку
        fprintf(stderr, "Failed to load GOST engine\n");
        return false;
    }
    ENGINE_set_default(_engine, ENGINE_METHOD_ALL); // Устанавливает движок по умолчанию для всех методов

    auto all = supportedSuites(); // Получает список шифров
    std::string cipherList; // Список шифров для SSL_CTX

    if (_algorithm == "any") { // Если выбран "any"
        for (size_t i = 0; i < all.size(); ++i) { // Проходит по всем шифрам
            cipherList += all[i]; // Добавляет шифр
            if (i + 1 < all.size()) cipherList += ":"; // Добавляет разделитель
        }
    } else { // Если указан конкретный шифр
        bool found = false;
        for (auto& s : all) { // Ищет шифр в списке
            if (s == _algorithm) {
                cipherList = s;
                found = true;
                break;
            }
        }
        if (!found) { // Если шифр не найден
            fprintf(stderr, "Unsupported GOST cipher: %s\n", _algorithm.c_str());
            fprintf(stderr, "Supported suites:\n");
            for (auto& s : all) fprintf(stderr, "  %s\n", s.c_str());
            return false;
        }
    }

    printf("Configuring GOST cipher list: %s\n", cipherList.c_str()); // Выводит список шифров
    if (SSL_CTX_set_cipher_list(ctx, cipherList.c_str()) != 1) { // Устанавливает список шифров
        ERR_print_errors_fp(stderr); // Выводит ошибки OpenSSL
        return false;
    }
    SSL_CTX_set_info_callback(ctx, [](const SSL* ssl, int where, int) { // Callback для отладки
        if (where & SSL_CB_HANDSHAKE_DONE) { // После завершения handshake
            printf("Negotiated cipher: %s\n", SSL_get_cipher(ssl)); // Выводит согласованный шифр
        }
    });
    return true; // Успех
}

} // namespace tls