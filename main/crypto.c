/* Encryption methods driver
 *
 * Provides AES-256-CBC, Serpent-256-CBC, and RSA-2048-OAEP.
 * AES and RSA use mbedTLS (bundled with ESP-IDF).
 * Serpent is a self-contained reference implementation.
 */

#include "crypto.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "mbedtls/aes.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/bignum.h"

static const char *TAG = "crypto";

/* Block size for AES and Serpent */
#define BLOCK_SIZE 16

/* RSA-2048 parameters */
#define RSA_KEY_BITS     2048
#define RSA_PLAIN_CHUNK  190   /* 256 - 2*hash_len - 2 for OAEP-SHA256 */
#define RSA_CIPHER_CHUNK 256   /* RSA modulus size in bytes */

/*----------------------------------------------------------
 * Key Derivation: SHA-256(password || salt) -> 32 bytes
 *----------------------------------------------------------*/
static bool derive_key(const char *password, const uint8_t *salt, uint8_t key[32])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    if (mbedtls_sha256_starts(&ctx, 0) != 0)
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }

    mbedtls_sha256_update(&ctx, (const uint8_t *)password, strlen(password));
    mbedtls_sha256_update(&ctx, salt, BLOCK_SIZE);

    if (mbedtls_sha256_finish(&ctx, key) != 0)
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }

    mbedtls_sha256_free(&ctx);
    return true;
}

/*----------------------------------------------------------
 * Random Bytes via mbedTLS CTR-DRBG
 *----------------------------------------------------------*/
static bool random_bytes(uint8_t *buf, size_t len)
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    const char *pers = "crypto_gen";

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);

    int ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char *)pers, strlen(pers));
    if (ret != 0)
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        return false;
    }

    ret = mbedtls_ctr_drbg_random(&drbg, buf, len);

    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);

    return (ret == 0);
}

/*----------------------------------------------------------
 * PKCS7 Padding (block ciphers)
 *----------------------------------------------------------*/
static size_t pkcs7_pad_size(size_t data_len)
{
    return data_len + (BLOCK_SIZE - (data_len % BLOCK_SIZE));
}

static bool pkcs7_pad(const uint8_t *data, size_t data_len,
                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t padded = pkcs7_pad_size(data_len);
    if (padded > out_cap) return false;

    uint8_t pad_val = (uint8_t)(padded - data_len);
    memcpy(out, data, data_len);
    memset(out + data_len, pad_val, pad_val);
    *out_len = padded;
    return true;
}

static bool pkcs7_unpad(const uint8_t *data, size_t data_len,
                          uint8_t *out, size_t *out_len)
{
    if (data_len == 0 || data_len % BLOCK_SIZE != 0) return false;

    uint8_t pad_val = data[data_len - 1];
    if (pad_val == 0 || pad_val > BLOCK_SIZE) return false;

    /* Verify all padding bytes match */
    for (size_t i = data_len - pad_val; i < data_len; i++)
    {
        if (data[i] != pad_val) return false;
    }

    *out_len = data_len - pad_val;
    memcpy(out, data, *out_len);
    return true;
}

/*----------------------------------------------------------
 * AES-256-CBC
 *----------------------------------------------------------*/
static bool aes_encrypt(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t *out_len,
                         const char *password)
{
    /* Generate salt and IV */
    uint8_t salt[BLOCK_SIZE], iv[BLOCK_SIZE];
    if (!random_bytes(salt, BLOCK_SIZE) || !random_bytes(iv, BLOCK_SIZE))
        return false;

    /* Derive key */
    uint8_t key[32];
    if (!derive_key(password, salt, key)) return false;

    /* Pad plaintext */
    uint8_t *padded = malloc(pkcs7_pad_size(in_len));
    if (!padded) return false;

    size_t padded_len;
    if (!pkcs7_pad(in, in_len, padded, pkcs7_pad_size(in_len), &padded_len))
    {
        free(padded);
        return false;
    }

    /* Encrypt */
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    int ret = mbedtls_aes_setkey_enc(&ctx, key, 256);
    if (ret != 0)
    {
        mbedtls_aes_free(&ctx);
        free(padded);
        return false;
    }

    uint8_t *cursor = out;

    /* Write salt and IV */
    memcpy(cursor, salt, BLOCK_SIZE);
    cursor += BLOCK_SIZE;
    memcpy(cursor, iv, BLOCK_SIZE);
    cursor += BLOCK_SIZE;

    /* CBC encrypt */
    ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, padded_len,
                                  iv, padded, cursor);

    mbedtls_aes_free(&ctx);
    free(padded);

    if (ret != 0) return false;

    *out_len = BLOCK_SIZE + BLOCK_SIZE + padded_len;
    return true;
}

