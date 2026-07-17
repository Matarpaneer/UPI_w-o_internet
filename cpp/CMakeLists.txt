cmake_minimum_required(VERSION 3.20)
project(UpiMesh CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# OpenSSL is required for crypto
find_package(OpenSSL REQUIRED)

# In Phase 1 we just build the crypto library. We will add Drogon and SQLite later.
add_library(upi_crypto
    crypto/ServerKeyHolder.cpp
    crypto/HybridCryptoService.cpp
)

target_include_directories(upi_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(upi_crypto PUBLIC OpenSSL::Crypto)
