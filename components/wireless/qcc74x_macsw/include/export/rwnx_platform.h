/**
 ****************************************************************************************
 *
 * @file rwnx_platform.h
 *
 * @brief RWNX reference platform initialization.
 *
 * Copyright (C) RivieraWaves 2019-2021
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup PLATFORM_DRIVERS
 * @{
 ****************************************************************************************
 */

#include "export/mac/mac_types.h"
/**
 ****************************************************************************************
 * @brief Initialize RWNX reference platform.
 *
 * It initializes all platform specific drivers (IPC, CRM, DMA, ...)
 ****************************************************************************************
 */
void rwnx_platform_init(void);

/**
 ****************************************************************************************
 * @brief Get the platform RTC counter value.
 *
 * The returned value is the raw 32 kHz RTC counter. MACSW users that need a time value
 * in microseconds must convert it with the platform RTC counter conversion macro.
 ****************************************************************************************
 */
uint64_t rwnx_platform_get_rtc_cnt(void);

#ifdef CFG_QCC74x_WIFI_PS_ENABLE
/**
 ****************************************************************************************
 * @brief Initialize RWNX reference platform.
 *
 * It initializes all platform specific drivers (IPC, CRM, DMA, ...)
 ****************************************************************************************
 */
void rwnx_platform_pds_init(void);
#endif

//void rwnx_lpfw_init(struct mac_addr const *mac, struct mac_addr const *bssid);
/// @}
