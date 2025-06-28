#pragma once
#include "../crypto/ICipherStrategy.h" // Для настройки шифрования
#include "../storage/IKeyStore.h" // Для загрузки ключей
#include <string> // Для хранения пути к сертификату и ключу

namespace tls {

class Server { // Класс сервера VPN
public:
    Server(ICipherStrategy* cs, IKeyStore* ks, int port, const std::string& certFile, const std::string& keyFile); // Конструктор
    bool run(); // Метод запуска сервера

private:
    ICipherStrategy* _cs; // Указатель на стратегию шифрования
    IKeyStore* _ks; // Указатель на хранилище ключей
    int _port; // Порт сервера
    std::string _certFile; // Путь к сертификату
    std::string _keyFile; // Путь к ключу
};

} // namespace tls