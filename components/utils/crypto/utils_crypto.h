#ifndef __UTILS_CRYPTO_H__
#define __UTILS_CRYPTO_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UTILS_CRYPTO_AES_KEY_SIZE        16
#define UTILS_CRYPTO_AES_BLOCK_SIZE      16
#define UTILS_CRYPTO_MAX_DATA_LEN        64

int crypto_get_random_iv(uint8_t iv[16]);

int crypto_aes_key_init(void);

int crypto_aes_cbc_encrypt(const uint8_t iv[16], size_t length,
                           const uint8_t *input, uint8_t *output);

int crypto_aes_cbc_decrypt(const uint8_t iv[16], size_t length,
                           const uint8_t *input, uint8_t *output);

int crypto_aes_cbc_decrypt_unpad(const uint8_t *input, uint32_t input_len,
                         uint8_t *output, uint32_t *output_len,
                         const uint8_t iv[16]);

int crypto_aes_cbc_encrypt_pad(const uint8_t *input, uint32_t input_len,
                       uint8_t *output, uint32_t *output_len,
                       const uint8_t iv[16]);

#ifdef __cplusplus
}
#endif

#endif

