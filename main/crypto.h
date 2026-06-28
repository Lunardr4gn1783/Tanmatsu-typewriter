#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 1. Define the enum FIRST
typedef enum {
    CRYPTO_METHOD_XOR = 0,
    CRYPTO_METHOD_COUNT
} CryptoMethod;

// 2. Declare the functions SECOND
const char* crypto_method_name(CryptoMethod method);

bool crypto_process(
    CryptoMethod method,
    uint8_t *data,
    size_t length,
    const char *password,
    bool is_encrypting
);

#endif