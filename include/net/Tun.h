// tls-crypto-gost-vpn-proto-tls13\include\net\Tun.h
#pragma once
#include <string>
#include <cstdint>

namespace tls {

class Tun {
public:
    // name="" значит "дать ядру выбрать имя" (обычно tun0/tun1 и т.п.)
    explicit Tun(const std::string& name = "");
    ~Tun();

    // файловый дескриптор TUN
    int fd() const { return _fd; }
    // фактическое имя интерфейса (если ядро подобрало)
    const std::string& ifname() const { return _ifname; }

    // простые helpers
    ssize_t readPacket(uint8_t* buf, size_t cap);
    ssize_t writePacket(const uint8_t* buf, size_t len);

private:
    int _fd = -1;
    std::string _ifname;
};

} // namespace tls