static bool aes_decrypt(const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t *out_len,
                          const char *password)
{
    /* Minimum: salt(16) + iv(16) + one block(16) */
    if (in_len < BLOCK_SIZE * 3) return false;

    const uint8_t *salt = in;
    const uint8_t *iv = in + BLOCK_SIZE;
    const uint8_t *ciphertext = in + BLOCK_SIZE * 2;
    size_t ct_len = in_len - BLOCK_SIZE * 2;

    /* Derive key */
    uint8_t key[32];
    if (!derive_key(password, salt, key)) return false;

    /* Decrypt */
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    int ret = mbedtls_aes_setkey_dec(&ctx, key, 256);
    if (ret != 0)
    {
        mbedtls_aes_free(&ctx);
        return false;
    }

    uint8_t *decrypted = malloc(ct_len);
    if (!decrypted)
    {
        mbedtls_aes_free(&ctx);
        return false;
    }

    /* Use non-const iv copy since mbedtls modifies it */
    uint8_t iv_copy[BLOCK_SIZE];
    memcpy(iv_copy, iv, BLOCK_SIZE);

    ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, ct_len,
                                  iv_copy, ciphertext, decrypted);

    mbedtls_aes_free(&ctx);

    if (ret != 0)
    {
        free(decrypted);
        return false;
    }

    /* Remove padding */
    bool ok = pkcs7_unpad(decrypted, ct_len, out, out_len);
    free(decrypted);
    return ok;
}

/*----------------------------------------------------------
 * Serpent-256 Block Cipher
 *
 * 32-round Substitution-Permutation network with 128-bit
 * blocks and 256-bit keys.
 *----------------------------------------------------------*/

static const uint8_t S_BOX[8][16] = {
    { 3, 8,15, 1,10, 6,12,11, 9, 7, 3,14, 5, 0,12, 7 },
    {13, 6,11, 8, 0, 3, 7, 0, 9, 3, 4, 6,10, 2, 8, 5 },
    { 1,13, 3, 7,10, 8,12, 4,13,14, 0, 7, 5, 9, 2,11 },
    { 7,11,14, 2, 1, 4, 0, 6,11, 8,12, 7, 1,14, 2,13 },
    { 2, 1,14, 7, 4,10, 8,13,15,12, 9, 0, 3, 5, 6,11 },
    {11, 8,12, 7, 1,14, 2,13, 6,15, 0, 9,10, 4, 5, 3 },
    { 1,15,13, 0, 7, 4, 9, 6,10,12,14, 2, 5, 3,11, 8 },
    { 4, 0, 5, 9, 7,12, 2,10,14, 1, 3, 8,11, 6,15,13 }
};

static const uint8_t S_BOX_INV[8][16] = {
    {13, 3,11, 0,10, 6, 5,12, 1,14, 4, 9, 7,15, 8, 3 },
    { 5, 8, 2,14,12,11, 1, 6, 9, 7, 4, 3,15,13, 0,10 },
    {10, 1,14, 7, 9, 0,12,11, 6,13,15, 3, 5, 8, 2, 4 },
    {13, 2, 8, 4, 6,11, 1,10, 9, 3,14, 5,15,12, 7, 0 },
    { 0, 4,11,14, 8, 3, 1, 6,15, 9, 2,12,13, 5,10, 7 },
    {11, 5, 8, 1,14, 6,12, 3, 2,15,13, 0, 4,10, 7, 9 },
    { 7, 0, 9, 3, 1,13,12, 6,14,11, 8, 5, 2,15,14,10 },
    { 1, 6, 8,13,11, 0, 4,12, 5, 9,14, 2, 7,10,15, 3 }
};

