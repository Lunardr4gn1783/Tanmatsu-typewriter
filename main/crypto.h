#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*----------------------------------------------------------
 * Supported encryption methods
 *----------------------------------------------------------*/
typedef enum {
    CRYPTO_METHOD_AES256_CBC = 0,
    CRYPTO_METHOD_SERPENT_CBC,
    CRYPTO_METHOD_RSA_2048,
    CRYPTO_METHOD_COUNT
} CryptoMethod;

/*----------------------------------------------------------
 * Human-readable names for the UI
 *----------------------------------------------------------*/
const char *crypto_method_name(CryptoMethod method);

/*----------------------------------------------------------
 * Output size calculation
 *
 * For AES/Serpent (block ciphers with PKCS7 padding):
 *   output_size = input_size + (16 - (input_size % 16))
 *
 * For RSA-2048 (encrypts 190 bytes at a time, outputs 256):
 *   output_size = ceil(input_size / 190) * 256
 *
 * The caller must allocate *out of at least this many bytes.
 *----------------------------------------------------------*/
size_t crypto_output_size(CryptoMethod method, size_t input_size);

/*----------------------------------------------------------
 * Encrypt
 *
 * Encrypts `in` of length `in_len` into `out`.
 * `out` must be at least crypto_output_size(method, in_len).
 * `out_len` is set to the actual number of bytes written.
 *
 * Returns true on success.
 *----------------------------------------------------------*/
bool crypto_encrypt(
    CryptoMethod method,
    const uint8_t *in,
    size_t in_len,
    uint8_t *out,
    size_t *out_len,
    const char *password);

/*----------------------------------------------------------
 * Decrypt
 *
 * Decrypts `in` of length `in_len` into `out`.
 * `out` must be at least `in_len` bytes.
 * `out_len` is set to the actual number of bytes written.
 *
 * Returns true on success.
 *----------------------------------------------------------*/
bool crypto_decrypt(
    CryptoMethod method,
    const uint8_t *in,
    size_t in_len,
    uint8_t *out,
    size_t *out_len,
    const char *password);

#endif
