cmake_minimum_required(VERSION 3.16)
project(tls_gost_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(PROJ_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_library(provider_loader src/provider/ProviderLoader.cpp)
target_include_directories(provider_loader PUBLIC ${PROJ_INCLUDE_DIR})
target_link_libraries(provider_loader PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_library(gost_cipher src/crypto/GostCipher.cpp)
target_include_directories(gost_cipher PUBLIC ${PROJ_INCLUDE_DIR})
target_link_libraries(gost_cipher PUBLIC provider_loader OpenSSL::SSL OpenSSL::Crypto)

add_library(file_keystore src/storage/FileKeyStore.cpp)
target_include_directories(file_keystore PUBLIC ${PROJ_INCLUDE_DIR})
target_link_libraries(file_keystore PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_library(tun src/net/Tun.cpp)
target_include_directories(tun PUBLIC ${PROJ_INCLUDE_DIR})

add_executable(server
  src/main_server.cpp
  src/net/Server.cpp
)
target_include_directories(server PRIVATE ${PROJ_INCLUDE_DIR})
target_link_libraries(server PRIVATE
  gost_cipher
  file_keystore
  provider_loader
  tun
  OpenSSL::SSL OpenSSL::Crypto
  Threads::Threads
)

add_executable(client
  src/main_client.cpp
  src/net/Client.cpp
)
target_include_directories(client PRIVATE ${PROJ_INCLUDE_DIR})
target_link_libraries(client PRIVATE
  gost_cipher
  file_keystore
  provider_loader
  tun
  OpenSSL::SSL OpenSSL::Crypto
  Threads::Threads
)

if (OPENSSL_VERSION VERSION_LESS 3.0.0)
  message(FATAL_ERROR "This project requires OpenSSL 3.x with provider support!")
endif()