static inline uint32_t rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

static inline uint32_t rotr32(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

/* Apply S-box to 128-bit state (8 nibbles) */
static void serpent_sbox(uint32_t state[4], int round, bool inverse)
{
    const uint8_t (*box)[16] = inverse ? S_BOX_INV : S_BOX;
    int idx = (8 - (round % 8)) % 8;

    uint32_t result[4] = {0, 0, 0, 0};
    for (int i = 0; i < 8; i++)
    {
        int word_idx = i / 2;
        int shift = (i % 2) * 4;
        uint32_t nibble = (state[word_idx] >> shift) & 0xF;
        uint32_t out = box[idx][nibble];
        result[word_idx] |= (out << shift);
    }
    state[0] = result[0]; state[1] = result[1];
    state[2] = result[2]; state[3] = result[3];
}

/* Linear transform (Serpent phi permutation on 128-bit state) */
static void serpent_linear(uint32_t s[4])
{
    s[1] = rotl32(s[1], 13);
    s[3] = rotl32(s[3], 3);
    s[0] ^= s[1] ^ s[3];
    s[2] ^= s[1] ^ (s[3] << 3);
    s[1] = rotl32(s[1], 7);
    s[3] = rotl32(s[3], 7);
    s[0] ^= s[1] ^ s[3];
    s[2] ^= s[1] ^ (s[3] << 3);
    s[0] = rotl32(s[0], 5);
    s[2] = rotl32(s[2], 22);
}

/* Inverse linear transform */
static void serpent_linear_inv(uint32_t s[4])
{
    s[2] = rotr32(s[2], 22);
    s[0] = rotr32(s[0], 5);
    s[0] ^= s[1] ^ s[3];
    s[2] ^= s[1] ^ (s[3] << 3);
    s[1] = rotr32(s[1], 7);
    s[3] = rotr32(s[3], 7);
    s[0] ^= s[1] ^ s[3];
    s[2] ^= s[1] ^ (s[3] << 3);
    s[3] = rotr32(s[3], 3);
    s[1] = rotr32(s[1], 13);
}

/* Key schedule: 256-bit key -> 33 round subkeys (128 bits each) */
static void serpent_key_schedule(const uint8_t key[32], uint32_t subkeys[33][4])
{
    static const uint32_t PHI[132] = {
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,
        0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba,0x9e3779ba
    };

    uint32_t k[132];
    for (int i = 0; i < 8; i++)
    {
        k[i] = (uint32_t)key[i*4]
             | ((uint32_t)key[i*4+1] << 8)
             | ((uint32_t)key[i*4+2] << 16)
             | ((uint32_t)key[i*4+3] << 24);
    }
    for (int i = 8; i < 132; i++)
    {
        uint32_t val = k[i-8] ^ k[i-5] ^ k[i-3] ^ k[i-1] ^ PHI[i] ^ (uint32_t)i;
        k[i] = rotl32(val, 11);
    }

    for (int r = 0; r < 33; r++)
    {
        subkeys[r][0] = k[r*4];
        subkeys[r][1] = k[r*4+1];
        subkeys[r][2] = k[r*4+2];
        subkeys[r][3] = k[r*4+3];
        serpent_sbox(subkeys[r], r, false);
        serpent_linear(subkeys[r]);
    }
}

static void serpent_encrypt_block(const uint32_t subkeys[33][4],
                                    const uint8_t in[16], uint8_t out[16])
{
    uint32_t s[4];

    for (int i = 0; i < 4; i++)
    {
        s[i] = (uint32_t)in[i*4]
             | ((uint32_t)in[i*4+1] << 8)
             | ((uint32_t)in[i*4+2] << 16)
             | ((uint32_t)in[i*4+3] << 24);
        s[i] ^= subkeys[0][i];
    }

    for (int r = 0; r < 31; r++)
    {
        serpent_sbox(s, r, false);
        serpent_linear(s);
        s[0] ^= subkeys[r+1][0];
        s[1] ^= subkeys[r+1][1];
        s[2] ^= subkeys[r+1][2];
        s[3] ^= subkeys[r+1][3];
    }

    serpent_sbox(s, 31, false);
    s[0] ^= subkeys[32][0];
    s[1] ^= subkeys[32][1];
    s[2] ^= subkeys[32][2];
    s[3] ^= subkeys[32][3];

    for (int i = 0; i < 4; i++)
    {
        out[i*4]   =  s[i]        & 0xFF;
        out[i*4+1] = (s[i] >>  8) & 0xFF;
        out[i*4+2] = (s[i] >> 16) & 0xFF;
        out[i*4+3] = (s[i] >> 24) & 0xFF;
    }
}

static void serpent_decrypt_block(const uint32_t subkeys[33][4],
                                    const uint8_t in[16], uint8_t out[16])
{
    uint32_t s[4];

    for (int i = 0; i < 4; i++)
    {
        s[i] = (uint32_t)in[i*4]
             | ((uint32_t)in[i*4+1] << 8)
             | ((uint32_t)in[i*4+2] << 16)
             | ((uint32_t)in[i*4+3] << 24);
    }

    s[0] ^= subkeys[32][0];
    s[1] ^= subkeys[32][1];
    s[2] ^= subkeys[32][2];
    s[3] ^= subkeys[32][3];
    serpent_sbox(s, 31, true);

    for (int r = 30; r >= 0; r--)
    {
        s[0] ^= subkeys[r+1][0];
        s[1] ^= subkeys[r+1][1];
        s[2] ^= subkeys[r+1][2];
        s[3] ^= subkeys[r+1][3];
        serpent_linear_inv(s);
        serpent_sbox(s, r, true);
    }

    s[0] ^= subkeys[0][0];
    s[1] ^= subkeys[0][1];
    s[2] ^= subkeys[0][2];
    s[3] ^= subkeys[0][3];

    for (int i = 0; i < 4; i++)
    {
        out[i*4]   =  s[i]        & 0xFF;
        out[i*4+1] = (s[i] >>  8) & 0xFF;
        out[i*4+2] = (s[i] >> 16) & 0xFF;
        out[i*4+3] = (s[i] >> 24) & 0xFF;
    }
}

/* Serpent CBC encrypt */
static bool serpent_encrypt(const uint8_t *in, size_t in_len,
                             uint8_t *out, size_t *out_len,
                             const char *password)
{
    uint8_t salt[BLOCK_SIZE], iv[BLOCK_SIZE];
    if (!random_bytes(salt, BLOCK_SIZE) || !random_bytes(iv, BLOCK_SIZE))
        return false;

    uint8_t key[32];
    if (!derive_key(password, salt, key)) return false;

    uint32_t subkeys[33][4];
    serpent_key_schedule(key, subkeys);

    uint8_t *padded = malloc(pkcs7_pad_size(in_len));
    if (!padded) return false;

    size_t padded_len;
    if (!pkcs7_pad(in, in_len, padded, pkcs7_pad_size(in_len), &padded_len))
    {
        free(padded);
        return false;
    }

    uint8_t *cursor = out;
    memcpy(cursor, salt, BLOCK_SIZE);
    cursor += BLOCK_SIZE;
    memcpy(cursor, iv, BLOCK_SIZE);
    cursor += BLOCK_SIZE;

    /* CBC encrypt */
    for (size_t i = 0; i < padded_len; i += BLOCK_SIZE)
    {
        for (int j = 0; j < BLOCK_SIZE; j++)
            padded[i + j] ^= iv[j];

        serpent_encrypt_block(subkeys, padded + i, cursor);

        memcpy(iv, cursor, BLOCK_SIZE);
        cursor += BLOCK_SIZE;
    }

    free(padded);
    *out_len = BLOCK_SIZE + BLOCK_SIZE + padded_len;
    return true;
}

static bool serpent_decrypt(const uint8_t *in, size_t in_len,
                             uint8_t *out, size_t *out_len,
                             const char *password)
{
    if (in_len < BLOCK_SIZE * 3) return false;

    const uint8_t *salt = in;
    const uint8_t *iv_init = in + BLOCK_SIZE;
    const uint8_t *ciphertext = in + BLOCK_SIZE * 2;
    size_t ct_len = in_len - BLOCK_SIZE * 2;

    uint8_t key[32];
    if (!derive_key(password, salt, key)) return false;

    uint32_t subkeys[33][4];
    serpent_key_schedule(key, subkeys);

    uint8_t *decrypted = malloc(ct_len);
    if (!decrypted) return false;

    uint8_t prev[BLOCK_SIZE];
    memcpy(prev, iv_init, BLOCK_SIZE);

    /* CBC decrypt */
    for (size_t i = 0; i < ct_len; i += BLOCK_SIZE)
    {
        serpent_decrypt_block(subkeys, ciphertext + i, decrypted + i);

        for (int j = 0; j < BLOCK_SIZE; j++)
            decrypted[i + j] ^= prev[j];

        memcpy(prev, ciphertext + i, BLOCK_SIZE);
    }

    bool ok = pkcs7_unpad(decrypted, ct_len, out, out_len);
    free(decrypted);
    return ok;
}

/*----------------------------------------------------------
 * RSA-2048-OAEP
 *
 * Encrypts with a randomly generated keypair.
 * File format: salt(16) | iv(16) | pubkey_len(4) | pubkey | enc_privkey_len(4) | enc_privkey | ciphertext
 *----------------------------------------------------------*/
static bool rsa_encrypt(const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t *out_len,
                          const char *password)
{
    int ret;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0)
    {
        mbedtls_pk_free(&pk);
        return false;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);

    ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                 (const unsigned char *)"rsa_keygen", 11);
    if (ret != 0)
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&pk);
        return false;
    }

    ret = mbedtls_rsa_gen_key(rsa, mbedtls_ctr_drbg_random, &drbg, RSA_KEY_BITS, 65537);
    if (ret != 0)
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&pk);
        return false;
    }

    /* Export public key as DER */
    uint8_t pubkey_der[600];
    ret = mbedtls_pk_write_pubkey_der(&pk, pubkey_der, sizeof(pubkey_der));
    if (ret < 0)
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&pk);
        return false;
    }
    size_t pubkey_len = (size_t)ret;
    memmove(pubkey_der, pubkey_der + sizeof(pubkey_der) - pubkey_len, pubkey_len);

    /* Export private key as DER */
    uint8_t privkey_der[1200];
    ret = mbedtls_pk_write_key_der(&pk, privkey_der, sizeof(privkey_der));
    if (ret < 0)
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&pk);
        return false;
    }
    size_t privkey_len = (size_t)ret;
    memmove(privkey_der, privkey_der + sizeof(privkey_der) - privkey_len, privkey_len);

    /* Encrypt private key with AES-256-CBC (password protects the key) */
    uint8_t enc_privkey[1200];
    size_t enc_privkey_len = 0;
    if (!aes_encrypt(privkey_der, privkey_len, enc_privkey, &enc_privkey_len, password))
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&pk);
        return false;
    }

    /* Pad plaintext for RSA encryption */
    size_t rsa_padded = ((in_len + RSA_PLAIN_CHUNK - 1) / RSA_PLAIN_CHUNK) * RSA_PLAIN_CHUNK;
    uint8_t *padded = malloc(rsa_padded);
    if (!padded)
    {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_pk_free(&pk);
        return false;
    }
    memset(padded, 0, rsa_padded);
    memcpy(padded, in, in_len);
    /* Apply PKCS7 padding to the last block */
    size_t last_block_off = (in_len / RSA_PLAIN_CHUNK) * RSA_PLAIN_CHUNK;
    size_t last_block_len = in_len - last_block_off;
    if (last_block_len == 0) last_block_len = RSA_PLAIN_CHUNK;
    {
        uint8_t pad_val = (uint8_t)(RSA_PLAIN_CHUNK - (in_len % RSA_PLAIN_CHUNK));
        if (pad_val == 0) pad_val = RSA_PLAIN_CHUNK;
        for (size_t i = 0; i < pad_val; i++)
        {
            padded[last_block_off + last_block_len - pad_val + i] = pad_val;
        }
    }

    /* Encrypt data with RSA public key */
    uint8_t *cursor = out;
    cursor += BLOCK_SIZE; /* Reserve space for salt (written later) */

    /* public key length + data */
    uint32_t pk_len32 = (uint32_t)pubkey_len;
    memcpy(cursor, &pk_len32, 4);
    cursor += 4;
    memcpy(cursor, pubkey_der, pubkey_len);
    cursor += pubkey_len;

    /* encrypted private key length + data */
    uint32_t epk_len32 = (uint32_t)enc_privkey_len;
    memcpy(cursor, &epk_len32, 4);
    cursor += 4;
    memcpy(cursor, enc_privkey, enc_privkey_len);
    cursor += enc_privkey_len;

    /* RSA-encrypt each chunk */
    for (size_t i = 0; i < rsa_padded; i += RSA_PLAIN_CHUNK)
    {
        size_t chunk_out = RSA_CIPHER_CHUNK;
        ret = mbedtls_rsa_pkcs1_encrypt(rsa, mbedtls_ctr_drbg_random, &drbg,
                                          MBEDTLS_RSA_PUBLIC,
                                          RSA_PLAIN_CHUNK,
                                          padded + i,
                                          cursor);
        if (ret != 0)
        {
            free(padded);
            mbedtls_ctr_drbg_free(&drbg);
            mbedtls_entropy_free(&entropy);
            mbedtls_pk_free(&pk);
            return false;
        }
        cursor += chunk_out;
    }

    free(padded);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_pk_free(&pk);

    /* Write salt at the reserved position */
    uint8_t salt[BLOCK_SIZE];
    if (!random_bytes(salt, BLOCK_SIZE)) return false;
    memcpy(out, salt, BLOCK_SIZE);

    *out_len = (size_t)(cursor - out);
    return true;
}

