/**
 * @file nh_sdio_drv.h
 * @brief SDIO2 driver wrapper header file
 * @author mlwang
 * @date 2025-07-17
 */

#ifndef nh_SDIO_DRV_H
#define nh_SDIO_DRV_H

#include "nh_msg_ctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Function Declarations
 *****************************************************************************/

/**
 * @brief Initialize SDIO driver
 * @retval 0 Success
 * @retval -1 Error
 */

#ifdef USE_SDIO2_DRIVER
nh_msg_ctrl_ops_t *sdio2_driver_init(void);
#elif defined(USE_SDIO3_DRIVER)
nh_msg_ctrl_ops_t *sdio3_driver_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* nh_SDIO_DRV_H */
