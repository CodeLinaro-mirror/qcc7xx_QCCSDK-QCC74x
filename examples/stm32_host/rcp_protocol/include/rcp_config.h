#ifndef __RCP_CONFIG_H__
#define __RCP_CONFIG_H__

/* --- User-provided configuration ----------------------------------------- */
#include "rcp_user_config.h"

/* --- Default values and available configurations ------------------------- */

/* Logging:
 * 1 - ON
 * 0 - OFF
 */
#ifndef RCP_LOG
#define RCP_LOG 1
#endif

/* Log level:
 * 0 - None
 * 1 - ERR
 * 2 - WRN
 * 3 - INF
 * 4 - DBG
 */
#ifndef RCP_LOG_LEVEL
#define RCP_LOG_LEVEL 3
#endif

/*
* BUS definition
*/

#ifndef RCP_LNK_MGMT_BUS
#define RCP_LNK_MGMT_BUS RCP_DEFAULT_BUS
#endif

#ifndef RCP_BT_BUS
#define RCP_BT_BUS RCP_DEFAULT_BUS
#endif

#ifndef RCP_WIFI_BUS
#define RCP_WIFI_BUS RCP_DEFAULT_BUS
#endif

#ifndef RCP_TEST_BUS
#define RCP_TEST_BUS RCP_DEFAULT_BUS
#endif

/*
 * Max Number of transmiited messages allowed to have pending ack
 * Must be less that RPC_SEQ_NUM_MAX
 */
#ifndef RCP_SEQ_NUM_ACK_WINDOW
#define RCP_SEQ_NUM_ACK_WINDOW 5
#endif

/* Number of RCP message buffers available for use. */
#ifndef RCP_NUM_BUFFERS
#define RCP_NUM_BUFFERS 5
#endif

#ifndef RCP_RX_NUM_BUFFERS
#define RCP_RX_NUM_BUFFERS 10
#endif

#ifndef RCP_WIFI_NUM_BUFFERS
#define RCP_WIFI_NUM_BUFFERS RCP_MAX_TX_EVENTS
#endif

/* Buffer size of single RCP message buffer in bytes */
#ifndef RCP_BUF_SIZE
#define RCP_BUF_SIZE 500
#endif

#ifndef RCP_MAX_TX_EVENTS
#define RCP_MAX_TX_EVENTS 12
#endif

#ifndef RCP_MAX_RX_EVENTS
#define RCP_MAX_RX_EVENTS (RCP_RX_NUM_BUFFERS - 5)
#endif

#ifndef RCP_UPLD_CREDIT_UPDATE_THRESHOLD
#define RCP_UPLD_CREDIT_UPDATE_THRESHOLD 2U
#endif

/* Maximum WiFi event callback functions. */
#ifndef RCP_WIFI_MAX_CB_CNT
#define RCP_WIFI_MAX_CB_CNT 2
#endif

/* Buffer number of external buffer wrappers supported */
#ifndef RCP_NUM_EXT_BUFFERS
#define RCP_NUM_EXT_BUFFERS 5
#endif

/* SDIO bus frequency - configured by the host, no effect on RCP device. */
#ifndef RCP_SDIO_FREQUENCY_HZ
#define RCP_SDIO_FREQUENCY_HZ 25000000
#endif

/* SDIO wide (4-bit) bus mode - configured by the host, no effect on RCP device. */
#ifndef RCP_SDIO_BUS_WIDE
#define RCP_SDIO_BUS_WIDE 1
#endif

#if !defined(RCP_OS_ZEPHYR) && !defined(RCP_OS_FREERTOS) && !defined(RCP_OS_LINUX)
#error "RCP OS not defined - define RCP_OS_ZEPHYR or RCP_OS_FREERTOS or RCP_OS_LINUX"
#endif

#if (defined(RCP_OS_ZEPHYR) && defined(RCP_OS_FREERTOS)) ||                                        \
	(defined(RCP_OS_ZEPHYR) && defined(RCP_OS_LINUX)) ||                                       \
	(defined(RCP_OS_FREERTOS) && defined(RCP_OS_LINUX))
#error "Only one OS can be defined - RCP_OS_ZEPHYR, RCP_OS_FREERTOS or RCP_OS_LINUX"
#endif

#endif