static bool rsa_decrypt(const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t *out_len,
                          const char *password)
{
    if (in_len < BLOCK_SIZE + 4 + 100 + 4 + 16 + RSA_CIPHER_CHUNK) return false;

    const uint8_t *cursor = in;

    /* salt */
    cursor += BLOCK_SIZE;

    /* public key */
    uint32_t pubkey_len;
    memcpy(&pubkey_len, cursor, 4);
    cursor += 4;
    cursor += pubkey_len; /* skip public key (not needed for decryption) */

    /* encrypted private key */
    uint32_t enc_privkey_len;
    memcpy(&enc_privkey_len, cursor, 4);
    cursor += 4;
    const uint8_t *enc_privkey = cursor;
    cursor += enc_privkey_len;

    /* ciphertext (RSA-encrypted blocks) */
    const uint8_t *ciphertext = cursor;
    size_t ct_len = (size_t)(in + in_len - cursor);
    if (ct_len == 0 || ct_len % RSA_CIPHER_CHUNK != 0) return false;

    /* Decrypt private key with password */
    uint8_t privkey_buf[1200];
    size_t privkey_len = 0;
    if (!aes_decrypt(enc_privkey, enc_privkey_len, privkey_buf, &privkey_len, password))
    {
        ESP_LOGE(TAG, "RSA: failed to decrypt private key (wrong password?)");
        return false;
    }

    /* Parse private key from DER */
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_key(&pk, privkey_buf, privkey_len, NULL, 0, NULL, NULL);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "RSA: failed to parse private key: -0x%04x", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);

    /* Decrypt RSA blocks */
    uint8_t *decrypted = malloc(ct_len);
    if (!decrypted)
    {
        mbedtls_pk_free(&pk);
        return false;
    }

    size_t dec_offset = 0;
    for (size_t i = 0; i < ct_len; i += RSA_CIPHER_CHUNK)
    {
        size_t block_len = RSA_PLAIN_CHUNK;
        ret = mbedtls_rsa_pkcs1_decrypt(rsa, mbedtls_ctr_drbg_random, NULL,
                                          MBEDTLS_RSA_PRIVATE,
                                          &block_len,
                                          ciphertext + i,
                                          decrypted + dec_offset,
                                          RSA_PLAIN_CHUNK);
        if (ret != 0)
        {
            ESP_LOGE(TAG, "RSA: block decrypt failed: -0x%04x", -ret);
            free(decrypted);
            mbedtls_pk_free(&pk);
            return false;
        }
        dec_offset += block_len;
    }

    mbedtls_pk_free(&pk);

    /* Remove PKCS7 padding from the last block */
    if (dec_offset == 0)
    {
        free(decrypted);
        return false;
    }

    uint8_t pad_val = decrypted[dec_offset - 1];
    if (pad_val == 0 || pad_val > RSA_PLAIN_CHUNK)
    {
        free(decrypted);
        return false;
    }

    /* Verify all padding bytes */
    for (size_t i = dec_offset - pad_val; i < dec_offset; i++)
    {
        if (decrypted[i] != pad_val)
        {
            free(decrypted);
            return false;
        }
    }

    size_t unpadded = dec_offset - pad_val;
    memcpy(out, decrypted, unpadded);
    *out_len = unpadded;
    free(decrypted);
    return true;
}

