/**
 * This file is based on ST67W6X_Network_Driver provided by STMicroelectronics.
 * See
 * https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Middlewares/ST/ST67W6X_Network_Driver
 * for more information.
 *
 * Reference source:
 * https://github.com/STMicroelectronics/x-cube-st67w61/blob/main/Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_default_config.h
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef RCP_DEFAULT_CONFIG_H
#define RCP_DEFAULT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "rcp_driver_config.h"

/* Exported constants --------------------------------------------------------*/
/** @addtogroup RCP_WiFi_Constants
 * @{
 */

#ifndef RCP_WIFI_MAX_DETECTED_AP
/** Maximum number of detected AP during the scan. Cannot be greater than 50 */
#define RCP_WIFI_MAX_DETECTED_AP 20
#endif /* RCP_WIFI_MAX_DETECTED_AP */

#ifndef WIFI_LOG_ENABLE
/** Enable/Disable Wi-Fi module logging */
#define WIFI_LOG_ENABLE 0
#endif /* WIFI_LOG_ENABLE */

/** @} */

/** @addtogroup RCP_BLE_Constants
 * @{
 */

#ifndef RCP_BLE_MAX_CONN_NBR
/** Maximum number of BLE connections. Must not be greater than 10.
 * The maximum connections depend on the mode as follows:
 * - Server mode (10 connection max)
 * - Client mode (9 connection max)
 * - Dual Mode (10 connections max with up to 9 as Client)
 */
#define RCP_BLE_MAX_CONN_NBR 2
#endif /* RCP_BLE_MAX_CONN_NBR */

/** Maximum number of BLE application services that can be created */
#define RCP_BLE_MAX_CREATED_SERVICE_NBR 3

/** Maximum number of BLE services supported including Generic access and
 * Generic attributes predefined services */
#define RCP_BLE_MAX_SERVICE_NBR RCP_BLE_MAX_CREATED_SERVICE_NBR + 2

/** Maximum number of BLE characteristics per service */
#define RCP_BLE_MAX_CHAR_NBR 5

#ifndef RCP_BLE_MAX_DETECTED_PERIPHERAL
/** Maximum number of detected peripheral during the scan. Cannot be greater
 * than 50 */
#define RCP_BLE_MAX_DETECTED_PERIPHERAL 10
#endif /* RCP_BLE_MAX_DETECTED_PERIPHERAL */

/** BLE Service/Characteristic UUID maximum size size */
#define RCP_BLE_MAX_UUID_SIZE 17

/** Maximum number of bonded devices */
#define RCP_BLE_MAX_BONDED_DEVICES 2

#ifndef BLE_LOG_ENABLE
/** Enable/Disable BLE module logging */
#define BLE_LOG_ENABLE 0
#endif /* BLE_LOG_ENABLE */

/** @} */

/** @addtogroup RCP_Net_Constants
 * @{
 */

#ifndef RCP_NET_IPV6_ENABLE
/** Enable IPv6 support : 0: Disabled, 1: Enabled */
#define RCP_NET_IPV6_ENABLE 0
#endif /* RCP_NET_IPV6_ENABLE */

#ifndef NET_LOG_ENABLE
/** Enable/Disable Network module logging */
#define NET_LOG_ENABLE 0
#endif /* NET_LOG_ENABLE */

/** @} */

/** @addtogroup RCP_MQTT_Constants
 * @{
 */

#ifndef MQTT_LOG_ENABLE
/** Enable/Disable MQTT module logging */
#define MQTT_LOG_ENABLE 0
#endif /* MQTT_LOG_ENABLE */

/** @} */

/** @addtogroup RCP_Common_Constants
 * @{
 */

#ifndef RCP_MAX_XFER
/** Maximum buffer size */
#define RCP_MAX_XFER 1520
#endif /* RCP_MAX_XFER */

#if ((RCP_MAX_XFER < 1520) || (RCP_MAX_XFER > (6 * 1024)))
#error "RCP_MAX_XFER must be between 1520 and (6*1024)"
#endif /* RCP_MAX_XFER */

#ifndef SYS_LOG_ENABLE
/** Enable/Disable System module logging */
#define SYS_LOG_ENABLE 1
#endif /* SYS_LOG_ENABLE */

#ifndef RCP_LOG_ENABLE
/** Debugging only: Enable/Disable command log, i.e. logs the commands
 * incoming/outcoming from/to the NCP */
#define RCP_LOG_ENABLE 1
#endif /* RCP_LOG_ENABLE */

#ifndef MDM_CMD_LOG_ENABLE
/** Enable/Disable Modem command log */
#define MDM_CMD_LOG_ENABLE RCP_LOG_ENABLE
#endif /* MDM_CMD_LOG_ENABLE */

#ifndef RCP_ASSERT_ENABLE
/** Enable/Disable NULL pointer check in the functions.
 * 0: Disabled, 1: Enabled */
#define RCP_ASSERT_ENABLE 1
#endif /* RCP_ASSERT_ENABLE */

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RCP_DEFAULT_CONFIG_H */
