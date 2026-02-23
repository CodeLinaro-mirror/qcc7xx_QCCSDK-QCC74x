#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include "qcc74x_sec_aes.h"
#include "qcc74x_ef_ctrl.h"
#include "qcc74x_l1c.h"
#include "qcc74x_sec_trng.h"
#include "utils_crypto.h"

//#define UTILS_CRYPTO_PROBE_CHIPER       "EFUSE_KEY_TEST"
static const uint8_t probe_plaintext[] = {
    0x45, 0x46, 0x55, 0x53, 0x45, 0x5f, 0x4b, 0x45,
    0x59, 0x5f, 0x54, 0x45, 0x53, 0x54, 0x02, 0x02,
};

static const uint8_t expected_ciphertext[] = {
    0x50, 0x74, 0xF4, 0x6E, 0x33, 0x9F, 0x49, 0x4C,
    0x1A, 0x25, 0x36, 0x8C, 0xEB, 0x7F, 0x8D, 0x7F
};

static const uint8_t probe_iv[UTILS_CRYPTO_AES_BLOCK_SIZE] = {
    0x83, 0x29, 0xe0, 0xc7, 0x6d, 0xa1, 0xb5, 0x08,
    0x1a, 0x7c, 0x3d, 0xf2, 0x0b, 0x5e, 0x9f, 0x44,
};

int crypto_get_random_iv(uint8_t iv[16])
{
    if (iv == NULL) {
        return -1;
    }

    qcc74x_trng_readlen(iv, UTILS_CRYPTO_AES_BLOCK_SIZE);

    return 0;
}

int crypto_aes_key_init(void)
{
    uint8_t actual_ciphertext[16];
    struct qcc74x_device_s *efuse_dev;
    uint32_t key_data[UTILS_CRYPTO_AES_KEY_SIZE / 4];

#if 0
    uint8_t key[16] = {0};
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, sizeof(probe_plaintext), probe_iv, probe_plaintext, actual_ciphertext);
    mbedtls_aes_free(&aes);

    for (int i = 0; i < sizeof(actual_ciphertext); i++) {
        printf("0x%02x ", actual_ciphertext[i]);
    }
    printf("\r\n\r\n");

    for (int i = 0; i < sizeof(probe_plaintext); i++) {
        printf("0x%02x ", probe_plaintext[i]);
    }
    printf("\r\n\r\n");
#endif
    crypto_aes_cbc_encrypt(probe_iv, sizeof(probe_plaintext), probe_plaintext, actual_ciphertext);

    if (memcmp(expected_ciphertext, actual_ciphertext, sizeof(expected_ciphertext)) == 0) {
        printf("This is an unencrypted chip that is about to write a random key...\r\n");
        efuse_dev = qcc74x_device_get_by_name("ef_ctrl");
        qcc74x_trng_readlen((uint8_t *)key_data, UTILS_CRYPTO_AES_KEY_SIZE);
        qcc74x_ef_ctrl_write_direct(efuse_dev, 0x3c, key_data, UTILS_CRYPTO_AES_KEY_SIZE / 4, 0);
        qcc74x_ef_ctrl_write_direct(efuse_dev, 0, NULL, 0, 1);
        memset(key_data, 0, sizeof(key_data));
    }

    printf("slot2 r/w lock...\r\n");
    //slot2 ==> key_index=0
    qcc74x_efuse_rw_lock_aes_key(0, 1, 1);

    return 0;
}

int crypto_aes_cbc_encrypt(const uint8_t iv[16], size_t length,
                           const uint8_t *input, uint8_t *output)
{
    int ret = -1;
    uintptr_t flag;
    struct qcc74x_device_s *aes;
    uint8_t *aligned_input;
    uint8_t *aligned_output;

    if (length % UTILS_CRYPTO_AES_BLOCK_SIZE != 0) {
        goto _fail1;
    }
    aligned_input = memalign(32, length);
    if (!aligned_input) {
        goto _fail1;
    }
    aligned_output = memalign(32, length);
    if (!aligned_output) {
        goto _fail2;
    }

    memcpy(aligned_input, input, length);
    qcc74x_l1c_dcache_clean_invalidate_range(aligned_input, length);
    qcc74x_l1c_dcache_clean_invalidate_range(aligned_output, length);

    flag = qcc74x_irq_save();
    aes = qcc74x_device_get_by_name("aes");

    qcc74x_aes_link_deinit(aes);
    qcc74x_group0_request_aes_access(aes);
    qcc74x_aes_init(aes);
    qcc74x_aes_set_hwkey_source(aes, 0);
    qcc74x_aes_select_hwkey(aes, 2, 0);
    qcc74x_aes_set_mode(aes, AES_MODE_CBC);
    qcc74x_aes_setkey(aes, NULL, 128);

    ret = qcc74x_aes_encrypt(aes, aligned_input, iv, aligned_output, length);

    qcc74x_aes_deinit(aes);
    qcc74x_group0_release_aes_access(aes);

    qcc74x_irq_restore(flag);
    if (ret != 0) {
        ret = -1;
        goto _fail3;
    }

    qcc74x_l1c_dcache_invalidate_range(aligned_output, length);

    memcpy(output, aligned_output, length);

    memset(aligned_input, 0, sizeof(aligned_input));

_fail3:
    free(aligned_output);
_fail2:
    free(aligned_input);
_fail1:

    return ret;
}