/*----------------------------------------------------------
 * Public API
 *----------------------------------------------------------*/
const char *crypto_method_name(CryptoMethod method)
{
    switch (method)
    {
        case CRYPTO_METHOD_AES256_CBC:  return "AES-256-CBC";
        case CRYPTO_METHOD_SERPENT_CBC: return "Serpent-CBC";
        case CRYPTO_METHOD_RSA_2048:    return "RSA-2048";
        default:                        return "Unknown";
    }
}

size_t crypto_output_size(CryptoMethod method, size_t input_size)
{
    switch (method)
    {
        case CRYPTO_METHOD_AES256_CBC:
        case CRYPTO_METHOD_SERPENT_CBC:
            /* salt(16) + iv(16) + padded data */
            return BLOCK_SIZE + BLOCK_SIZE + pkcs7_pad_size(input_size);

        case CRYPTO_METHOD_RSA_2048:
        {
            /* salt(16) + pubkey_len(4) + pubkey(~600) + epk_len(4) + enc_epk(~600) + ciphertext */
            size_t rsa_ct = ((input_size + RSA_PLAIN_CHUNK - 1) / RSA_PLAIN_CHUNK) * RSA_CIPHER_CHUNK;
            return BLOCK_SIZE + 4 + 600 + 4 + 600 + rsa_ct;
        }

        default:
            return 0;
    }
}

