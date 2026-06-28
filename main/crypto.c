// It isn't a crypto miner... This is a dedicated encryption methods driver
#include "crypto.h"
#include <string.h>

/*----------------------------------------------------------
 * Provider 0: Basic XOR Stream Cipher
 *----------------------------------------------------------*/
static void cipher_xor(uint8_t *data, size_t length, const char *password)
{
    size_t pass_len = strlen(password);
    if (pass_len == 0) return;

    for (size_t i = 0; i < length; i++) 
    {
        data[i] ^= password[i % pass_len];
    }
}

/*----------------------------------------------------------
 * The Routing Switchboard
 *----------------------------------------------------------*/
bool crypto_process(
    CryptoMethod method,
    uint8_t *data,
    size_t length,
    const char *password,
    bool is_encrypting)
{
    if (!data || length == 0 || !password) return false;

    switch (method)
    {
        case CRYPTO_METHOD_XOR:
            // XOR is symmetric, so encrypting and decrypting use the exact same logic
            cipher_xor(data, length, password);
            return true;
            
        // case CRYPTO_METHOD_AES256:
        //    if (is_encrypting) cipher_aes_encrypt(data, length, password);
        //    else cipher_aes_decrypt(data, length, password);
        //    return true;

        default:
            return false;
    }
}

const char* crypto_method_name(CryptoMethod method)
{
    switch (method)
    {
        case CRYPTO_METHOD_XOR: return "XOR Stream";
        // case CRYPTO_METHOD_AES256: return "AES-256-CBC";
        default: return "Unknown";
    }
}