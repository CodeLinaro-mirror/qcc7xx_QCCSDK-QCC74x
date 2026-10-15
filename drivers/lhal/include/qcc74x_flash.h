#ifndef _QCC74x_FLASH_H
#define _QCC74x_FLASH_H

#include "qcc74x_core.h"

/** @addtogroup LHAL
  * @{
  */

/** @addtogroup FLASH
  * @{
  */

/** @defgroup FLASH_IOMODE flash iomode definition
  * @{
  */
#define FLASH_IOMODE_NIO 0 /*!< Normal IO mode define */
#define FLASH_IOMODE_DO  1 /*!< Dual Output mode define */
#define FLASH_IOMODE_QO  2 /*!< Quad Output mode define */
#define FLASH_IOMODE_DIO 3 /*!< Dual IO mode define */
#define FLASH_IOMODE_QIO 4 /*!< Quad IO mode define */
/**
  * @}
  */

/** @defgroup FLASH_AES_KEY_TYPE flash aes key type definition
  * @{
  */
#define FLASH_AES_KEY_128BITS        0
#define FLASH_AES_KEY_192BITS        2
#define FLASH_AES_KEY_256BITS        1
#define FLASH_AES_KEY_DOUBLE_128BITS 3

/** Wait indefinitely when acquiring the platform flash resource lock. */
#define QCC74x_FLASH_RESOURCE_WAIT_FOREVER UINT32_MAX
/**
  * @}
  */

#if defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
#define FLASH_XIP_BASE (0x23000000)
#elif defined(QCC743)
#define FLASH_XIP_BASE (0xA0000000)
#elif defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
#define FLASH_XIP_BASE (0x80000000)
#elif defined(QCC74x_undef) || defined(QCC74x_undef)
#define FLASH_XIP_BASE (0x58000000)
#endif

struct qcc74x_flash_aes_config_s {
    uint8_t region;
    uint8_t region_enable;
    uint8_t lock_enable;
    const uint8_t *key;
    uint8_t keybits;
    uint8_t *iv;
    uint32_t start_addr;
    uint32_t end_addr;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize flash.
 *
 * @return Zero on success; a negated errno value on failure
 */
int qcc74x_flash_init(void);

/**
 * @brief Get flash jedec id.
 *
 * @return uint32_t
 */
uint32_t qcc74x_flash_get_jedec_id(void);

/**
 * @brief get flash size
 *
 * @return flash size
 */
uint32_t qcc74x_flash_get_size(void);

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
/**
 * @brief get flash2 size
 *
 * @return flash2 size
 */
uint32_t qcc74x_flash2_get_size(void);
#endif

/**
 * @brief Get flash config.
 *
 * @param [out] cfg_addr pointer to save config
 * @param [in] len flash config size
 */
void qcc74x_flash_get_cfg(uint8_t **cfg_addr, uint32_t *len);

/**
 * @brief Set flash iomode.
 *
 * @param [in] iomode flash iomode, use @ref FLASH_IOMODE
 */
void qcc74x_flash_set_iomode(uint8_t iomode);

/**
 * @brief Get flash image offset.
 *
 * @return flash image offset
 */
uint32_t qcc74x_flash_get_image_offset(void);

/**
 * @brief Acquire exclusive access to the flash/XIP resource.
 *
 * The weak default implementation returns -ENOSYS. A platform that defines
 * CONFIG_DISABLE_FLASH_OP_IRQ_SAVE_RESTORE must override this hook. The hook,
 * its complete call path, and all state accessed while flash XIP is unavailable
 * must reside in RAM, PSRAM, or ROM. ISR implementations must not block.
 * DMA users that access flash XIP must hold the same resource for the complete
 * DMA transfer.
 *
 * @param [in] timeout_ms maximum wait time in milliseconds, or
 *                        QCC74x_FLASH_RESOURCE_WAIT_FOREVER
 * @return Zero on success; a negated errno value on failure
 */
int qcc74x_flash_resource_lock(uint32_t timeout_ms);

/**
 * @brief Release exclusive access to the flash/XIP resource.
 *
 * This must be paired with a successful qcc74x_flash_resource_lock() call and
 * released by an execution context permitted by the platform lock policy.
 */
void qcc74x_flash_resource_unlock(void);

/**
 * @brief Erase flash with sectors.
 *
 * @param [in] addr
 * @param [in] len
 * @return Zero on success; a negated errno value on failure
 */
int qcc74x_flash_erase(uint32_t addr, uint32_t len);

/**
 * @brief Write data into flash.
 *
 * @param [in] addr flash physical address
 * @param [in] data pointer to data buffer
 * @param [in] len length of data
 * @return Zero on success; a negated errno value on failure
 */
int qcc74x_flash_write(uint32_t addr, uint8_t *data, uint32_t len);

/**
 * @brief Read data from flash.
 *
 * @param [in] addr flash physical address
 * @param [out] data pointer to data buffer
 * @param [in] len length of data
 * @return Zero on success; a negated errno value on failure
 */
int qcc74x_flash_read(uint32_t addr, uint8_t *data, uint32_t len);

/**
 * @brief read flash unique id
 *
 * @param data
 * @param id_len
 * @return int
 */
int qcc74x_flash_get_unique_id(uint8_t *data, uint8_t id_len);

/**
 * @brief Config flash cache.
 *
 * @param [in] cont_read enable or not continuous read mode.
 * @param [in] cache_enable enable cache or not
 * @param [in] cache_way_disable ways of cache to disable
 * @param [in] flash_offset flash image offset
 * @return Zero on success; a negated errno value on failure
 */
int qcc74x_flash_set_cache(uint8_t cont_read, uint8_t cache_enable, uint8_t cache_way_disable, uint32_t flash_offset);

/**
 * @brief Initialize flash aes controller.
 *
 * @param [in] config pointer to flash aes config.
 */
void qcc74x_flash_aes_init(struct qcc74x_flash_aes_config_s *config);

/**
 * @brief Enable flash decrypt with aes.
 *
 */
void qcc74x_flash_aes_enable(void);

/**
 * @brief Disable flash decrypt with aes.
 *
 */
void qcc74x_flash_aes_disable(void);

/**
 * @brief Initialize flash jump to encrypted app.
 *
 * @param [in] index region index
 * @param [in] flash_addr flash physical address.
 * @param [in] len firmware length.
 */
void qcc74x_flash_jump_encrypted_app(uint8_t index, uint32_t flash_addr, uint32_t len);

#ifdef __cplusplus
}
#endif

/**
  * @}
  */

/**
  * @}
  */

#endif
