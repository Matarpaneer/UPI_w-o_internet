# UPIMesh C++ Backend

This is the C++ port of the UPIMesh backend. It provides the exact identical API surface and hybrid cryptographic idempotency guarantees as the original Java Spring Boot implementation, but rewritten in high-performance asynchronous C++ using the Drogon HTTP framework.

## Prerequisites

- **CMake** >= 3.14
- **C++17 Compiler** (Clang/GCC/MSVC)
- **OpenSSL** (`brew install openssl` or `apt-get install libssl-dev`)
- **nlohmann/json** (`brew install nlohmann-json` or bundled header)
- **SQLite3** (`brew install sqlite` or `apt-get install libsqlite3-dev`)
- **uuid** (`brew install ossp-uuid` or `apt-get install uuid-dev`)
- **Drogon** (built from source or installed via package manager)

## Architecture Overview

The system mirrors the Java original by strictly separating concerns:

1. **`crypto/`**: Handles RSA-OAEP / AES-256-GCM hybrid encryption, maintaining the exact byte-level wire format defined in Phase 1.
2. **`model/`**: Contains core entities (`Account`, `Transaction`, `MeshPacket`) and their strictly mapped JSON serialization boundaries. Also handles SQLite `Database` management and repository patterns (`AccountRepository`, `TransactionRepository`).
3. **`service/`**: Business logic. `IdempotencyService` enforces duplicate rejection. `SettlementService` handles double-entry accounting. `BridgeIngestionService` orchestrates the mesh upload pipeline.
4. **`controller/`**: The Drogon `ApiController`, mapping the HTTP surface 1-to-1 against the Java implementation.

### The Single-Writer Serialization Tradeoff

In Phase 4, a major structural decision was made in `SettlementService`. Because the system relies on a single shared `Database` connection wrapper around SQLite, independent `save()` statements (e.g. debiting sender and crediting receiver) could allow a concurrent API reader to observe a half-applied transaction.

To absolutely guarantee ACID isolation without rewriting the repository layer, the entire `SettlementService::settle` block is wrapped in a `std::recursive_mutex` at the C++ level and wrapped in an explicit SQLite `BEGIN IMMEDIATE TRANSACTION`. 

**The Tradeoff:** This perfectly isolates readers and guarantees double-entry integrity, but it completely serializes all reads and writes across the entire application. It effectively caps the system throughput at 1 operation at a time system-wide. In a production cluster running PostgreSQL, this C++ mutex would be removed, relying on the database's native row locks.

## Build Steps

```bash
cd cpp
mkdir -p build && cd build
cmake ..
make all
```

## Running the Server

```bash
cd build
./drogonserver
```
The server will start asynchronously on `http://0.0.0.0:8080` using a 16-thread pool.

## Running the Test Suite (CTest)

The project includes an end-to-end integration test suite encompassing Phase 1-3 binary verification, concurrent read-write stress testing, and real external ingestion validation.

```bash
cd build
ctest --output-on-failure
```
