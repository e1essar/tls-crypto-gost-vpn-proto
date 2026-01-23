#pragma once
#include <string>
#include <cstdint>

namespace tls {

class Tun {
public:
    explicit Tun(const std::string& name = "");
    ~Tun();

    int fd() const { return _fd; }
    const std::string& ifname() const { return _ifname; }

    ssize_t readPacket(uint8_t* buf, size_t cap);
    ssize_t writePacket(const uint8_t* buf, size_t len);

private:
    int _fd = -1;
    std::string _ifname;
};

}
