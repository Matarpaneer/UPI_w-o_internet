# UPIMesh 

UPIMesh is an experimental demonstration of an offline-first mesh-networked payment system using a hybrid cryptography model. It guarantees strong idempotency, double-entry consistency, and replay-attack prevention even when devices lack internet connectivity.

> **Note**: This project has been entirely rewritten in C++. The original Java Spring Boot implementation has been retired.

## C++ Backend

The entire server has been ported to high-performance C++17 using the **Drogon** web framework and **SQLite**. It matches the original Java implementation's cryptographic boundary byte-for-byte, while enforcing rigorous single-writer serialization for maximum database consistency.

For full build instructions, API documentation, and test suite execution, please see the [C++ Directory README](cpp/README.md).

## Getting Started

```bash
cd cpp
mkdir build && cd build
cmake ..
make all
./drogonserver
```

## Running Tests
```bash
cd cpp/build
ctest --output-on-failure
```
