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

add_executable(verify_phase1 crypto/verify_phase1.cpp)
target_link_libraries(verify_phase1 upi_crypto OpenSSL::Crypto)

add_executable(cross_lang_test crypto/cross_lang_test.cpp)
target_link_libraries(cross_lang_test upi_crypto OpenSSL::Crypto)

# Phase 2: Models & Persistence
add_library(upi_model
    model/Database.cpp
    model/AccountRepository.cpp
    model/TransactionRepository.cpp
)
target_include_directories(upi_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Link against system sqlite3
target_link_libraries(upi_model PUBLIC sqlite3)

add_executable(craft_packet craft_packet.cpp)
target_link_libraries(craft_packet PRIVATE upi_crypto upi_model OpenSSL::Crypto)

add_executable(verify_phase2 model/verify_phase2.cpp)
target_link_libraries(verify_phase2 upi_model)

# Phase 3: Services
add_library(upi_service
    service/IdempotencyService.cpp
    service/SettlementService.cpp
    service/BridgeIngestionService.cpp
    service/MeshSimulatorService.cpp
)
target_include_directories(upi_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(upi_service PUBLIC upi_model upi_crypto OpenSSL::Crypto)

add_executable(verify_phase3 service/verify_phase3.cpp)
target_link_libraries(verify_phase3 upi_service)

# ---------------------------------------------------------
# Phase 5: CTest Wiring
# ---------------------------------------------------------
enable_testing()

add_test(NAME verify_phase1 COMMAND verify_phase1)
add_test(NAME verify_phase2 COMMAND verify_phase2)
add_test(NAME verify_phase3 COMMAND verify_phase3)

# Integration tests that require the Drogon HTTP server
add_test(NAME smoke_test COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_with_server.sh ${CMAKE_CURRENT_BINARY_DIR} bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.sh)
add_test(NAME concurrency_test COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_with_server.sh ${CMAKE_CURRENT_BINARY_DIR} bash ${CMAKE_CURRENT_SOURCE_DIR}/concurrency_test.sh)
add_test(NAME read_write_test COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_with_server.sh ${CMAKE_CURRENT_BINARY_DIR} python3 ${CMAKE_CURRENT_SOURCE_DIR}/read_write_test.py)
add_test(NAME pre_ctest_checks COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_with_server.sh ${CMAKE_CURRENT_BINARY_DIR} python3 ${CMAKE_CURRENT_SOURCE_DIR}/pre_ctest_checks.py)

# Phase 4: App Entry (Drogon)
find_package(Drogon CONFIG REQUIRED)

add_executable(drogonserver main.cpp controller/ApiController.cpp)
target_include_directories(drogonserver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(drogonserver PUBLIC upi_service upi_crypto upi_model Drogon::Drogon)