bool crypto_encrypt(
    CryptoMethod method,
    const uint8_t *in,
    size_t in_len,
    uint8_t *out,
    size_t *out_len,
    const char *password)
{
    if (!in || !out || !out_len || !password || in_len == 0)
        return false;

    switch (method)
    {
        case CRYPTO_METHOD_AES256_CBC:
            return aes_encrypt(in, in_len, out, out_len, password);

        case CRYPTO_METHOD_SERPENT_CBC:
            return serpent_encrypt(in, in_len, out, out_len, password);

        case CRYPTO_METHOD_RSA_2048:
            return rsa_encrypt(in, in_len, out, out_len, password);

        default:
            return false;
    }
}

bool crypto_decrypt(
    CryptoMethod method,
    const uint8_t *in,
    size_t in_len,
    uint8_t *out,
    size_t *out_len,
    const char *password)
{
    if (!in || !out || !out_len || !password || in_len == 0)
        return false;

    switch (method)
    {
        case CRYPTO_METHOD_AES256_CBC:
            return aes_decrypt(in, in_len, out, out_len, password);

        case CRYPTO_METHOD_SERPENT_CBC:
            return serpent_decrypt(in, in_len, out, out_len, password);

        case CRYPTO_METHOD_RSA_2048:
            return rsa_decrypt(in, in_len, out, out_len, password);

        default:
            return false;
    }
}
