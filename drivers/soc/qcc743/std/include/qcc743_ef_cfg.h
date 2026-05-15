#ifndef __QCC743_EF_CFG_H__
#define __QCC743_EF_CFG_H__

#include "qcc74x_ef_ctrl.h"
#include "qcc743_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup  QCC743_Peripheral_Driver
 *  @{
 */

/** @addtogroup  EF_CTRL
 *  @{
 */

/** @defgroup  EF_CTRL_Public_Types
 *  @{
 */

/*@} end of group EF_CTRL_Public_Types */

/** @defgroup  EF_CTRL_Public_Constants
 *  @{
 */

/*@} end of group EF_CTRL_Public_Constants */

/** @defgroup  EF_CTRL_Public_Macros
 *  @{
 */

/*@} end of group EF_CTRL_Public_Macros */

/** @defgroup  EF_CTRL_Public_Functions
 *  @{
 */
void qcc74x_efuse_read_secure_boot(uint8_t *sign, uint8_t *aes);

void qcc74x_efuse_read_secure_boot_app(uint8_t *sign, uint8_t *aes);

int qcc74x_efuse_enable_aes(uint8_t aes_type, uint8_t xts_mode);

int qcc74x_efuse_rw_lock_aes_key(uint8_t key_index, uint8_t rd_lock, uint8_t wr_lock);

int qcc74x_efuse_rw_lock_dbg_key(uint8_t rd_lock, uint8_t wr_lock);

int qcc74x_efuse_write_lock_pk_hash(uint32_t pkhash_len);

int qcc74x_efuse_write_lock_usb_pid_vid(void);

/*@} end of group EF_CTRL_Public_Functions */

/*@} end of group EF_CTRL */

/*@} end of group QCC743_Peripheral_Driver */

#ifdef __cplusplus
}
#endif

#endif /* __QCC743_EF_CFG_H__ */
