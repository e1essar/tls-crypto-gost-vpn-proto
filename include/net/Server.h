#pragma once
#include "../crypto/ICipherStrategy.h" // Для настройки шифрования
#include "../storage/IKeyStore.h" // Для загрузки ключей
#include <string> // Для хранения хоста

namespace tls {

class Client { // Класс клиента VPN
public:
    Client(ICipherStrategy* cs, IKeyStore* ks, const std::string& host, int port); // Конструктор
    bool run(); // Метод запуска клиента

private:
    ICipherStrategy* _cs; // Указатель на стратегию шифрования
    IKeyStore* _ks; // Указатель на хранилище ключей
    std::string _host; // Адрес сервера
    int _port; // Порт сервера
};

} // namespace tls