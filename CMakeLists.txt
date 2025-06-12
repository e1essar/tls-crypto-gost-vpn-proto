cmake_minimum_required(VERSION 3.10...3.25)
project(tls_gost_app)

set(CMAKE_CXX_STANDARD 11)
find_package(OpenSSL REQUIRED)

# Пути для заголовков: основной и все подпапки
include_directories(
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/crypto
    ${CMAKE_SOURCE_DIR}/include/engine
    ${CMAKE_SOURCE_DIR}/include/storage
    ${CMAKE_SOURCE_DIR}/include/net
)

# Библиотека загрузчика движков
add_library(engine_loader
    src/engine/EngineLoader.cpp
)
target_link_libraries(engine_loader
    ${OPENSSL_LIBRARIES}
)

# Библиотека GOST-шифра
add_library(gost_cipher
    src/crypto/GostCipher.cpp
)
target_link_libraries(gost_cipher
    engine_loader
    ${OPENSSL_LIBRARIES}
)

# Библиотека файлового хранилища ключей
add_library(file_keystore
    src/storage/FileKeyStore.cpp
)
target_link_libraries(file_keystore
    ${OPENSSL_LIBRARIES}
)

# Серверное приложение
add_executable(server
    src/main_server.cpp
    src/net/Server.cpp
)
target_link_libraries(server
    gost_cipher
    file_keystore
    engine_loader
    ${OPENSSL_LIBRARIES}
)

# Клиентское приложение
add_executable(client
    src/main_client.cpp
    src/net/Client.cpp
)
target_link_libraries(client
    gost_cipher
    file_keystore
    engine_loader
    ${OPENSSL_LIBRARIES}
)
