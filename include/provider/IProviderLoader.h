// tls-crypto-gost-vpn-proto-tls13\include\provider\IProviderLoader.h
#pragma once
#include <openssl/provider.h> // ����� API ��� �����������
#include <string>

namespace tls {

    class IProviderLoader {
    public:
        virtual ~IProviderLoader() = default;

        // ��������� ��������� �� ����� (��������, "gost" ��� "default")
        virtual OSSL_PROVIDER* loadProvider(const std::string& name) = 0;

        // ��������� ���������
        virtual void unloadProvider(OSSL_PROVIDER* provider) = 0;
    };

} // namespace tls
