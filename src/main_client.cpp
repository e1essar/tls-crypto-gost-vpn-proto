#include "net/Client.h"
#include "crypto/GostCipher.h"
#include "engine/EngineLoader.h"
#include "storage/FileKeyStore.h"
#include <getopt.h> // Для парсинга аргументов
#include <iostream> // Для cerr

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1"; // Хост по умолчанию
    int port = 4433; // Порт по умолчанию
    std::string algorithm = "any"; // Алгоритм по умолчанию

    static struct option longopts[] = { // Опции командной строки
        {"host",   required_argument, nullptr, 'h'},
        {"port",   required_argument, nullptr, 'p'},
        {"cipher", required_argument, nullptr, 'c'},
        {nullptr,  0,                 nullptr,   0 }
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "h:p:c:", longopts, nullptr)) != -1) { // Парсит аргументы
        switch (opt) {
            case 'h': host      = optarg; break; // Устанавливает хост
            case 'p': port      = std::stoi(optarg); break; // Устанавливает порт
            case 'c': algorithm = optarg; break; // Устанавливает алгоритм
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [--host host] [--port port] [--cipher suite]\n";
                return 1;
        }
    }

    printf("Starting client to %s:%d with cipher '%s'\n", host.c_str(), port, algorithm.c_str());

    tls::EngineLoader loader; // Создает загрузчик движков
    tls::FileKeyStore ks; // Создает хранилище ключей
    tls::GostCipher gost(&loader, algorithm); // Создает стратегию шифрования
    tls::Client cli(&gost, &ks, host, port); // Создает клиента
    return cli.run() ? 0 : 1; // Запускает клиента и возвращает код завершения
}