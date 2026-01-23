#include "net/Tun.h"
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

namespace tls {

Tun::Tun(const std::string& name) {
    _fd = open("/dev/net/tun", O_RDWR);
    if (_fd < 0) {
        perror("open(/dev/net/tun)");
        throw std::runtime_error("Failed to open /dev/net/tun");
    }

    struct ifreq ifr{};
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (!name.empty()) {
        strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
    }

    if (ioctl(_fd, TUNSETIFF, (void*)&ifr) < 0) {
        perror("ioctl(TUNSETIFF)");
        close(_fd);
        _fd = -1;
        throw std::runtime_error("TUNSETIFF failed");
    }

    _ifname = ifr.ifr_name;
    printf("[TUN] opened %s\n", _ifname.c_str());
}

Tun::~Tun() {
    if (_fd >= 0) close(_fd);
}

ssize_t Tun::readPacket(uint8_t* buf, size_t cap) {
    return read(_fd, buf, cap);
}

ssize_t Tun::writePacket(const uint8_t* buf, size_t len) {
    return write(_fd, buf, len);
}

}