int crypto_aes_cbc_decrypt(const uint8_t iv[16], size_t length,
                           const uint8_t *input, uint8_t *output)
{
    int ret = -1;
    uintptr_t flag;
    struct qcc74x_device_s *aes;
    uint8_t *aligned_input;
    uint8_t *aligned_output;

    if (length % UTILS_CRYPTO_AES_BLOCK_SIZE != 0) {
        goto _fail1;
    }

    aligned_input = memalign(32, length);
    if (!aligned_input) {
        goto _fail1;
    }
    aligned_output = memalign(32, length);
    if (!aligned_output) {
        goto _fail2;
    }

    memcpy(aligned_input, input, length);

    qcc74x_l1c_dcache_clean_invalidate_range(aligned_input, length);
    qcc74x_l1c_dcache_clean_invalidate_range(aligned_output, length);

    flag = qcc74x_irq_save();
    aes = qcc74x_device_get_by_name("aes");

    qcc74x_aes_link_deinit(aes);
    qcc74x_group0_request_aes_access(aes);
    qcc74x_aes_init(aes);
    qcc74x_aes_set_hwkey_source(aes, 0);
    qcc74x_aes_select_hwkey(aes, 2, 0);
    qcc74x_aes_set_mode(aes, AES_MODE_CBC);
    qcc74x_aes_setkey(aes, NULL, 128);

    ret = qcc74x_aes_decrypt(aes, aligned_input, iv, aligned_output, length);

    qcc74x_aes_deinit(aes);
    qcc74x_group0_release_aes_access(aes);
    qcc74x_irq_restore(flag);

    if (ret != 0) {
        ret = -1;
        goto _fail3;
    }

    qcc74x_l1c_dcache_invalidate_range(aligned_output, length);

    memcpy(output, aligned_output, length);
    memset(aligned_output, 0, sizeof(aligned_output));

_fail3:
    free(aligned_output);
_fail2:
    free(aligned_input);
_fail1:
    return ret;
}

int crypto_aes_cbc_decrypt_unpad(const uint8_t *input, uint32_t input_len,
                         uint8_t *output, uint32_t *output_len,
                         const uint8_t iv[16])
{
    uint8_t pad_value;

    if (input == NULL || output == NULL || output_len == NULL || iv == NULL) {
        return -1;
    }

    if (crypto_aes_cbc_decrypt(iv, input_len, input, output) != 0) {
        return -1;
    }

    pad_value = output[input_len - 1];
    if (pad_value == 0 || pad_value > UTILS_CRYPTO_AES_BLOCK_SIZE) {
        memset(output, 0, input_len);
        return -1;
    }
    for (uint8_t i = 0; i < pad_value; i++) {
        if (output[input_len - 1 - i] != pad_value) {
            memset(output, 0, input_len);
            return -1;
        }
    }

    *output_len = input_len - pad_value;
    memset(output + *output_len, 0, pad_value);

    return 0;
}

int crypto_aes_cbc_encrypt_pad(const uint8_t *input, uint32_t input_len,
                       uint8_t *output, uint32_t *output_len,
                       const uint8_t iv[16])
{
    uint32_t padded_len;
    uint8_t pad_value;

    if (input == NULL || output == NULL || output_len == NULL || iv == NULL || input_len == 0) {
        return -1;
    }

    padded_len = ((input_len / UTILS_CRYPTO_AES_BLOCK_SIZE) + 1) * UTILS_CRYPTO_AES_BLOCK_SIZE;
    pad_value = padded_len - input_len;

    memcpy(output, input, input_len);
    memset(output + input_len, pad_value, pad_value);

    if (crypto_aes_cbc_encrypt(iv, padded_len, output, output) != 0) {
        return -1;
    }

    *output_len = padded_len;
    return 0;
}
