cmake_minimum_required(VERSION 3.10...3.25) # Требуемая версия CMake
project(tls_gost_app) # Имя проекта

set(CMAKE_CXX_STANDARD 11) # Стандарт C++11
find_package(OpenSSL REQUIRED) # Находит OpenSSL
# Проверяем, что это OpenSSL 3
if (OPENSSL_VERSION VERSION_LESS 3.0.0)
    message(FATAL_ERROR "This project requires OpenSSL 3.x with provider support!")
endif()

include_directories( # Директории для заголовков
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/crypto
    ${CMAKE_SOURCE_DIR}/include/provider
    ${CMAKE_SOURCE_DIR}/include/storage
    ${CMAKE_SOURCE_DIR}/include/net
)

add_library(provider_loader src/provider/ProviderLoader.cpp) # Библиотека загрузчика провайдеров
target_link_libraries(provider_loader ${OPENSSL_LIBRARIES})

add_library(gost_cipher src/crypto/GostCipher.cpp) # Библиотека ГОСТ-шифрования
target_link_libraries(gost_cipher provider_loader ${OPENSSL_LIBRARIES})

add_library(file_keystore src/storage/FileKeyStore.cpp) # Библиотека хранилища ключей
target_link_libraries(file_keystore ${OPENSSL_LIBRARIES})

add_executable(server src/main_server.cpp src/net/Server.cpp) # Исполняемый файл сервера
target_link_libraries(server gost_cipher file_keystore provider_loader ${OPENSSL_LIBRARIES})

add_executable(client src/main_client.cpp src/net/Client.cpp) # Исполняемый файл клиента
target_link_libraries(client gost_cipher file_keystore provider_loader ${OPENSSL_LIBRARIES})