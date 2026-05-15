/**
 * This file is based on ST67W6X_Network_Driver provided by STMicroelectronics.
 * See
 * https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Middlewares/ST/ST67W6X_Network_Driver
 * for more information.
 *
 * Reference source:
 * https://github.com/STMicroelectronics/x-cube-st67w61/blob/main/Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_api.h
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef RCP_API_H
#define RCP_API_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "log.h"
#include "modem_cmd_handler.h"
#include "rcp_default_config.h"
#include "rcp_os.h"
#include <stdbool.h>
/** @defgroup RCP RCP Driver
 */

/** @defgroup RCP_System RCP Driver System
 * @ingroup  RCP
 */

/** @defgroup RCP_WiFi RCP Driver Wi-Fi
 * @ingroup  RCP
 */

/** @defgroup RCP_Net RCP Driver Net
 * @ingroup  RCP
 */

/** @defgroup RCP_MQTT RCP Driver MQTT
 * @ingroup  RCP
 */

/** @defgroup RCP_BLE RCP Driver BLE
 * @ingroup  RCP
 */

/** @defgroup RCP_Netif RCP Driver Network Interface
 * @ingroup  RCP
 */

/* Exported constants --------------------------------------------------------*/
/* ===================================================================== */
/** @defgroup RCP_System_Constants RCP Driver System Constants
 * @ingroup  RCP_System
 * @{
 */
/* ===================================================================== */
/**
 * @brief  RCP Status enumeration
 */
typedef enum {
  RCP_STATUS_OK = 0,   /*!< Operation successful */
  RCP_STATUS_BUSY = 1, /*!< Operation not done: driver in use by another task */
  RCP_STATUS_ERROR = 2,               /*!< Operation failed */
  RCP_STATUS_TIMEOUT = 3,             /*!< Operation timeout */
  RCP_STATUS_IO_ERROR = 4,            /*!< IO error */
  RCP_STATUS_UNEXPECTED_RESPONSE = 5, /*!< Unexpected response */
} RCP_Status_t;

/**
 * @brief  RCP Module ID enumeration
 */
typedef enum {
  RCP_MODULE_ID_UNDEF = 0, /*!< Module ID undefined */
  RCP_MODULE_ID_B = 1,     /*!< Module ID -B Model */
  RCP_MODULE_ID_U = 2,     /*!< Module ID -U Model */
  RCP_MODULE_ID_P = 3,     /*!< Module ID -P Model */
} RCP_ModuleID_e;

#define RCP_SYS_FS_FILENAME_SIZE 32 /*!< Maximum size of the filename */

#define RCP_SYS_FS_MAX_FILES 20 /*!< Maximum number of files */

#define RCP_PART_NAME_SIZE 16 /*!< Maximum size of a partition name */

/** @} */

/* ===================================================================== */
/** @defgroup RCP_WiFi_Constants RCP Driver Wi-Fi Constants
 * @ingroup  RCP_WiFi
 * @{
 */
/* ===================================================================== */
/* RCP_event_id_t */
#define RCP_WIFI_EVT_SCAN_DONE_ID                                              \
  1 /*!< event_args parameter to be casted to RCP_WiFi_Scan_Result_t */
#define RCP_WIFI_EVT_CONNECTED_ID 2    /*!< no param */
#define RCP_WIFI_EVT_DISCONNECTED_ID 3 /*!< no param */
#define RCP_WIFI_EVT_GOT_IP_ID 4       /*!< no param */
#define RCP_WIFI_EVT_CONNECTING_ID 5   /*!< no param */
#define RCP_WIFI_EVT_REASON_ID 6       /*!< no param */

#define RCP_WIFI_EVT_STA_CONNECTED_ID                                          \
  10 /*!< event_args parameter to be casted to MAC ??? */
#define RCP_WIFI_EVT_STA_DISCONNECTED_ID                                       \
  11 /*!< event_args parameter to be casted to MAC ??? */
#define RCP_WIFI_EVT_DIST_STA_IP_ID                                            \
  12 /*!< event_args parameter to be casted to MAC+IP ??? */

#define RCP_WIFI_MAX_SSID_SIZE 32 /*!< Maximum size of the SSID */

#define RCP_WIFI_MAX_PASSWORD_SIZE 63 /*!< Maximum size of the password */

#define RCP_WIFI_MAX_HOSTNAME_SIZE 32 /*!< Maximum size of the hostname */

#define RCP_WIFI_MAX_CONNECTED_STATIONS                                        \
  4 /*!< Maximum number of connected stations */

#define RCP_WIFI_MAX_TWT_FLOWS 8 /*!< Maximum number of TWT flows */

#define RCP_CMD_MATCH_BUFF_SIZE                                                \
  256 /*!< Maximum size for modem cmd handler match_buf */

/**
 * @brief  Wi-Fi IPv4/IPv6 layer version
 */
typedef enum {
  RCP_WIFI_IPV4 = 0x00, /*!< IPv4 */
  RCP_WIFI_IPV6 = 0x01, /*!< IPv6 */
} RCP_WiFi_IPVer_e;

/**
 * @brief  Wi-Fi scan type
 */
typedef enum {
  RCP_WIFI_SCAN_STATE_ACTIVE = 0, /*!< Active scan */
  RCP_WIFI_SCAN_STATE_PASSIVE = 1 /*!< Passive scan */
} RCP_WiFi_scan_type_e;

/**
 * @brief  Wi-Fi security encryption method
 */
typedef enum {
  RCP_WIFI_SECURITY_OPEN = 0x00, /*!< Wi-Fi is open */
  RCP_WIFI_SECURITY_WEP =
      0x01, /*!< Wired Equivalent Privacy option for Wi-Fi security. */
  RCP_WIFI_SECURITY_WPA_PSK = 0x02,  /*!< Wi-Fi Protected Access */
  RCP_WIFI_SECURITY_WPA2_PSK = 0x03, /*!< Wi-Fi Protected Access 2 */
  RCP_WIFI_SECURITY_WPA_WPA2_PSK =
      0x04, /*!< Wi-Fi Protected Access with both modes */
  RCP_WIFI_SECURITY_WPA_ENT = 0x05,  /*!< Wi-Fi Protected Access */
  RCP_WIFI_SECURITY_WPA3_SAE = 0x06, /*!< Wi-Fi Protected Access 3 */
  RCP_WIFI_SECURITY_WPA2_WPA3_SAE =
      0x07, /*!< Wi-Fi Protected Access with both modes */
  RCP_WIFI_SECURITY_UNKNOWN = 0x08, /*!< Other modes */
} RCP_WiFi_SecurityType_e;

/**
 * @brief  Wi-Fi Soft-AP security encryption method
 */
typedef enum {
  RCP_WIFI_AP_SECURITY_OPEN = 0x00, /*!< Wi-Fi is open */
  RCP_WIFI_AP_SECURITY_WEP =
      0x01, /*!< Wired Equivalent Privacy option for Wi-Fi security. */
  RCP_WIFI_AP_SECURITY_WPA_PSK = 0x02,  /*!< Wi-Fi Protected Access */
  RCP_WIFI_AP_SECURITY_WPA2_PSK = 0x03, /*!< Wi-Fi Protected Access 2 */
  RCP_WIFI_AP_SECURITY_WPA3_PSK = 0x04, /*!< Wi-Fi Protected Access 3 */
} RCP_WiFi_ApSecurityType_e;

/**
 * @brief  Wi-Fi Connection status
 */
typedef enum {
  RCP_WIFI_STATE_STA_NO_STARTED_CONNECTION = 0, /*!< No connection started */
  RCP_WIFI_STATE_STA_CONNECTED = 1,             /*!< Connection established */
  RCP_WIFI_STATE_STA_GOT_IP = 2,                /*!< IP address received */
  RCP_WIFI_STATE_STA_CONNECTING = 3,            /*!< Connection in progress */
  RCP_WIFI_STATE_STA_DISCONNECTED = 4,          /*!< Connection lost */
  RCP_WIFI_STATE_STA_OFF = 5,                   /*!< Wi-Fi is off */
} RCP_WiFi_StaStateType_e;

/**
 * @brief  Wi-Fi Soft-AP status
 */
typedef enum {
  RCP_WIFI_STATE_AP_RESET = 0,   /*!< Soft-AP reset */
  RCP_WIFI_STATE_AP_RUNNING = 1, /*!< Soft-AP running */
  RCP_WIFI_STATE_AP_OFF = 2,     /*!< Soft-AP off */
} RCP_WiFi_ApStateType_e;

/**
 * @brief  Wi-Fi Connection pairwise cipher type enumeration
 */
typedef enum {
  RCP_WIFI_EVENT_BEACON_IND_CIPHER_NONE = 0,     /*!< None */
  RCP_WIFI_EVENT_BEACON_IND_CIPHER_WEP = 1,      /*!< WEP40 */
  RCP_WIFI_EVENT_BEACON_IND_CIPHER_AES = 4,      /*!< AES/CCMP */
  RCP_WIFI_EVENT_BEACON_IND_CIPHER_TKIP = 3,     /*!< TKIP */
  RCP_WIFI_EVENT_BEACON_IND_CIPHER_TKIP_AES = 5, /*!< TKIP and AES/CCMP */
  RCP_WIFI_EVENT_BEACON_IND_UNKNOW = 7,          /*!< Unknown */
} RCP_WiFi_CipherType_e;

/**
 * @brief  Wi-Fi protocol enumeration
 */
typedef enum {
  RCP_WIFI_PROTOCOL_UNKNOWN, /*!< Unknown */
  RCP_WIFI_PROTOCOL_11B,     /*!< 802.11b */
  RCP_WIFI_PROTOCOL_11G,     /*!< 802.11g */
  RCP_WIFI_PROTOCOL_11N,     /*!< 802.11n */
  RCP_WIFI_PROTOCOL_11AX     /*!< 802.11ax */
} RCP_WiFi_Protocol_e;

/**
 * @brief  Partition table entry returned by AT+PART?
 */
typedef struct {
  uint32_t active_idx;           /*!< Active slot index */
  uint32_t age;                  /*!< Partition age/counter */
  char name[RCP_PART_NAME_SIZE]; /*!< Partition name */
  uint32_t addr0;                /*!< Slot 0 start address */
  uint32_t addr1;                /*!< Slot 1 start address */
  uint32_t len0;                 /*!< Slot 0 length */
  uint32_t len1;                 /*!< Slot 1 length */
} RCP_PartInfo_t;

/**
 * @brief  Wi-Fi antenna diversity mode enumeration
 */
typedef enum {
  RCP_WIFI_ANTENNA_DISABLED, /*!< Antenna diversity disabled */
  RCP_WIFI_ANTENNA_STATIC,   /*!< Static antenna selection */
  RCP_WIFI_ANTENNA_DYNAMIC,  /*!< Dynamic antenna selection */
  RCP_WIFI_ANTENNA_UNKNOWN,  /*!< Unknown antenna selection */
} RCP_WiFi_AntennaMode_e;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_Net_Constants RCP Driver Net Constants
 * @ingroup  RCP_Net
 * @{
 */
/* ===================================================================== */
/* RCP_event_id_t */
#define RCP_NET_EVT_SOCK_DATA_ID                                               \
  50 /*!< event_args parameter to be casted to RCP_Net_CbParamData_t */
#define RCP_NET_EVT_SOCK_CONNECTED_ID                                          \
  51 /*!< event_args parameter to be casted to RCP_Net_CbParamData_t */
#define RCP_NET_EVT_SOCK_DISCONNECTED_ID                                       \
  52 /*!< event_args parameter to be casted to RCP_Net_CbParamData_t */

#define RCP_NET_MAX_CONNECTIONS                                                \
  5 /*!< Maximum number of Network connections                                 \
     */

/**
 * @brief  DNS resolved type
 */
typedef enum {
  RCP_NET_DNS_IPV4_IPV6 =
      0x01, /*!< Try to resolve IPv4 first, try IPv6 if IPv4 fails only */
  RCP_NET_DNS_IPV4 = 0x02, /*!< Resolve IPv4 only */
#if (RCP_NET_IPV6_ENABLE == 1)
  RCP_NET_DNS_IPV6 = 0x03, /*!< Resolve IPv6 only */
#endif                     /* RCP_NET_IPV6_ENABLE */
} RCP_Net_Dns_ResolveType_e;

/**
 * @brief  Net protocol enumeration
 */
typedef enum {
  RCP_NET_TCP_CONNECTION = 0,      /*!< TCP connection */
  RCP_NET_UDP_CONNECTION = 1,      /*!< UDP connection */
  RCP_NET_UDP_LITE_CONNECTION = 2, /*!< UDP Lite connection */
  RCP_NET_TCP_SSL_CONNECTION = 3,  /*!< TCP SSL connection */
#if (RCP_NET_IPV6_ENABLE == 1)
  RCP_NET_TCPV6_CONNECTION = 4,     /*!< IPv6 TCP connection */
  RCP_NET_UDPV6_CONNECTION = 5,     /*!< IPv6 UDP connection */
  RCP_NET_TCPV6_SSL_CONNECTION = 6, /*!< IPv6 TCP SSL connection */
#endif                              /* RCP_NET_IPV6_ENABLE */
  RCP_NET_UNKNOWN_CONNECTION = 7,   /*!< Unknown connection */
} RCP_Net_Protocol_e;

/**
 * @brief  DHCP Client type enumeration
 */
typedef enum {
  RCP_NET_DHCP_DISABLED = 0,    /*!< DHCP client disabled */
  RCP_NET_DHCP_STA_ENABLED = 1, /*!< DHCP client enabled for the station */
  RCP_NET_DHCP_AP_ENABLED = 2,  /*!< DHCP client enabled for the Soft-AP */
  RCP_NET_DHCP_STA_AP_ENABLED =
      3, /*!< DHCP client enabled for both station and Soft-AP */
} RCP_Net_DhcpType_e;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_MQTT_Constants RCP Driver MQTT Constants
 * @ingroup  RCP_MQTT
 * @{
 */
/* ===================================================================== */
/* RCP_event_id_t */
#define RCP_MQTT_EVT_CONNECTED_ID 60             /*!< no param */
#define RCP_MQTT_EVT_DISCONNECTED_ID 61          /*!< no param */
#define RCP_MQTT_EVT_SUBSCRIPTION_RECEIVED_ID 62 /*!< no param */

/** @} */

/* ===================================================================== */
/** @defgroup RCP_BLE_Constants RCP Driver BLE Constants
 * @ingroup  RCP_BLE
 * @{
 */
/* ===================================================================== */
/* RCP_event_id_t */
#define RCP_BLE_EVT_CONNECTED_ID                                               \
  21 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_DISCONNECTED_ID                                            \
  22 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_CONNECTION_PARAM_ID                                        \
  23 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_READ_ID                                                    \
  24 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_WRITE_ID                                                   \
  25 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_SERVICE_FOUND_ID                                           \
  26 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_CHAR_FOUND_ID                                              \
  27 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_INDICATION_STATUS_ENABLED_ID                               \
  28 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_INDICATION_STATUS_DISABLED_ID                              \
  29 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID                             \
  30 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_NOTIFICATION_STATUS_DISABLED_ID                            \
  31 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_NOTIFICATION_DATA_ID                                       \
  32 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_MTU_SIZE_ID                                                \
  33 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PAIRING_FAILED_ID                                          \
  34 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PAIRING_COMPLETED_ID                                       \
  35 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PAIRING_CONFIRM_ID                                         \
  36 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PAIRING_CANCELED_ID                                        \
  37 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PASSKEY_ENTRY_ID                                           \
  38 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PASSKEY_DISPLAY_ID                                         \
  39 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_PASSKEY_CONFIRM_ID                                         \
  40 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_SCAN_DONE_ID                                               \
  41 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_INDICATION_ACK_ID                                          \
  42 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */
#define RCP_BLE_EVT_INDICATION_NACK_ID                                         \
  43 /*!< event_args parameter to be casted to RCP_Ble_CbParamData_t */

#define RCP_BLE_BD_ADDR_SIZE 6 /*!< BLE BD Address size */
#define RCP_BLE_BD_ADDR_STRING_SIZE                                            \
  17 /*!< Length of a complete BD address string */
#define RCP_BLE_DEVICE_NAME_SIZE 26 /*!< BLE Device Name size */
#define RCP_BLE_MANUF_DATA_SIZE 30  /*!< BLE Maximum Manufacturer Data size */

/* AD types for advertising data and scan response data */
#define RCP_BLE_AD_TYPE_FLAGS 0x01U /*!< Advertising type Flags */
#define RCP_BLE_AD_TYPE_SHORTENED_LOCAL_NAME                                   \
  0x08U /*!< Advertising type Shortened Local Name */
#define RCP_BLE_AD_TYPE_COMPLETE_LOCAL_NAME                                    \
  0x09U /*!< Advertising type Complete Local Name */
#define RCP_BLE_AD_TYPE_MANUFACTURER_SPECIFIC_DATA                             \
  0xFFU /*!< Advertising type Manufacturer Specific Data */

/**
 * @brief  BLE mode type
 */
typedef enum {
  RCP_BLE_MODE_DEINIT = 0,
  RCP_BLE_MODE_CLIENT = 1,
  RCP_BLE_MODE_SERVER = 2,
  RCP_BLE_MODE_DUAL = 3
} RCP_Ble_Mode_e;

/**
 * @brief  BLE Advertising type enumeration
 */
typedef enum {
  RCP_BLE_ADV_TYPE_IND = 0,
  RCP_BLE_ADV_TYPE_SCAN_IND = 1,
  RCP_BLE_ADV_TYPE_NONCONN_IND = 2
} RCP_Ble_AdvType_e;

/**
 * @brief  BLE Advertising channel enumeration
 */
typedef enum {
  RCP_BLE_ADV_CHANNEL_37 = 1,
  RCP_BLE_ADV_CHANNEL_38 = 2,
  RCP_BLE_ADV_CHANNEL_39 = 4,
  RCP_BLE_ADV_CHANNEL_ALL = 7
} RCP_Ble_AdvChannel_e;

/**
 * @brief  BLE Scan type enumeration
 */
typedef enum {
  RCP_BLE_SCAN_PASSIVE = 0,
  RCP_BLE_SCAN_ACTIVE = 1
} RCP_Ble_ScanType_e;

/**
 * @brief  BLE address type
 */
typedef enum {
  RCP_BLE_PUBLIC_ADDR = 0,
  RCP_BLE_RANDOM_ADDR = 1,
  RCP_BLE_RPA_PUBLIC_ADDR = 2,
  RCP_BLE_RPA_RANDOM_ADDR = 3
} RCP_Ble_AddrType_e;

/**
 * @brief  BLE address type
 */
typedef enum {
  RCP_BLE_SCAN_FILTER_ALLOW_ALL = 0,
  RCP_BLE_SCAN_FILTER_ALLOW_ONLY_WLST = 1,
  RCP_BLE_SCAN_FILTER_ALLOW_UND_RPA_DIR = 2,
  RCP_BLE_SCAN_FILTER_ALLOW_WLIST_PRA_DIR = 3
} RCP_Ble_FilterPolicy_e;

/**
 * @brief  BLE UUID types
 */
typedef enum {
  RCP_BLE_UUID_TYPE_16 = 0,
  RCP_BLE_UUID_TYPE_128 = 2,
} RCP_Ble_UuidType_e;

/** @} */

/* Exported types ------------------------------------------------------------*/
/* ===================================================================== */
/** @defgroup RCP_System_Types RCP Driver System Types
 * @ingroup  RCP_System
 * @{
 */
/* ===================================================================== */

/**
 * @brief  IO Init Bus callback
 */
typedef int32_t (*RCP_IO_Init_cb_t)(uint8_t);

/**
 * @brief  IO DeInit Bus callback
 */
typedef int32_t (*RCP_IO_DeInit_cb_t)(void);

/**
 * @brief  IO Delay Bus callback
 */
typedef void (*RCP_IO_Delay_cb_t)(uint32_t);

/**
 * @brief  IO Send Bus callback
 */
typedef int32_t (*RCP_IO_Send_cb_t)(uint8_t, uint8_t *, uint16_t, uint32_t);

/**
 * @brief  IO Receive Bus callback
 */
typedef int32_t (*RCP_IO_Receive_cb_t)(uint8_t, uint8_t *, uint16_t, uint32_t);

/**
 * @brief  RCP events type
 */
typedef uint8_t RCP_event_id_t;

/**
 * @brief  Callback for Wi-Fi station events. the type of event_args depends of
 * the event_id:\
 *        - RCP_WiFi_Scan_Result_t for RCP_WIFI_EVT_SCAN_DONE_ID
 *        - NULL for [RCP_WIFI_EVT_CONNECTED_ID, RCP_WIFI_EVT_DISCONNECTED_ID, \
 *                    RCP_WIFI_EVT_GOT_IP_ID]
 */
typedef void (*RCP_UpperLayer_wifi_sta_cb_t)(RCP_event_id_t event_id,
                                             void *event_args);

/**
 * @brief  Callback for Wi-Fi access point events. the type of event_args
 * depends of the event_id:\
 *         - .. for [RCP_WIFI_EVT_STA_CONNECTED_ID,
 * RCP_WIFI_EVT_STA_DISCONNECTED_ID, \ RCP_WIFI_EVT_DIST_STA_IP_ID]
 */
typedef void (*RCP_UpperLayer_wifi_ap_cb_t)(RCP_event_id_t event_id,
                                            void *event_args);

/**
 * @brief  Callback for Net events. the type of event_args depends of the
 * event_id:
 *         - RCP_Net_CbParamData_t for all existing events
 */
typedef void (*RCP_UpperLayer_net_cb_t)(RCP_event_id_t event_id,
                                        void *event_args);

/**
 * @brief  Callback for MQTT events. the type of event_args depends of the
 * event_id: None
 */
typedef void (*RCP_UpperLayer_mqtt_cb_t)(RCP_event_id_t event_id,
                                         void *event_args);

/**
 * @brief  Callback for BLE events. the type of event_args depends of the
 * event_id:\
 *         - RCP_Ble_CbParamData_t for all existing events
 */
typedef void (*RCP_UpperLayer_ble_cb_t)(RCP_event_id_t event_id,
                                        void *event_args);

/**
 * @brief  Upper Layer Callbacks
 */
typedef struct {
  RCP_UpperLayer_wifi_sta_cb_t
      UL_wifi_sta_cb; /*!< Callback for Wi-Fi station events */
  RCP_UpperLayer_wifi_ap_cb_t
      UL_wifi_ap_cb; /*!< Callback for Wi-Fi access point events */
  RCP_UpperLayer_net_cb_t UL_net_cb;   /*!< Callback for Net events */
  RCP_UpperLayer_mqtt_cb_t UL_mqtt_cb; /*!< Callback for MQTT events */
  RCP_UpperLayer_ble_cb_t UL_ble_cb;   /*!< Callback for BLE events */
} RCP_UL_Cb_t;

/**
 * @brief  IO Bus functions
 */
typedef struct {
  RCP_IO_Init_cb_t IO_Init;       /*!< IO_Init Bus function */
  RCP_IO_DeInit_cb_t IO_DeInit;   /*!< IO_DeInit Bus function */
  RCP_IO_Delay_cb_t IO_Delay;     /*!< IO_Delay Bus function */
  RCP_IO_Send_cb_t IO_Send;       /*!< IO_Send Bus function */
  RCP_IO_Receive_cb_t IO_Receive; /*!< IO_Receive Bus function */
} RCP_IO_t;

/**
 * @brief  RCP module version structure
 */
typedef struct {
  uint8_t Major; /*!< Major version */
  uint8_t Sub1;  /*!< Minor sub1 version */
  uint8_t Sub2;  /*!< Minor sub2 version */
  uint8_t Patch; /*!< Patch version */
} RCP_Version_t;

/**
 * @brief  RCP module ID structure
 */
typedef struct {
  RCP_ModuleID_e ModuleID; /*!< Module ID */
  char ModuleName[25];     /*!< Module name */
} RCP_ModuleID_t;

/**
 * @brief  RCP module information
 */
typedef struct {
  RCP_Version_t FW_Version;       /*!< FW version string */
  RCP_Version_t SDK_Version;      /*!< SDK version string */
  RCP_Version_t WiFi_MAC_Version; /*!< Wi-Fi MAC version string */
  RCP_Version_t PHYRF_Version;    /*!< PHY/RF version string */
  RCP_Version_t
      BT_Controller_Version;      /*!< Bluetooth controller version string */
  RCP_Version_t BT_Stack_Version; /*!< Bluetooth stack version string */
  uint8_t Build_Date[32];         /*!< Build date string */
  uint32_t BatteryVoltage;        /*!< Battery voltage */
  int8_t trim_wifi_hp[14];        /*!< Wi-Fi high power trim */
  int8_t trim_wifi_lp[14];        /*!< Wi-Fi low power trim */
  int8_t trim_ble[5];             /*!< BLE trim */
  int8_t trim_xtal;               /*!< XTAL Frequency compensation */
  uint8_t Mac_Address[6];         /*!< Customer MAC address */
  uint8_t AntiRollbackBootloader; /*!< AntiRollback Bootloader counter */
  uint8_t AntiRollbackApp;        /*!< AntiRollback Application counter */
  RCP_ModuleID_t ModuleID;        /*!< Module name */
  uint16_t BomID;                 /*!< BOM ID */
  uint8_t Manufacturing_Week;     /*!< Manufacturing week */
  uint8_t Manufacturing_Year;     /*!< Manufacturing year */
} RCP_ModuleInfo_t;

/**
 * @brief  File System list structure
 */
typedef struct {
  char filename[RCP_SYS_FS_MAX_FILES]
               [RCP_SYS_FS_FILENAME_SIZE]; /*!< List of filenames */
  uint32_t nb_files;                       /*!< Number of files */
} RCP_FS_FilesList_t;

/**
 * @brief  Low Power configuration
 */
typedef struct {
  uint32_t WakeUpPinIn;        /*!< NCP wake up pin */
  uint32_t PSMode;             /*!< Power Save Mode of the NCP */
  uint32_t WiFi_DTIM_Factor;   /*!< DTIM Factor of the NCP */
  uint32_t WiFi_DTIM_Interval; /*!< DTIM Listen interval of the NCP */
} RCP_LowPowerCfg_t;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_WiFi_Types RCP Driver Wi-Fi Types
 * @ingroup  RCP_WiFi
 * @{
 */
/* ===================================================================== */

/**
 * @brief  Wi-Fi Callback parameter for network data
 */
typedef struct {
  uint8_t IP[4]; /*!< IP address of the station disconnected from the Soft-AP */
  uint8_t
      MAC[6]; /*!< MAC address of the station disconnected from the Soft-AP */
} RCP_WiFi_CbParamData_t;

/**
 * @brief  Wi-Fi Station settings
 */
typedef struct {
  uint32_t ReconnRetries;  /*!< Number of reconnection retries */
  uint32_t ReconnInterval; /*!< Interval between reconnection retries */
  uint32_t Channel;        /*!< Channel number */
  uint8_t AutoConnect; /*!< Flag to indicate if auto connection is enabled */
} RCP_WiFi_STASettings_t;

/**
 * @brief  Wi-Fi Soft-AP settings
 */
typedef struct {
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE +
               1];     /*!< Service Set Identifier value. Wi-Fi Soft-AP name */
  uint8_t MAC_Addr[6]; /*!< MAC address */
} RCP_WiFi_APSettings_t;

/**
 * @brief  Wi-Fi Beacon information structure from the scan result
 */
typedef struct {
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE +
               1]; /*!< Service Set Identifier value. Wi-Fi Soft-AP name */
  RCP_WiFi_SecurityType_e
      Security;    /*!< Security encryption of the Wi-Fi Soft-AP */
  int16_t RSSI;    /*!< Signal strength of Wi-Fi Soft-AP */
  uint8_t MAC[6];  /*!< MAC address of Wi-Fi Soft-AP */
  uint8_t Channel; /*!< Wi-Fi channel */
  RCP_WiFi_CipherType_e Pairwise_cipher; /*!< Pairwise cipher value */
  RCP_WiFi_Protocol_e Protocol;          /*!< Wi-Fi protocol supported */
  uint8_t WPS;                           /*!< Is WPS enabled */
} RCP_WiFi_AP_t;

/**
 * @brief  Wi-Fi Scan result
 */
typedef struct {
  uint32_t Count;    /*!< Number of detected APs */
  RCP_WiFi_AP_t *AP; /*!< List of detected APs */
  uint32_t More;     /*!< Flag to indicate if more results are available */
} RCP_WiFi_Scan_Result_t;

/**
 * @brief  Wi-Fi scan result callback
 */
typedef void (*RCP_WiFi_Scan_Result_cb_t)(int32_t status,
                                          RCP_WiFi_Scan_Result_t *Scan_results);

/**
 * @brief  Wi-Fi connection options
 */
typedef struct {
  /** Service Set Identifier value. Wi-Fi Access Point name */
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE + 1];
  /** Password of Wi-Fi Access Point */
  uint8_t Password[RCP_WIFI_MAX_PASSWORD_SIZE + 1];
  /** MAC address of Wi-Fi Access Point */
  uint8_t MAC[6];
  /** Interval between Wi-Fi connection retries. Unit: second. Default: 0.
   * Maximum: 7200 */
  uint16_t Reconnection_interval;
  /** Number of attempts the device makes to reconnect to the AP. Default 0
   * (always try), Max = 1000 */
  uint16_t Reconnection_nb_attempts;
  /** WPS mode. 0: WPS not used, 1: WPS PBC */
  uint32_t WPS;
  /** Connect to Access Point with WEP encryption. 0: WEP not used (default), 1:
   * WEP used */
  uint32_t WEP;
} RCP_WiFi_Connect_Opts_t;

/**
 * @brief  Wi-Fi internal context
 */
typedef struct {
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE +
               1]; /*!< Service Set Identifier value. Wi-Fi Access Point name */
  RCP_WiFi_SecurityType_e
      Security; /*!< Security encryption method of the Access Point */
  RCP_WiFi_STASettings_t STASettings;     /*!< Station settings */
  RCP_WiFi_APSettings_t APSettings;       /*!< Access Point settings */
  RCP_WiFi_Scan_Result_cb_t scan_done_cb; /*!< Callback for scan done */
  int32_t scan_status;                    /*!< Status of the scan */
  RCP_WiFi_Scan_Result_t ScanResults;     /*!< Scan results */
  RCP_WiFi_StaStateType_e StaState;       /*!< Station state */
  RCP_WiFi_ApStateType_e ApState;         /*!< Access Point state */
} RCP_Wifi_Ctx_t;

/**
 * @brief  Wi-Fi scan options
 */
typedef struct {
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE +
               1]; /*!< Service Set Identifier value. Wi-Fi Access Point name */
  uint8_t MAC[6];  /*!< MAC address of Wi-Fi Access Point */
  RCP_WiFi_scan_type_e scan_type; /*!< Scan type */
  uint8_t Channel;                /*!< Wi-Fi channel */
  uint8_t MaxCnt;                 /*!< Max number of APs to return */
} RCP_WiFi_Scan_Opts_t;

/**
 * @brief  Wi-Fi Soft-AP configuration
 */
typedef struct {
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE +
               1]; /*!< Service Set Identifier value. Wi-Fi Soft-AP name */
  uint8_t Password[RCP_WIFI_MAX_PASSWORD_SIZE +
                   1]; /*!< Password of Wi-Fi Soft-AP */
  RCP_WiFi_ApSecurityType_e
      Security;            /*!< Security encryption of the Wi-Fi Soft-AP */
  uint32_t Channel;        /*!< Channel Wi-Fi is operating at */
  uint32_t MaxConnections; /*!< Max number of stations that Soft-AP will allow
                              to connect. Range [1,4] */
  uint32_t Hidden; /*!< Flag to indicate if the SSID is hidden. 0: broadcasting
                      SSID, 1: hidden SSID */
  RCP_WiFi_Protocol_e Protocol; /*!< Wi-Fi protocol (b/g/n/ax) */
} RCP_WiFi_ApConfig_t;

/**
 * @brief  Wi-Fi connected station information
 */
typedef struct {
  uint8_t IP[4];  /*!< IP address of connected station */
  uint8_t MAC[6]; /*!< MAC address of connected station */
} RCP_WiFi_Connected_Sta_Info_t;

/**
 * @brief  Wi-Fi connected station structure
 */
typedef struct {
  uint32_t Count; /*!< Number of connected stations */
  RCP_WiFi_Connected_Sta_Info_t
      STA[RCP_WIFI_MAX_CONNECTED_STATIONS]; /*!< Array of connected stations */
} RCP_WiFi_Connected_Sta_t;

/**
 * @brief  Wi-Fi TWT setup parameters
 */
typedef struct {
  /** TWT Setup command (0: Request, 1: Suggest, 2: Demand) */
  uint8_t setup_type;
  /** Flow Type (0: Announced, 1: Unannounced) */
  uint8_t flow_type;
  /** Wake interval Exponent */
  uint32_t wake_int_exp;
  /** Nominal Minimum TWT Wake Duration */
  uint32_t min_twt_wake_dur;
  /** TWT Wake Interval Mantissa */
  uint32_t wake_int_mantissa;
} RCP_WiFi_TWT_Setup_Params_t;

/**
 * @brief  Wi-Fi TWT status
 */
typedef struct {
  /** Flag indicating whether TWT is supported (=1) or not (=0)*/
  uint8_t is_supported;
  struct {
    /** Flow Type (0: Announced, 1: Unannounced) */
    uint8_t flow_type;
    /** Wake interval Exponent */
    uint32_t wake_int_exp;
    /** Nominal Minimum TWT Wake Duration */
    uint32_t min_twt_wake_dur;
    /** TWT Wake Interval Mantissa */
    uint32_t wake_int_mantissa;
  } flow[RCP_WIFI_MAX_TWT_FLOWS]; /*!< Array of TWT flows */
  /** Number of TWT flows */
  uint8_t flow_count;
} RCP_WiFi_TWT_Status_t;

/**
 * @brief  Wi-Fi TWT teardown parameters
 */
typedef struct {
  /** Flag indicating whether to teardown all TWT flows */
  uint8_t all_twt;
  /** TWT flow ID */
  uint8_t id;
} RCP_WiFi_TWT_Teardown_Params_t;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_Net_Types RCP Driver Net Types
 * @ingroup  RCP_Net
 * @{
 */
/* ===================================================================== */

/**
 * @brief  Callback parameter for network data
 */
typedef struct {
  uint32_t socket_id;             /*!< Socket ID */
  uint32_t available_data_length; /*!< Available data length */
  char remote_ip[46];             /*!< Remote IP address */
  uint16_t remote_port;           /*!< Remote port */
} RCP_Net_CbParamData_t;

/**
 * @brief  Network connection configuration
 */
typedef struct {
  RCP_Net_Protocol_e Protocol; /*!< Protocol type (TCP/UDP) */
  uint8_t Number;              /*!< Connection number */
  uint16_t RemotePort;         /*!< Remote port */
  uint16_t LocalPort;          /*!< Local port */
  char RemoteIP[46];           /*!< Remote IP address */
  uint8_t TeType; /*!< Connection type (0: runs as client, 1: runs as server) */
  char *Name;     /*!< Connection name */
  uint16_t KeepAlive; /*!< Keep Alive */
} RCP_Net_Connection_t;

/**
 * @brief  Network ping result
 */
typedef struct {
  uint32_t average_time;   /*!< Average time taken for the ping */
  uint32_t total_time;     /*!< Total time taken for the ping */
  uint32_t min_time;       /*!< Minimum time taken for the ping */
  uint32_t max_time;       /*!< Maximum time taken for the ping */
  uint16_t response_count; /*!< Number of responses received */
  uint16_t lost_count;     /*!< Number of lost responses */
  rcp_sem_t sem_ping;      /*!< Semaphore for ping synchronization */
} RCP_Net_PingResult_t;

/**
 * @brief  Network time structure
 */
typedef struct {
  uint32_t seconds;     /*!< Seconds */
  uint32_t minutes;     /*!< Minutes */
  uint32_t hours;       /*!< Hours */
  uint32_t day;         /*!< Day */
  uint32_t month;       /*!< Month */
  uint32_t year;        /*!< Year */
  uint32_t day_of_week; /*!< Day of the week */
  char raw[26];         /*!< Raw time string */
} RCP_Net_Time_t;

/**
 * @brief IP address structure for RCP layer
 * */

/**
 * @brief  IPv4 local address structure aligned version of ip4_addr_t
 */
struct RCP_Net_ip4_addr {
  uint32_t addr; /*!< IPv4 address */
};

/**
 * @brief  IPv4 address structure
 */
typedef struct RCP_Net_ip4_addr RCP_Net_Ip4_Addr_t;

/**
 * @brief  IPv6 address structure
 */
typedef struct RCP_Net_ip6_addr {
  uint32_t addr[4]; /*!< IPv6 address */
} RCP_Net_Ip6_Addr_t;

/**
 * @brief  IP address structure
 */
typedef struct {
  union {
    RCP_Net_Ip6_Addr_t ip6; /*!< IPv6 address */
    RCP_Net_Ip4_Addr_t ip4; /*!< IPv4 address */
  } u_addr;                 /*!< IP address union */
  uint8_t type;             /*!< IP address type */
} RCP_Net_Ip_addr_t;
/**
 * @brief  Net internal context
 */
typedef struct {
  uint8_t Supported; /*!< Flag to indicate if the network is supported */
  struct {
    RCP_WiFi_IPVer_e IP_Ver;   /*!< IP layer version */
    uint8_t IP_Addr[4];        /*!< IP Address */
    uint8_t IP_Mask[4];        /*!< Netmask Address */
    uint8_t Gateway_Addr[4];   /*!< Gateway IP Address */
    uint8_t DNS1[4];           /*!< IP Address of First DNS */
    uint8_t DNS2[4];           /*!< IP Address of Second DNS */
    uint8_t DNS3[4];           /*!< IP Address of Third DNS */
    uint32_t IP6_LinkLocal[4]; /*!< IPv6 linklocal (binary, network order) */
    uint32_t IP6_Global[4]; /*!< IPv6 global/unicast (binary, network order) */
  } Net_sta_info;           /*!< Network Wi-Fi station information */
  struct {
    uint8_t IP_Addr[4];       /*!< Wi-Fi IP Address */
    uint8_t IP_Mask[4];       /*!< Wi-Fi Netmask Address */
  } Net_ap_info;              /*!< Network Wi-Fi Soft-AP information */
  uint8_t DHCP_STA_IsEnabled; /*!< Flag to indicate if DHCP client is enabled
                                 for the station */
  uint8_t DHCP_AP_IsEnabled; /*!< Flag to indicate if DHCP client is enabled for
                                the Soft-AP */
  int32_t AppBuffRecvDataSize; /*!< Size of the buffer to receive data */
  uint8_t *AppBuffRecvData;    /*!< Buffer to receive data */
} RCP_Net_Ctx_t;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_MQTT_Types RCP Driver MQTT Types
 * @ingroup  RCP_MQTT
 * @{
 */
/* ===================================================================== */

/**
 * @brief  Callback parameter for MQTT data
 */
typedef struct {
  uint32_t topic_length;   /*!< Length of the topic */
  uint32_t message_length; /*!< Length of the message */
} RCP_MQTT_CbParamData_t;

/**
 * @brief  MQTT internal context
 */
typedef struct {
  int32_t AppBuffRecvDataSize; /*!< Size of the buffer to receive data */
  uint8_t *AppBuffRecvData;    /*!< Buffer to receive data */
} RCP_MQTT_Ctx_t;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_BLE_Types RCP Driver BLE Types
 * @ingroup  RCP_BLE
 * @{
 */
/* ===================================================================== */

/**
 * @brief  BLE connection status structure
 */
typedef struct {
  struct {
    uint8_t conn_handle;                  /*!< Connection handle */
    uint8_t BDAddr[RCP_BLE_BD_ADDR_SIZE]; /*!< BD address of BLE Device */
  } Connection[RCP_BLE_MAX_CONN_NBR];     /*!< Array of BLE connections */
  uint8_t Count;                          /*!< Number of BLE connections */
} RCP_Ble_Connection_status_t;

/**
 * @brief  BLE characteristic structure
 */
typedef struct {
  uint8_t char_idx;                      /*!< Characteristic index */
  uint8_t uuid_type;                     /*!< UUID type (16-bit or 128-bit) */
  char char_uuid[RCP_BLE_MAX_UUID_SIZE]; /*!< Characteristic UUID */
  uint8_t char_property;                 /*!< Characteristic Property */
  uint8_t char_permission;               /*!< Characteristic Permission */
  uint8_t char_handle;                   /*!< Characteristic handle */
  uint8_t char_value_handle;             /*!< Characteristic value handle */
} RCP_Ble_Characteristic_t;

/**
 * @brief  BLE service structure
 */
typedef struct {
  uint8_t service_idx;  /*!< Service index */
  uint8_t service_type; /*!< Service type */
  uint8_t uuid_type;    /*!< UUID type (16-bit or 128-bit) */
  char service_uuid[RCP_BLE_MAX_UUID_SIZE]; /*!< Service UUID */
  RCP_Ble_Characteristic_t charac;          /*!< Service characteristic */
} RCP_Ble_Service_t;

/**
 * @brief  BLE remote device structure
 */
typedef struct {
  int16_t RSSI;        /*!< Signal strength of BLE connection */
  uint8_t IsConnected; /*!< Connection status */
  uint8_t conn_handle; /*!< Connection handle */
  uint8_t conn_role;   /*!< Connection role: 0: connected as a master, 1:
                          connected as a slave */
  uint8_t DeviceName[RCP_BLE_DEVICE_NAME_SIZE];      /*!< BLE device name */
  uint8_t ManufacturerData[RCP_BLE_MANUF_DATA_SIZE]; /*!< Device manufacturer
                                                        data */
  uint8_t BDAddr[RCP_BLE_BD_ADDR_SIZE]; /*!< BD address of BLE Device */
  uint8_t bd_addr_type;                 /*!< Type of BD address */
} RCP_Ble_Device_t;

/**
 * @brief  BLE bonded device structure
 */
typedef struct {
  uint8_t BDAddr[RCP_BLE_BD_ADDR_SIZE]; /*!< BD address of BLE Device */
  uint8_t bd_addr_type;                 /*!< Type of BD address */
  uint8_t LongTermKey[32];              /*!< Long Term Key */
} RCP_Ble_Bonded_Device_t;

/**
 * @brief  Callback parameter for BLE data
 */
typedef struct {
  uint8_t service_idx;                /*!< Service index */
  uint8_t charac_idx;                 /*!< Characteristic index */
  uint16_t charac_value_handle;       /*!< Characteristic value handle */
  uint8_t notification_status[2];     /*!< Notification status */
  uint8_t indication_status[2];       /*!< Indication status */
  uint16_t mtu_size;                  /*!< MTU Size */
  uint32_t PassKey;                   /*!< BLE Security passkey */
  uint8_t LongTermKey[32];            /*!< BLE Security Long Term Key */
  uint32_t available_data_length;     /*!< Available data length */
  RCP_Ble_Device_t remote_ble_device; /*!< BLE Remote device */
  RCP_Ble_Service_t Service;          /*!< BLE service */
} RCP_Ble_CbParamData_t;

/**
 * @brief  BLE network settings
 */
typedef struct {
  uint8_t DeviceConnectedNb; /*!< Connection status */
  RCP_Ble_Device_t
      RemoteDevice[RCP_BLE_MAX_CONN_NBR]; /*!< BLE remote device in network */
  uint8_t BLE_Mask[4];                    /*!< BLE mask */
  RCP_Ble_Mode_e Mode;                    /*!< BLE mode */
} RCP_Ble_Network_t;

/**
 * @brief  Scan results structure
 */
typedef struct {
  uint32_t Count; /*!< Number of BLE peripherals detected */
  RCP_Ble_Device_t
      *Detected_Peripheral; /*!< Array of BLE peripherals information. */
} RCP_Ble_Scan_Result_t;

/**
 * @brief  Scan results structure
 */
typedef struct {
  uint32_t Count; /*!< Number of BLE bonded device detected */
  RCP_Ble_Bonded_Device_t
      Bonded_device[RCP_BLE_MAX_BONDED_DEVICES]; /*!< Array of BLE bonded
                                                    devices information. */
} RCP_Ble_Bonded_Devices_Result_t;

/**
 * @brief  BLE scan result callback
 */
typedef void (*RCP_Ble_Scan_Result_cb_t)(
    RCP_Ble_Scan_Result_t *Ble_Scan_results);

/**
 * @brief  BLE internal context
 */
typedef struct {
  RCP_Ble_Network_t NetSettings;         /*!< Network settings */
  RCP_Ble_Scan_Result_cb_t scan_done_cb; /*!< Callback for scan done */
  RCP_Ble_Scan_Result_t ScanResults;     /*!< Scan results */
  int32_t AppBuffRecvDataSize; /*!< Size of the buffer to receive data */
  uint8_t *AppBuffRecvData;    /*!< Buffer to receive data */
  uint8_t ScanComplete;        /*!< Indicator to manage BLE Scan */
  uint32_t startScanTime;      /*!< Timer for BLE Scan */
} RCP_Ble_Ctx_t;

/**
 * @brief  BLE write data
 */
typedef struct {
  uint8_t conn_handle; /*!< Connection handle */
  uint8_t service_idx; /*!< Service index */
  uint8_t charac_idx;  /*!< Characteristic index */
  uint8_t data_len;    /*!< Data length */
  uint8_t data[16];    /*!< Data */
} RCP_BLEWriteData_t;

/**
 * @brief  BLE scan options structure
 */
typedef struct {
  RCP_Ble_ScanType_e scan_type;     /*!< Scan type (1: Active, 0: Passive) */
  RCP_Ble_AddrType_e own_addr_type; /*!< BLE Address type */
  RCP_Ble_FilterPolicy_e filter_policy; /*!< BLE Scan filter policy */
  /** BLE Scan interval. The scan interval equals this parameter multiplied by
   * 0.625 ms. Should be more than or equal to scan_window. */
  uint16_t scan_interval;
  /** BLE Scan window. The scan window equals this parameter multiplied by 0.625
   * ms. Should be less than or equal to scan_interval.*/
  uint16_t scan_window;
} RCP_Ble_Scan_Opts_t;

/**
 * @brief  BLE Connection options structure
 */
typedef struct {
  RCP_Ble_AddrType_e remote_addr_type;  /*!< BLE Address type */
  uint8_t BDAddr[RCP_BLE_BD_ADDR_SIZE]; /*!< BD address of BLE Device */
  uint32_t timeout;                     /*!< Connection timeout (ms) */
} RCP_Ble_Connect_Opts_t;

/** @} */

/* ===================================================================== */
/** @defgroup RCP_Netif_Types RCP Driver Network Interface Types
 * @ingroup  RCP_Netif
 * @{
 */
/* ===================================================================== */

/**
 * @brief  Network interface link up/down function type
 * @return Status of the operation (0 for success, negative for error)
 */
typedef int32_t (*RCP_Net_if_link_t)(void);

/**
 * @brief  Network interface callbacks structure
 */
typedef struct {
  RCP_Net_if_link_t link_sta_up_fn; /*!< Function to handle link up events */
  RCP_Net_if_link_t
      link_sta_down_fn;              /*!< Function to handle link down events */
  RCP_Net_if_link_t link_ap_up_fn;   /*!< Function to handle link up events */
  RCP_Net_if_link_t link_ap_down_fn; /*!< Function to handle link down events */
} RCP_Net_if_cb_t;

/** @} */

/* ===================================================================== */
/** @addtogroup RCP_System_Types
 * @{
 */
/* ===================================================================== */

/**
 * @brief  RCP Event callback function type
 * @param  hObj: pointer to module handle
 * @param  argc: pointer to argument count
 * @param  argv: pointer to argument values
 */
typedef void (*RCP_Event_cb_t)(void *hObj, uint16_t *argc, char **argv);

/**
 * @brief  RCP Event data callback function type
 * @param  event_id: event ID
 * @param  data: pointer to modem command handler data
 * @param  len: length of the data
 * @return process length on success, negative value on error
 */
typedef int32_t (*RCP_Event_data_cb_t)(uint32_t event_id,
                                       struct modem_cmd_handler_data *data,
                                       uint16_t len);

/**
 * @brief  RCP Modem context structure
 */
typedef struct modem {
  rcp_thread_t modem_task_handle;             /*!< Modem task handle */
  struct modem_cmd_handler modem_cmd_handler; /*!< Modem command handler */
  struct modem_cmd_handler_data
      modem_cmd_handler_data; /*!< Modem command handler data */
  struct modem_iface iface;   /*!< Modem interface */
  rcp_sem_t sem_tx_ready;     /*!< TX ready semaphore */
  rcp_sem_t sem_response;     /*!< Response semaphore */
  rcp_sem_t sem_if_ready;     /*!< Interface ready semaphore */
  void *ping_msg;             /*!< Pointer to received ping message */
  uint8_t cmd_match_buf[RCP_CMD_MATCH_BUFF_SIZE]; /*!< Command match buffer */
  uint16_t *argc;                                 /*!< Argument count pointer */
  char **argv;          /*!< Argument values pointer */
  uint16_t rx_data_len; /*!< Length of received data */
  void *rx_data;        /*!< Pointer to received data */
} RCP_Modem_t;

/**
 * @brief  RCP Object structure
 */
typedef struct {
  RCP_Event_cb_t Ble_event_cb;  /*!< BLE event callback function */
  RCP_Event_cb_t WiFi_event_cb; /*!< Wi-Fi event callback function */
  RCP_Event_cb_t Net_event_cb;  /*!< Net event callback function */
  RCP_Event_cb_t MQTT_event_cb; /*!< MQTT event callback function */
  RCP_Net_if_cb_t Netif_cb;     /*!< Netif callbacks handler */
  RCP_Event_data_cb_t
      MQTT_event_data_cb; /*!< MQTT event data callback function */
  RCP_Event_data_cb_t
      Ble_event_data_cb; /*!< BLE event data callback function */
  RCP_Event_cb_t
      Net_event_ping_cb; /*!< Net event ping data callback function */
  RCP_Event_cb_t Net_event_data_cb; /*!< Net event data callback function */
  RCP_Event_cb_t
      WiFi_event_scan_cb; /*!< Wi-Fi event scan data callback function */
} RCP_Callback_t;

/**
 * @brief  RCP Object structure
 */
typedef struct {
  RCP_ModuleInfo_t ModuleInfo;   /*!< Module information */
  RCP_UL_Cb_t ulcbs;             /*!< Upper Layer Callbacks */
  RCP_LowPowerCfg_t LowPowerCfg; /*!< Low Power configuration */
  RCP_Callback_t Callbacks;      /*!< Callbacks for different events */
  RCP_Ble_Ctx_t BleCtx;          /*!< BLE context */
  RCP_Wifi_Ctx_t WifiCtx;        /*!< Wi-Fi context */
  RCP_Net_Ctx_t NetCtx;          /*!< Net context */
  RCP_MQTT_Ctx_t MQTTCtx;        /*!< MQTT context */
  RCP_Modem_t Modem;             /*!< Modem context */
} RCP_Object_t;

/** @} */

/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/

/** Default timeout (ms) for module readiness */
#define RCP_READY_DEFAULT_TIMEOUT_MS (4000U)

/* ===================================================================== */
/** @defgroup RCP_System_Macros RCP Driver System Macros
 * @ingroup  RCP_System
 * @{
 */
/* ===================================================================== */

/**
 * \def SYS_LOG_DEBUG( ... )
 * Print system debug message
 */

/**
 * \def SYS_LOG_ERROR( ... )
 * Print system error message
 */

/**
 * \def SYS_LOG_ERROR_EX( line, file, ... )
 * Print system error message with file and line information
 */

/**
 * \def SYS_LOG_INFO( ... )
 * Print system info message
 */

/**
 * \def SYS_LOG_WARN( ... )
 * Print system warning message
 */

#if (SYS_LOG_ENABLE == 1)
#define SYS_LOG_DEBUG(...) LOG_INF(__VA_ARGS__)
#define SYS_LOG_ERROR(...) LOG_ERR(__VA_ARGS__)
#define SYS_LOG_ERROR_EX(line, file, ...) LogErrorEx(line, file, __VA_ARGS__)
#define SYS_LOG_INFO(...) LOG_INF(__VA_ARGS__)
#define SYS_LOG_WARN(...) LOG_WRN(__VA_ARGS__)
#else
#define SYS_LOG_DEBUG(...)
#define SYS_LOG_ERROR(...)
#define SYS_LOG_ERROR_EX(line, file, ...)
#define SYS_LOG_INFO(...)
#define SYS_LOG_WARN(...)
#endif /* SYS_LOG_ENABLE */

/** @} */

/* ===================================================================== */
/** @defgroup RCP_WiFi_Macros RCP Driver Wi-Fi Macros
 * @ingroup  RCP_WiFi
 * @{
 */
/* ===================================================================== */

/**
 * \def WIFI_LOG_DEBUG( ... )
 * Print Wi-Fi debug message
 */

/**
 * \def WIFI_LOG_ERROR( ... )
 * Print Wi-Fi error message
 */

/**
 * \def WIFI_LOG_INFO( ... )
 * Print Wi-Fi info message
 */

/**
 * \def WIFI_LOG_WARN( ... )
 * Print Wi-Fi warning message
 */

#if (WIFI_LOG_ENABLE == 1)
#define WIFI_LOG_DEBUG(...) LogDebug(__VA_ARGS__)
#define WIFI_LOG_ERROR(...) LogError(__VA_ARGS__)
#define WIFI_LOG_INFO(...) LogInfo(__VA_ARGS__)
#define WIFI_LOG_WARN(...) LogWarn(__VA_ARGS__)
#else
#define WIFI_LOG_DEBUG(...)
#define WIFI_LOG_ERROR(...)
#define WIFI_LOG_INFO(...)
#define WIFI_LOG_WARN(...)
#endif /* WIFI_LOG_ENABLE */

/** @} */

/* ===================================================================== */
/** @defgroup RCP_BLE_Macros RCP Driver BLE Macros
 * @ingroup  RCP_BLE
 * @{
 */
/* ===================================================================== */

/**
 * \def BLE_LOG_DEBUG( ... )
 * Print BLE debug message
 */

/**
 * \def BLE_LOG_ERROR( ... )
 * Print BLE error message
 */

/**
 * \def BLE_LOG_INFO( ... )
 * Print BLE info message
 */

/**
 * \def BLE_LOG_WARN( ... )
 * Print BLE warning message
 */

#if (BLE_LOG_ENABLE == 1)
#define BLE_LOG_DEBUG(...) LogDebug(__VA_ARGS__)
#define BLE_LOG_ERROR(...) LogError(__VA_ARGS__)
#define BLE_LOG_INFO(...) LogInfo(__VA_ARGS__)
#define BLE_LOG_WARN(...) LogWarn(__VA_ARGS__)
#else
#define BLE_LOG_DEBUG(...)
#define BLE_LOG_ERROR(...)
#define BLE_LOG_INFO(...)
#define BLE_LOG_WARN(...)
#endif /* BLE_LOG_ENABLE */

/** @} */

/* ===================================================================== */
/** @defgroup RCP_Net_Macros RCP Driver Net Macros
 * @ingroup  RCP_Net
 * @{
 */
/* ===================================================================== */

/**
 * \def NET_LOG_DEBUG( ... )
 * Print Network debug message
 */

/**
 * \def NET_LOG_ERROR( ... )
 * Print Network error message
 */

/**
 * \def NET_LOG_INFO( ... )
 * Print Network info message
 */

/**
 * \def NET_LOG_WARN( ... )
 * Print Network warning message
 */

#if (NET_LOG_ENABLE == 1)
#define NET_LOG_DEBUG(...) LogDebug(__VA_ARGS__)
#define NET_LOG_ERROR(...) LogError(__VA_ARGS__)
#define NET_LOG_INFO(...) LogInfo(__VA_ARGS__)
#define NET_LOG_WARN(...) LogWarn(__VA_ARGS__)
#else
#define NET_LOG_DEBUG(...)
#define NET_LOG_ERROR(...)
#define NET_LOG_INFO(...)
#define NET_LOG_WARN(...)
#endif /* NET_LOG_ENABLE */

/** @} */

/* ===================================================================== */
/** @defgroup RCP_MQTT_Macros RCP Driver MQTT Macros
 * @ingroup  RCP_MQTT
 * @{
 */
/* ===================================================================== */

/**
 * \def MQTT_LOG_DEBUG( ... )
 * Print MQTT debug message
 */

/**
 * \def MQTT_LOG_ERROR( ... )
 * Print MQTT error message
 */

/**
 * \def MQTT_LOG_INFO( ... )
 * Print MQTT info message
 */

/**
 * \def MQTT_LOG_WARN( ... )
 * Print MQTT warning message
 */

#if (MQTT_LOG_ENABLE == 1)
#define MQTT_LOG_DEBUG(...) LogDebug(__VA_ARGS__)
#define MQTT_LOG_ERROR(...) LogError(__VA_ARGS__)
#define MQTT_LOG_INFO(...) LogInfo(__VA_ARGS__)
#define MQTT_LOG_WARN(...) LogWarn(__VA_ARGS__)
#else
#define MQTT_LOG_DEBUG(...)
#define MQTT_LOG_ERROR(...)
#define MQTT_LOG_INFO(...)
#define MQTT_LOG_WARN(...)
#endif /* MQTT_LOG_ENABLE */

/** @} */

/* Exported functions ------------------------------------------------------- */
/* ===================================================================== */
/** @defgroup RCP_System_Functions RCP Driver System Functions
 * @ingroup  RCP_System
 * @{
 */
/* ===================================================================== */

/**
 * @brief  Get the RCP object
 * @return RCP_Object_t
 */
RCP_Object_t *RCP_ObjGet(void);

/**
 * @brief  Register the upper layer callbacks
 * @param  Obj: pointer to module handle
 * @param  UL_wifi_sta_cb: callback for Wi-Fi station events
 * @param  UL_wifi_ap_cb: callback for Wi-Fi access point events
 * @param  UL_net_cb: callback for Net events
 * @param  UL_mqtt_cb: callback for MQTT events
 * @param  UL_ble_cb: callback for BLE events
 * @return Operation status
 */
RCP_Status_t RCP_RegisterULcb(RCP_Object_t *Obj,
                              RCP_UpperLayer_wifi_sta_cb_t UL_wifi_sta_cb,
                              RCP_UpperLayer_wifi_ap_cb_t UL_wifi_ap_cb,
                              RCP_UpperLayer_net_cb_t UL_net_cb,
                              RCP_UpperLayer_mqtt_cb_t UL_mqtt_cb,
                              RCP_UpperLayer_ble_cb_t UL_ble_cb);

/**
 * @brief  Initialize WIFI module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Init(RCP_Object_t *Obj);

/**
 * @brief  Initialize WIFI module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_DeInit(RCP_Object_t *Obj);

/**
 * @brief  Wait for the WIFI module interface to be ready
 * @param  Obj: pointer to module handle
 * @param  time_ms: timeout in milliseconds
 * @return Operation status
 * @retval RCP_STATUS_OK Ready signal received
 * @retval RCP_STATUS_ERROR Timeout or invalid parameters
 */
RCP_Status_t RCP_WaitForReady(RCP_Object_t *Obj, uint32_t time_ms);

/**
 * @brief  Reset To factory defaults
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_ResetToFactoryDefault(RCP_Object_t *Obj);

/**
 * @brief  Send command to module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_SendAT(RCP_Object_t *Obj);

/**
 * @brief  Reset the module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Reset(RCP_Object_t *Obj);

/**
 * @brief  Get current remaining heap size in NCP
 * @param  Obj: pointer to module handle
 * @param  RemainingHeap: current remaining heap size
 * @param  LwipHeap: lwip heap size
 * @return Operation status
 */
RCP_Status_t RCP_GetNcpHeapState(RCP_Object_t *Obj, uint32_t *RemainingHeap,
                                 uint32_t *LwipHeap);

/**
 * @brief  Get the parameter store mode
 * @param  Obj: pointer to module handle
 * @param  mode: store mode
 * @return Operation status
 */
RCP_Status_t RCP_GetStoreMode(RCP_Object_t *Obj, uint32_t *mode);

/**
 * @brief  Set the parameter store mode
 * @param  Obj: pointer to module handle
 * @param  mode: store mode
 * @return Operation status
 */
RCP_Status_t RCP_SetStoreMode(RCP_Object_t *Obj, uint32_t mode);

/**
 * @brief  Read the EFUSE content
 * @param  Obj: pointer to module handle
 * @param  addr: address of EFUSE to read
 * @param  nbytes: number of EFUSE bytes to read
 * @param  data: pointer to output data
 * @return Operation Status
 */
RCP_Status_t RCP_ReadEFuse(RCP_Object_t *Obj, uint32_t addr, uint32_t nbytes,
                           uint8_t *data);

/**
 * @brief  Read raw eFuse data (AT+EFUSE-R=<nbytes>,"<addr>"[,<reload>])
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to read (max 8192)
 * @param  addr: eFuse address string e.g. "0x1c"
 * @param  reload: 0=do not force reload, 1=force reload from eFuse
 * @param  data: output buffer for raw data bytes
 * @return Operation status
 */
RCP_Status_t RCP_EFuseRead(RCP_Object_t *Obj, uint32_t nbytes, const char *addr,
                           uint32_t reload, uint8_t *data);

/**
 * @brief  Reset the module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_ResetModule(RCP_Object_t *Obj);

/**
 * @brief  Get GMR version information (AT, MAC SW, SDK, BT versions)
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_GetGMR(RCP_Object_t *Obj);

/**
 * @brief  Read supply voltage (AT+VBAT?)
 * @param  Obj: pointer to module handle
 * @param  Voltage: output buffer for voltage in mV
 * @return Operation status
 */
RCP_Status_t RCP_GetVBAT(RCP_Object_t *Obj, uint32_t *Voltage);

/**
 * @brief  Read part number (AT+PN?)
 * @param  Obj: pointer to module handle
 * @param  PartNumber: output buffer for part number string
 * @param  PartNumberSize: size of output buffer
 * @return Operation status
 */
RCP_Status_t RCP_GetPN(RCP_Object_t *Obj, char *PartNumber,
                       uint32_t PartNumberSize);

/**
 * @brief  Get GMAC slots as integers (AT+GMAC?)
 * @param  Obj: pointer to module handle
 * @param  GMACSLOT0: output buffer for SLOT0 MAC as uint64
 * @param  GMACSLOT1: output buffer for SLOT1 MAC as uint64
 * @param  GMACSLOT2: output buffer for SLOT2 MAC as uint64
 * @return Operation status
 */
RCP_Status_t RCP_GetGMAC(RCP_Object_t *Obj, uint8_t slot0_mac[6],
                         uint8_t slot1_mac[6], uint8_t slot2_mac[6]);

/**
 * @brief  Set Echo mode (ATE0=off, ATE1=on)
 * @param  Obj: pointer to module handle
 * @param  enable: 0 to disable echo, 1 to enable echo
 * @return Operation status
 */
RCP_Status_t RCP_SetEcho(RCP_Object_t *Obj, uint32_t enable);

/**
 * @brief  Query supported Commands list (AT+CMD?)
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_GetCmdList(RCP_Object_t *Obj);

/**
 * @brief  Query system message state (AT+SYSMSG?)
 * @param  Obj: pointer to module handle
 * @param  state: output buffer for current SYSMSG state
 * @return Operation status
 */
RCP_Status_t RCP_GetSysMsg(RCP_Object_t *Obj, uint32_t *state);

/**
 * @brief  Set system message state (AT+SYSMSG=<state>)
 * @param  Obj: pointer to module handle
 * @param  state: bit-field value for system message control
 * @return Operation status
 */
RCP_Status_t RCP_SetSysMsg(RCP_Object_t *Obj, uint32_t state);

/**
 * @brief  Query Error code output mode (AT+SYSLOG?)
 * @param  Obj: pointer to module handle
 * @param  state: output buffer for current SYSLOG state (0 or 1)
 * @return Operation status
 */
RCP_Status_t RCP_GetSysLog(RCP_Object_t *Obj, uint32_t *state);

/**
 * @brief  Set Error code output mode (AT+SYSLOG=<0|1>)
 * @param  Obj: pointer to module handle
 * @param  state: 0 disable error code output, 1 enable
 * @return Operation status
 */
RCP_Status_t RCP_SetSysLog(RCP_Object_t *Obj, uint32_t state);

/**
 * @brief  Query chip temperature (AT+TEMP?)
 * @param  Obj: pointer to module handle
 * @param  temp: output buffer for temperature (float)
 * @return Operation status
 */
RCP_Status_t RCP_GetTemp(RCP_Object_t *Obj, float *temp);

/**
 * @brief  Configure a GPIO pin as floating input state
 * @param  Obj: pointer to module handle
 * @param  pin: GPIO pin number (0-31)
 * @return Operation status
 */
RCP_Status_t RCP_RestoreGPIO(RCP_Object_t *Obj, uint32_t pin);

/**
 * @brief  Confirm eFuse write (AT+EFUSE-CFM)
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_EFuseConfirm(RCP_Object_t *Obj);

/**
 * @brief  Write raw data to eFuse buffer (AT+EFUSE-W=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to write (max 8192)
 * @param  addr: eFuse address string e.g. "0x1c"
 * @param  data: pointer to raw data bytes
 * @return Operation status
 */
RCP_Status_t RCP_EFuseWrite(RCP_Object_t *Obj, uint32_t nbytes,
                            const char *addr, uint8_t *data);

/**
 * @brief  Write hex string to eFuse buffer (AT+EFUSE-W-HEX=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to write (max 8192)
 * @param  addr: eFuse address string e.g. "0x2c"
 * @param  hex_data: pointer to hex string (length must be 2*nbytes)
 * @return Operation status
 */
RCP_Status_t RCP_EFuseWriteHex(RCP_Object_t *Obj, uint32_t nbytes,
                               const char *addr, const char *hex_data);

/**
 * @brief  Write raw data to Flash (AT+FLASH-W=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to write (max 8192)
 * @param  addr: flash address string e.g. "0x210000"
 * @param  data: pointer to raw data bytes
 * @return Operation status
 */
RCP_Status_t RCP_FlashWrite(RCP_Object_t *Obj, uint32_t nbytes,
                            const char *addr, uint8_t *data);

/**
 * @brief  Write hex string to Flash (AT+FLASH-W-HEX=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to write (max 8192)
 * @param  addr: flash address string e.g. "0x210000"
 * @param  hex_data: pointer to hex string (length must be 2*nbytes)
 * @return Operation status
 */
RCP_Status_t RCP_FlashWriteHex(RCP_Object_t *Obj, uint32_t nbytes,
                               const char *addr, const char *hex_data);

/**
 * @brief  Read Flash raw data (AT+FLASH-R=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to read (max 8192)
 * @param  addr: flash address string e.g. "0x210000"
 * @param  data: output buffer for raw data bytes
 * @return Operation status
 */
RCP_Status_t RCP_FlashRead(RCP_Object_t *Obj, uint32_t nbytes, const char *addr,
                           uint8_t *data);

/**
 * @brief  Read Flash as hex string (AT+FLASH-R-HEX=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to read (max 8192)
 * @param  addr: flash address string e.g. "0x210000"
 * @param  hex_data: output buffer for hex string (caller must provide >=
 * 2*nbytes+1)
 * @return Operation status
 */
RCP_Status_t RCP_FlashReadHex(RCP_Object_t *Obj, uint32_t nbytes,
                              const char *addr, char *hex_data);

/**
 * @brief  Read eFuse raw data (AT+EFUSE-R=<nbytes>,"<addr>"[,<reload>])
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to read (max 8192)
 * @param  addr: eFuse address string e.g. "0x1c"
 * @param  reload: 0=do not force reload, 1=force reload from eFuse
 * @param  data: output buffer for raw data (caller must provide >= 2*nbytes+1)
 * @return Operation status
 */
RCP_Status_t RCP_EFuseRead(RCP_Object_t *Obj, uint32_t nbytes, const char *addr,
                           uint32_t reload, uint8_t *data);
/**
 * @brief  Read eFuse as hex string
 * (AT+EFUSE-R-HEX=<nbytes>,"<addr>"[,<reload>])
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to read (max 8192)
 * @param  addr: eFuse address string e.g. "0x1c"
 * @param  reload: 0=do not force reload, 1=force reload from eFuse
 * @param  hex_data: output buffer for hex string (caller must provide >=
 * 2*nbytes+1)
 * @return Operation status
 */
RCP_Status_t RCP_EFuseReadHex(RCP_Object_t *Obj, uint32_t nbytes,
                              const char *addr, uint32_t reload,
                              char *hex_data);

/**
 * @brief  Query partition table (AT+PART?)
 * @param  Obj: pointer to module handle
 * @param  parts: optional output array for parsed entries
 * @param  nb_parts: in/out entry count, input is array capacity, output is
 * parsed count
 * @return Operation status
 */
RCP_Status_t RCP_PartQuery(RCP_Object_t *Obj, RCP_PartInfo_t *parts,
                           uint32_t *nb_parts);

/**
 * @brief  Export minidump partition as binary data (AT+MINIDUMP)
 * @param  Obj: pointer to module handle
 * @param  dump_data: optional output buffer for binary dump data
 * @param  dump_len: in/out buffer size in bytes; input capacity, output stored
 * length
 * @return Operation status
 */
RCP_Status_t RCP_Minidump(RCP_Object_t *Obj, uint8_t *dump_data,
                          uint32_t *dump_len);

/**
 * @brief  Erase Flash area (AT+FLASH-E=<nbytes>,"<addr>")
 * @param  Obj: pointer to module handle
 * @param  nbytes: number of bytes to erase (max 8192)
 * @param  addr: flash address string e.g. "0x210000"
 * @return Operation status
 */
RCP_Status_t RCP_FlashErase(RCP_Object_t *Obj, uint32_t nbytes,
                            const char *addr);

/**
 * @brief  Configure GPIO as output with pull mode (AT+IOPUPD=<pin>,<state>)
 * @param  Obj: pointer to module handle
 * @param  pin: GPIO pin number (0-31)
 * @param  state: 0=floating, 1=pull-up, 2=pull-down
 * @return Operation status
 */
RCP_Status_t RCP_GPIOPullUpDn(RCP_Object_t *Obj, uint32_t pin, uint32_t state);

/**
 * @brief  Set GPIO output level (AT+IOOUT=<pin>,<state>)
 * @param  Obj: pointer to module handle
 * @param  pin: GPIO pin number (0-31)
 * @param  state: 0=low, 1=high
 * @return Operation status
 */
RCP_Status_t RCP_GPIOOutput(RCP_Object_t *Obj, uint32_t pin, uint32_t state);

/**
 * @brief  Configure GPIO as input mode (AT+IOIN=<pin>,<pull_state>)
 * @param  Obj: pointer to module handle
 * @param  pin: GPIO pin number (0-31)
 * @param  pull_state: 0=floating, 1=pull-up, 2=pull-down
 * @return Operation status
 */
RCP_Status_t RCP_GPIOInputConfig(RCP_Object_t *Obj, uint32_t pin,
                                 uint32_t pull_state);

/**
 * @brief  Query GPIO input value (AT+IOIN=<pin>?)
 * @param  Obj: pointer to module handle
 * @param  pin: GPIO pin number (0-31)
 * @param  state: output buffer for current input value (0 or 1)
 * @return Operation status
 */
RCP_Status_t RCP_GPIOInputRead(RCP_Object_t *Obj, uint32_t pin,
                               uint32_t *state);

/**
 * @brief  Delete the file entry in the NCP file system
 * @param  Obj: pointer to module handle
 * @param  filename: pointer to the file name
 * @return Operation status
 */
RCP_Status_t RCP_FS_DeleteFile(RCP_Object_t *Obj, char *filename);

/**
 * @brief  Create a file entry in the NCP file system
 * @param  Obj: pointer to module handle
 * @param  filename: pointer to the file name
 * @return Operation status
 */
RCP_Status_t RCP_FS_CreateFile(RCP_Object_t *Obj, char *filename);

/**
 * @brief  Write data to a file in the NCP file system
 * @param  Obj: pointer to module handle
 * @param  filename: pointer to the file name
 * @param  offset: offset in the file
 * @param  data: pointer to the data to write
 * @param  len: length of the data to write
 * @return Operation status
 */
RCP_Status_t RCP_FS_WriteFile(RCP_Object_t *Obj, char *filename,
                              uint32_t offset, uint8_t *data, uint32_t len);

/**
 * @brief  Read data from a file in the NCP file system
 * @param  Obj: pointer to module handle
 * @param  filename: pointer to the file name
 * @param  offset: offset in the file
 * @param  data: pointer to the data to read
 * @param  len: length of the data to read
 * @return Operation status
 */
RCP_Status_t RCP_FS_ReadFile(RCP_Object_t *Obj, char *filename, uint32_t offset,
                             uint8_t *data, uint32_t len);

/**
 * @brief  Get the size of a file in the NCP file system
 * @param  Obj: pointer to module handle
 * @param  filename: pointer to the file name
 * @param  size: pointer to the file size
 * @return Operation status
 */
RCP_Status_t RCP_FS_GetSizeFile(RCP_Object_t *Obj, char *filename,
                                uint32_t *size);

/**
 * @brief  List all files in the NCP file system
 * @param  Obj: pointer to module handle
 * @param  files_list: pointer to the file list structure
 * @return Operation status
 */
RCP_Status_t RCP_FS_ListFiles(RCP_Object_t *Obj,
                              RCP_FS_FilesList_t *files_list);

/**
 * @brief  Send command to start the FWU process
 * @param  Obj: pointer to module handle
 * @param  enable: 0 Terminate the FWU transmission. 1 Start the FWU
 * transmission
 * @return Operation status
 */
RCP_Status_t RCP_FWU_starts(RCP_Object_t *Obj, uint32_t enable);

/**
 * @brief  Send command to stop FWU process and reboot the module using the new
 * firmware
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_FWU_Finish(RCP_Object_t *Obj);

/**
 * @brief  Send command to send FWU data buf of size len
 * @param  Obj: pointer to module handle
 * @param  buff: pointer to the FWU data to send
 * @param  len: length of the FWU data to send
 * @return Operation status
 */
RCP_Status_t RCP_FWU_Send(RCP_Object_t *Obj, uint8_t *buff, uint32_t len);

/**
 * @brief  Configure low power settings in the host context
 * @param  Obj: pointer to module handle
 * @param  WakeUpPinIn: NCP wake up pin
 * @param  ps_mode: power save mode (0: normal, 1: hibernate, 2: standby)
 * @return Operation status
 */
RCP_Status_t RCP_LowPowerConfig(RCP_Object_t *Obj, uint32_t WakeUpPinIn,
                                uint32_t ps_mode);

/**
 * @brief  Configure power mode
 * @param  Obj: pointer to module handle
 * @param  ps_mode: power save mode (0: normal, 1: hibernate, 2: standby)
 * @param  hbn_level: hibernate wakeup source selection
 * @return Operation status
 */
RCP_Status_t RCP_SetPowerMode(RCP_Object_t *Obj, uint32_t ps_mode,
                              uint32_t hbn_level);

/**
 * @brief  Get cached power mode
 * @param  Obj: pointer to module handle
 * @param  ps_mode: output buffer for cached power mode
 * @return Operation status
 */
RCP_Status_t RCP_GetPowerMode(RCP_Object_t *Obj, uint32_t *ps_mode);

/**
 * @brief  Set NCP wake-up pin
 * @param  Obj: pointer to module handle
 * @param  wakeup_pin: pin number of the wake up input
 * @return Operation status
 */
RCP_Status_t RCP_SetWakeUpPin(RCP_Object_t *Obj, uint32_t wakeup_pin);

/**
 * @brief  Execute command
 * @param  Obj: pointer to module handle
 * @param  at_cmd: command string
 * @return Operation status
 */
RCP_Status_t RCP_ExeCommand(RCP_Object_t *Obj, char *at_cmd);

/** @} */

/* ===================================================================== */
/** @defgroup RCP_WiFi_Functions RCP Driver Wi-Fi Functions
 * @ingroup  RCP_WiFi
 * @{
 */
/* ===================================================================== */
/**
 * @brief  Initialize WIFI module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_Init(RCP_Object_t *Obj);

/**
 * @brief  Deinitialize WIFI module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_DeInit(RCP_Object_t *Obj);

/**
 * @brief  Activate STA mode
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_Station_Start(RCP_Object_t *Obj);

/**
 * @brief  List all detected APs
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_Scan(RCP_Object_t *Obj);

/**
 * @brief  Set scan options APs
 * @param  Obj: pointer to module handle
 * @param  ScanOpts: Pointer to scan options structure
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_SetScanOpts(RCP_Object_t *Obj,
                                  RCP_WiFi_Scan_Opts_t *ScanOpts);

/**
 * @brief  Set the reconnection options
 * @param  Obj: pointer to module handle
 * @param  ConnectOpts: Pointer to connection options structure
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_SetReconnectionOpts(RCP_Object_t *Obj,
                                          RCP_WiFi_Connect_Opts_t *ConnectOpts);

/**
 * @brief  Join an Access Point
 * @param  Obj: pointer to module handle
 * @param  ConnectOpts: Pointer to connection options structure
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_Connect(RCP_Object_t *Obj,
                              RCP_WiFi_Connect_Opts_t *ConnectOpts);

/**
 * @brief  Get the information of the Access Point where the station is
 * connected
 * @param  Obj: pointer to module handle
 * @param  Rssi: Pointer to RSSI value
 * @param  Security: Pointer to security type
 * @param  Protocol: Pointer to protocol type
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_GetConnectInfo(RCP_Object_t *Obj, int32_t *Rssi,
                                     RCP_WiFi_SecurityType_e *Security,
                                     RCP_WiFi_Protocol_e *Protocol);

/**
 * @brief  Disconnect from a network
 * @param  Obj: pointer to module handle
 * @param  restore: Remove the saved connection information
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_Disconnect(RCP_Object_t *Obj, uint32_t restore);

/**
 * @brief  Chooses if auto connect should be On or Off
 * @param  Obj: pointer to module handle
 * @param  OnOff: If set to 1 WIFI module will connect to last SSID used to
 * connect
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_SetAutoConnect(RCP_Object_t *Obj, uint32_t OnOff);

/**
 * @brief  Return the auto connect status : On or Off
 * @param  Obj: pointer to module handle
 * @param  OnOff: If set to 1 WIFI module will connect to last SSID used to
 * connect
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_GetAutoConnect(RCP_Object_t *Obj, uint32_t *OnOff);

/**
 * @brief  Get the MAC address of the station
 * @param  Obj: pointer to module handle
 * @param  Mac: pointer to the MAC address array
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_Station_GetMACAddress(RCP_Object_t *Obj, uint8_t *Mac);

/**
 * @brief  Get the country code information
 * @param  Obj: pointer to module handle
 * @param  Policy: value to specify if the country code align on AP's one
 * @param  CountryString: pointer to Country code string
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_GetCountryCode(RCP_Object_t *Obj, uint32_t *Policy,
                                     char *CountryString);

/**
 * @brief  Set the country code information
 * @param  Obj: pointer to module handle
 * @param  Policy: value to specify if the country code align on AP's one
 * @param  CountryString: pointer to Country code string
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_SetCountryCode(RCP_Object_t *Obj, uint32_t *Policy,
                                     char *CountryString);

/**
 * @brief  Set the Wi-Fi mode to Station + Soft-AP
 * @param  Obj: pointer to module handle
 * @return RCP_Status_t
 */
RCP_Status_t RCP_WiFi_SetDualMode(RCP_Object_t *Obj);

/**
 * @brief  Configure and activate Soft-AP
 * @param  Obj: pointer to module handle
 * @param  ApConfig: Pointer to Soft-AP config structure
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_AP_Start(RCP_Object_t *Obj,
                               RCP_WiFi_ApConfig_t *ApConfig);

/**
 * @brief  Deactivate the Soft-AP
 * @param  Obj: pointer to module handle
 * @param  Reconnect: 0: Do not reconnect, 1: check if station were previously
 * connected and reconnect
 * @return RCP_Status_t
 */
RCP_Status_t RCP_WiFi_AP_Stop(RCP_Object_t *Obj, uint8_t Reconnect);

/**
 * @brief  Get the Soft-AP configuration
 * @param  Obj: pointer to module handle
 * @param  ApConfig: pointer to the Soft-AP configuration structure
 * @return RCP_Status_t
 */
RCP_Status_t RCP_WiFi_AP_GetConfig(RCP_Object_t *Obj,
                                   RCP_WiFi_ApConfig_t *ApConfig);

/**
 * @brief  List all connected stations
 * @param  Obj: pointer to module handle
 * @param  Sta: pointer to the connected stations structure
 * @return RCP_Status_t
 */
RCP_Status_t RCP_WiFi_AP_ListConnectedStations(RCP_Object_t *Obj,
                                               RCP_WiFi_Connected_Sta_t *Sta);

/**
 * @brief  Disconnect a station from the Soft-AP
 * @param  Obj: pointer to module handle
 * @param  MAC: pointer to the MAC address of the station to disconnect
 * @return RCP_Status_t
 */
RCP_Status_t RCP_WiFi_AP_DisconnectStation(RCP_Object_t *Obj, uint8_t *MAC);

/**
 * @brief  Get the Wi-Fi mode of the Soft-AP
 * @param  Obj: pointer to module handle
 * @param  Protocol: pointer to the Wi-Fi protocol
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_AP_GetMode(RCP_Object_t *Obj,
                                 RCP_WiFi_Protocol_e *Protocol);

/**
 * @brief  Set the Wi-Fi mode of the Soft-AP
 * @param  Obj: pointer to module handle
 * @param  Protocol: Wi-Fi protocol
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_AP_SetMode(RCP_Object_t *Obj,
                                 RCP_WiFi_Protocol_e Protocol);

/**
 * @brief  Get the MAC address of the Soft-AP
 * @param  Obj: pointer to module handle
 * @param  Mac: pointer to the MAC address array
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_AP_GetMACAddress(RCP_Object_t *Obj, uint8_t *Mac);

/**
 * @brief  Set DTIM period
 * @param  Obj: pointer to module handle
 * @param  dtim: the dtim period
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_SetDTIM(RCP_Object_t *Obj, uint32_t dtim);

/**
 * @brief  Get the DTIM period of the Access Point
 * @param  Obj: pointer to module handle
 * @param  dtim: pointer to the DTIM period
 * @return Operation Status.
 */
RCP_Status_t RCP_WiFi_GetDTIM_AP(RCP_Object_t *Obj, uint32_t *dtim);

/**
 * @brief  Setup the Wi-Fi TWT power save mode
 * @param  Obj: pointer to module handle
 * @param  twt_params: pointer to TWT parameters structure
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_TWT_Setup(RCP_Object_t *Obj,
                                RCP_WiFi_TWT_Setup_Params_t *twt_params);

/**
 * @brief  Get the Wi-Fi TWT status
 * @param  Obj: pointer to module handle
 * @param  twt_status: pointer to TWT flow status
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_TWT_GetStatus(RCP_Object_t *Obj,
                                    RCP_WiFi_TWT_Status_t *twt_status);

/**
 * @brief  Get the Wi-Fi TWT power support status
 * @param  Obj: pointer to module handle
 * @param  is_supported: pointer to TWT support status
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_TWT_IsSupported(RCP_Object_t *Obj,
                                      uint32_t *is_supported);

/**
 * @brief  Teardown the Wi-Fi TWT power save mode
 * @param  Obj: pointer to module handle
 * @param  twt_params: pointer to TWT parameters structure
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_TWT_Teardown(RCP_Object_t *Obj,
                                   RCP_WiFi_TWT_Teardown_Params_t *twt_params);

/**
 * @brief  Set the antenna configuration
 * @param  Obj: pointer to module handle
 * @param  mode: Antenna mode (disabled, static, dynamic)
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_SetAntennaEnable(RCP_Object_t *Obj,
                                       RCP_WiFi_AntennaMode_e mode);

/**
 * @brief  Get the antenna configuration
 * @param  Obj: pointer to module handle
 * @param  mode: Pointer to the antenna mode (disabled, static, dynamic)
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_GetAntennaEnable(RCP_Object_t *Obj,
                                       RCP_WiFi_AntennaMode_e *mode);

/**
 * @brief  Get the antenna used in static mode
 * @param  Obj: pointer to module handle
 * @param  antenna_id: Pointer to antenna ID used (0: antenna 1, 1: antenna 2)
 * @return Operation status
 */
RCP_Status_t RCP_WiFi_GetAntennaUsed(RCP_Object_t *Obj, uint32_t *antenna_id);

/** @} */

/* ===================================================================== */
/** @defgroup RCP_Net_Functions RCP Driver Net Functions
 * @ingroup  RCP_Net
 * @{
 */
/* ===================================================================== */
/**
 * @brief  Initialize the Net default configuration
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Net_Init(RCP_Object_t *Obj);

/**
 * @brief  Deinitialize the Net module
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Net_DeInit(RCP_Object_t *Obj);

/**
 * @brief  Set the host name of the station
 * @param  Obj: pointer to module handle
 * @param  Hostname: string containing the host name
 * @return Operation status
 */
RCP_Status_t RCP_Net_SetHostname(RCP_Object_t *Obj, uint8_t Hostname[33]);

/**
 * @brief  Return the host name
 * @param  Obj: pointer to module handle
 * @param  Hostname: string containing the host name
 * @return Operation status
 */
RCP_Status_t RCP_Net_GetHostname(RCP_Object_t *Obj, uint8_t Hostname[33]);

/**
 * @brief  Set the IP address of the module
 * @param  Obj: pointer to module handle
 * @param  Ip_addr: pointer to the IP address array
 * @param  Gateway_addr: pointer to the IP address array
 * @param  Netmask_addr: pointer to the IP address array
 * @return Operation status
 */
RCP_Status_t RCP_Net_Station_SetIPAddress(RCP_Object_t *Obj, uint8_t Ip_addr[4],
                                          uint8_t Gateway_addr[4],
                                          uint8_t Netmask_addr[4]);

/**
 * @brief  Get the IP address of the station
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Net_Station_GetIPAddress(RCP_Object_t *Obj);

/**
 * @brief  Set the IP address of the Soft-AP
 * @param  Obj: pointer to module handle
 * @param  Ip_addr: pointer to the IP address array
 * @param  Netmask_addr: pointer to the Netmask address array
 * @return RCP_Status_t
 */
RCP_Status_t RCP_Net_AP_SetIPAddress(RCP_Object_t *Obj, uint8_t Ip_addr[4],
                                     uint8_t Netmask_addr[4]);

/**
 * @brief  Get the IP address of the Soft-AP
 * @param  Obj: pointer to module handle
 * @return RCP_Status_t
 */
RCP_Status_t RCP_Net_AP_GetIPAddress(RCP_Object_t *Obj);

/**
 * @brief  Get the DHCP config
 * @param  Obj: pointer to module handle
 * @param  State: pointer to the Dhcp client enum
 * @return Operation status
 */
RCP_Status_t RCP_Net_GetDhcpConfig(RCP_Object_t *Obj,
                                   RCP_Net_DhcpType_e *State);

/**
 * @brief  Set the DHCP global configuration
 * @param  Obj: pointer to module handle
 * @param  State: pointer to the dhcp client enum
 * @param  Operate: pointer defining to enable / disable dhcp client
 * @return Operation status
 */
RCP_Status_t RCP_Net_SetDhcpConfig(RCP_Object_t *Obj, RCP_Net_DhcpType_e *State,
                                   uint32_t *Operate);

/**
 * @brief  Get the DHCP server configuration
 * @param  Obj: pointer to module handle
 * @param  lease_time: lease time
 * @param  start_ip: pointer to the start IP address
 * @param  end_ip: pointer to the end IP address
 * @return RCP_Status_t
 */
RCP_Status_t RCP_Net_GetDhcpsConfig(RCP_Object_t *Obj, uint32_t *lease_time,
                                    uint8_t start_ip[4], uint8_t end_ip[4]);

/**
 * @brief  Set the DHCP server configuration
 * @param  Obj: pointer to module handle
 * @param  lease_time: lease time
 * @return RCP_Status_t
 */
RCP_Status_t RCP_Net_SetDhcpsConfig(RCP_Object_t *Obj, uint32_t lease_time);

/**
 * @brief  Get the DNS information
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Net_GetDnsAddress(RCP_Object_t *Obj);

/**
 * @brief  Set the DNS IP address of the module
 * @param  Obj: pointer to module handle
 * @param  Dns1_addr: pointer to the DNS1 address array
 * @param  Dns2_addr: pointer to the DNS2 address array
 * @param  Dns3_addr: pointer to the DNS3 address array
 * @return Operation status
 */
RCP_Status_t RCP_Net_SetDnsAddress(RCP_Object_t *Obj, uint8_t Dns1_addr[4],
                                   uint8_t Dns2_addr[4], uint8_t Dns3_addr[4]);

/** @brief  Ping a remote host
 * @param  Obj: pointer to module handle
 * @param  location: pointer to the remote host address
 * @param  length: length of the remote host address
 * @param  count: number of ping requests to send
 * @param  interval: time interval between each ping request : [100, 3500] (in
 * ms)
 * @param  timeout: maximum time to wait for a ping response : [100, 3500] (in
 * ms)
 * @param  ping_result: pointer to the ping result structure
 * @return Operation status
 */
RCP_Status_t RCP_Net_Ping(RCP_Object_t *Obj, char *location, uint16_t length,
                          uint16_t count, uint16_t interval, uint16_t timeout,
                          RCP_Net_PingResult_t *ping_result);

/**
 * @brief  DNS Lookup to get IP address
 * @param  Obj: pointer to module handle
 * @param  url: Domain Name to be resolved
 * @param  ip_address: IP address structure with the result address
 * @param  resolve_type: Address type to resolve.
 *         When resolve succeed, it is update with the effective address type
 * obtained.
 * @return Operation status
 */
RCP_Status_t
RCP_Net_ResolveHostAddress(RCP_Object_t *Obj, const char *url,
                           RCP_Net_Ip_addr_t *ip_address,
                           RCP_Net_Dns_ResolveType_e *resolve_type);

/**
 * @brief  Convert an IPv6 address string to four host-order 32-bit words
 * @param  src: textual IPv6 address (compressed or full)
 * @param  dst: uint32_t[4] destination array
 * @return 1 success, 0 failure.
 */
int32_t RCP_Net_Inet6_aton(const char *src, uint32_t dst[4]);

/**
 * @brief  Configure and Start a Client connection
 * @param  Obj: pointer to module handle
 * @param  conn: pointer to the connection structure
 * @return Operation status
 */
RCP_Status_t RCP_Net_StartClientConnection(RCP_Object_t *Obj,
                                           RCP_Net_Connection_t *conn);

/**
 * @brief  Stop Client connection
 * @param  Obj: pointer to module handle
 * @param  conn: pointer to the connection structure
 * @return Operation status
 */
RCP_Status_t RCP_Net_StopClientConnection(RCP_Object_t *Obj,
                                          RCP_Net_Connection_t *conn);

/**
 * @brief  Configure and Start a Server
 * @param  Obj: pointer to module handle
 * @param  Port: Connection Port
 * @param  protocol: Connection Type
 * @param  ca_enable: 1 to enable CA
 * @param  keepalive: keepalive time in seconds
 * @return Operation status
 */
RCP_Status_t RCP_Net_StartServer(RCP_Object_t *Obj, uint32_t Port,
                                 RCP_Net_Protocol_e protocol, uint8_t ca_enable,
                                 uint32_t keepalive);

/**
 * @brief  Stop a Server
 * @param  Obj: pointer to module handle
 * @param  close_connections: 1 to close all of the server connections
 * @return Operation status
 */
RCP_Status_t RCP_Net_StopServer(RCP_Object_t *Obj, uint8_t close_connections);

/**
 * @brief  Send command to set Server Maximum concurrent connections
 * @param  Obj: pointer to module handle
 * @param  MaxConnections: Server maximum concurrent connections to set in range
 * [1,5]
 * @return Operation status
 */
RCP_Status_t RCP_Net_SetServerMaxConnections(RCP_Object_t *Obj,
                                             uint8_t MaxConnections);

/**
 * @brief  Send command to set options for a given socket
 * @param  Obj: pointer to module handle
 * @param  Socket: Connection ID of the socket
 * @param  Linger:  Configure the SO_LINGER options for the socket
 * @param  TcpNoDelay:  Configure the TCP_NODELAY option for the socket
 * @param  SoSndTimeout:  Configure the SO_SNDTIMEO option for socket
 * @param  KeepAlive: Configure the SO_KEEPALIVE option for socket
 * @return Operation status
 */
RCP_Status_t RCP_Net_SetTCPOpt(RCP_Object_t *Obj, uint8_t Socket,
                               int16_t Linger, uint16_t TcpNoDelay,
                               uint16_t SoSndTimeout, uint16_t KeepAlive);

/**
 * @brief  Get information for an opened socket
 * @param  Obj: pointer to module handle
 * @param  Socket: Connection ID of the socket to get information for
 * @param  conn: pointer to the connection structure to fill with socket
 * information
 * @return Operation status
 */
RCP_Status_t RCP_Net_GetSocketInformation(RCP_Object_t *Obj, uint8_t Socket,
                                          RCP_Net_Connection_t *conn);

/**
 * @brief  Send an amount data over Wi-Fi
 * @param  Obj: pointer to module handle
 * @param  Socket: number of the socket
 * @param  pdata: pointer to data
 * @param  req_len: nr of bytes of the data to be sent
 * @param  SentLen: pointer to variable which contains nr of bytes  sent
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Net_SendData(RCP_Object_t *Obj, uint8_t Socket, uint8_t *pdata,
                              uint32_t req_len, uint32_t *SentLen,
                              uint32_t Timeout);

/**
 * @brief  Send an amount data over Wi-Fi using non connected protocols
 * @param  Obj: pointer to module handle
 * @param  Socket: number of the socket
 * @param  IpAddress: IP address of remote host to send data to
 * @param  Port: Port of remote host to send data to
 * @param  pdata: pointer to data
 * @param  req_len: nr of bytes of the data to be sent
 * @param  SentLen: pointer to variable which contains nr of bytes  sent
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Net_SendData_Non_Connected(RCP_Object_t *Obj, uint8_t Socket,
                                            char *IpAddress, uint32_t Port,
                                            uint8_t *pdata, uint32_t req_len,
                                            uint32_t *SentLen,
                                            uint32_t Timeout);

/**
 * @brief  Set Receive buffer length when reading data from socket
 * @param  Obj: pointer to module handle
 * @param  Socket: number of the socket
 * @param  BufLen: Buffer length to set
 * @return Operation status
 */
RCP_Status_t RCP_Net_SetReceiveBufferLen(RCP_Object_t *Obj, uint8_t Socket,
                                         uint32_t BufLen);

/**
 * @brief  Get Receive buffer length when reading data from socket
 * @param  Obj: pointer to module handle
 * @param  Socket: number of the socket
 * @param  BufLen: pointer to retrieve the length of the NCP buffer
 * @return Operation status
 */
RCP_Status_t RCP_Net_GetReceiveBufferLen(RCP_Object_t *Obj, uint8_t Socket,
                                         uint32_t *BufLen);

/**
 * @brief  Inquire RCP to check if socket data is available on a socket (Passive
 * mode)
 * @param  Obj: pointer to module handle
 * @param  Socket: number of the socket
 * @param  AvailableDataSize: pointer where to return the available data size on
 * a socket
 * @return Operation status
 */
RCP_Status_t RCP_Net_IsDataAvailableOnSocket(RCP_Object_t *Obj, uint8_t Socket,
                                             uint32_t *AvailableDataSize);

/**
 * @brief  Pull data from RCP on a socket (Passive mode)
 * @param  Obj: pointer to module handle
 * @param  Socket: number of the socket
 * @param  req_len: nr of bytes of the data to be sent
 * @param  pData: pointer to data
 * @param  Receivedlen: pointer to variable which contains nr of bytes  sent
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Net_PullDataFromSocket(RCP_Object_t *Obj, uint8_t Socket,
                                        uint32_t req_len, uint8_t *pData,
                                        uint32_t *Receivedlen,
                                        uint32_t Timeout);

/**
 * @brief  Send command to get current SNTP status, timezone and servers and
 * store results
 * @param  Obj: pointer to module handle
 * @param  Enable:  SNTP usage status
 * @param  Timezone:  Configured Timezone
 * @param  SntpServer1:  Configured Primary SNTP Server URL
 * @param  SntpServer2:  Configured Secondary SNTP Server URL
 * @param  SntpServer3:  Configured Third SNTP Server URL
 * @return Operation status
 */
RCP_Status_t RCP_Net_SNTP_GetConfiguration(RCP_Object_t *Obj, uint8_t *Enable,
                                           int16_t *Timezone,
                                           uint8_t SntpServer1[64],
                                           uint8_t SntpServer2[64],
                                           uint8_t SntpServer3[64]);

/**
 * @brief  Send command to set SNTP status, timezone and servers
 * @param  Obj: pointer to module handle
 * @param  Enable:  Enable/Disable SNTP usage
 * @param  Timezone:  Timezone to set in one of the 2 following formats:
 *                     - range [-12,14]: marks most of the time zones by offset
 * from UTC in whole hours
 *                     - HHmm with HH in range[-12,+14] and mm in range [00,59]
 * @param  SntpServer1:  Primary SNTP Server URL to use
 * @param  SntpServer2:  Secondary SNTP Server URL to use
 * @param  SntpServer3:  Third SNTP Server URL to use
 * @return Operation status
 */
RCP_Status_t RCP_Net_SNTP_SetConfiguration(RCP_Object_t *Obj, uint8_t Enable,
                                           int16_t Timezone,
                                           uint8_t *SntpServer1,
                                           uint8_t *SntpServer2,
                                           uint8_t *SntpServer3);

/**
 * @brief  Send command to get SNTP Synchronization interval and store results
 * @param  Obj: pointer to module handle
 * @param  Interval:  Configured SNTP time synchronization interval, in seconds
 * @return Operation status
 */
RCP_Status_t RCP_Net_SNTP_GetInterval(RCP_Object_t *Obj, uint16_t *Interval);

/**
 * @brief  Send command to set SNTP Synchronization interval
 * @param  Obj: pointer to module handle
 * @param  Interval:  SNTP time synchronization interval, in seconds
 * (range:[15,4294967])
 * @return Operation status
 */
RCP_Status_t RCP_Net_SNTP_SetInterval(RCP_Object_t *Obj, uint16_t Interval);

/**
 * @brief  Send command to query date string from SNTP, the used format is
 * asctime style time and store results
 * @param  Obj: pointer to module handle
 * @param  Time: pointer to the structure where to store the date string
 * @return Operation status
 */
RCP_Status_t RCP_Net_SNTP_GetTime(RCP_Object_t *Obj, RCP_Net_Time_t *Time);

/**
 * @brief  Send command to set SSL configuration for a given socket (key and
 * certificates names and Auth method)
 * @param  Obj: pointer to module handle
 * @param  Socket: Connection ID of the socket
 * @param  AuthMode:  Authentication Mode configured (0 : None, 1: Client Only,
 * 2: Server Only, 3: Mutual Auth )
 * @param  Certificate:  Client certificate name to use (saved in NCP romfs)
 * @param  PrivateKey:  Private key name to use(saved in NCP romfs), it must
 * match \p Certificate
 * @param  CaCertificate:  Certificate Authority's to use(saved in NCP romfs),
 * it must be the \p Certificate issuer
 * @return Operation status
 */
RCP_Status_t RCP_Net_SSL_SetConfiguration(RCP_Object_t *Obj, uint8_t Socket,
                                          uint8_t AuthMode,
                                          uint8_t *Certificate,
                                          uint8_t *PrivateKey,
                                          uint8_t *CaCertificate);

/**
 * @brief  Send command to set SNI to use for a given socket
 * @param  Obj: pointer to module handle
 * @param  Socket: Connection ID of the socket
 * @param  SslSni: Server Name indication to advertise when connecting to an SSL
 * Server
 * @return Operation status
 */
RCP_Status_t RCP_Net_SSL_SetServerName(RCP_Object_t *Obj, uint8_t Socket,
                                       uint8_t *SslSni);

/**
 * @brief  Send command to get ALPNs to use for a given socket
 * @param  Obj: pointer to module handle
 * @param  Socket: Connection ID of the socket
 * @param  Alpn1: Primary Application Layer Protocol Name to advertise when
 * connecting to an SSL Server
 * @param  Alpn2: Secondary Application Layer Protocol Name to advertise when
 * connecting to an SSL Server
 * @param  Alpn3: Third Application Layer Protocol Name to advertise when
 * connecting to an SSL Server
 * @return Operation status
 */
RCP_Status_t RCP_Net_SSL_SetALPN(RCP_Object_t *Obj, uint8_t Socket,
                                 uint8_t *Alpn1, uint8_t *Alpn2,
                                 uint8_t *Alpn3);

/**
 * @brief  Send command to set PSK to use for a given socket
 * @param  Obj: pointer to module handle
 * @param  Socket: Connection ID of the socket
 * @param  Psk: pre-shared key to use to open an SSL connection for the given
 * Connection ID
 * @param  Hint: pre-shared key identity to use to open an SSL connection for
 * the given Connection ID
 * @return Operation status
 */
RCP_Status_t RCP_Net_SSL_SetPSK(RCP_Object_t *Obj, uint8_t Socket, uint8_t *Psk,
                                uint8_t *Hint);

/** @} */

/* ===================================================================== */
/** @defgroup RCP_MQTT_Functions RCP Driver MQTT Functions
 * @ingroup  RCP_MQTT
 * @{
 */
/* ===================================================================== */

/**
 * @brief  Initialize BLE device as Client/Server or de-initialize
 * @param  Obj: pointer to module handle
 * @param  p_recv_data: pointer to received data buffer (allocated by the upper
 * layer)
 * @param  recv_data_buf_len: length of the received data buffer
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_Init(RCP_Object_t *Obj, uint8_t *p_recv_data,
                           uint32_t recv_data_buf_len);

/**
 * @brief  De-initialize MQTT device
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_DeInit(RCP_Object_t *Obj);

/**
 * @brief  Send command to set MQTT User configuration
 * @param  Obj: pointer to module handle
 * @param  Scheme: Authentication scheme (0: No security, 1: Username/Password,
 * 2: Certificate)
 * @param  ClientId: pointer to the client ID
 * @param  Username: pointer to the username
 * @param  Password: pointer to the password
 * @param  Certificate: pointer to the client certificate
 * @param  PrivateKey: pointer to the client private key
 * @param  CaCertificate: pointer to the CA certificate
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_SetUserConfiguration(RCP_Object_t *Obj, uint32_t Scheme,
                                           uint8_t *ClientId, uint8_t *Username,
                                           uint8_t *Password,
                                           uint8_t *Certificate,
                                           uint8_t *PrivateKey,
                                           uint8_t *CaCertificate);
/**
 * @brief  Send command to get MQTT User configuration
 * @param  Obj: pointer to module handle
 * @param  ClientId: pointer to the client ID
 * @param  Username: pointer to the username
 * @param  Password: pointer to the password
 * @param  Certificate: pointer to the client certificate
 * @param  PrivateKey: pointer to the client private key
 * @param  CaCertificate: pointer to the CA certificate
 * @return Operation status
 */
RCP_Status_t
RCP_MQTT_GetUserConfiguration(RCP_Object_t *Obj, uint8_t ClientId[32],
                              uint8_t Username[32], uint8_t Password[32],
                              uint8_t Certificate[64], uint8_t PrivateKey[64],
                              uint8_t CaCertificate[64]);

/**
 * @brief  Send command to set MQTT configuration
 * @param  Obj: pointer to module handle
 * @param  KeepAlive: Keep alive time in seconds
 * @param  DisableCleanSession: Clean session flag
 * @param  WillTopic: pointer to the will topic
 * @param  WillMessage: pointer to the will message
 * @param  WillQos: QoS level for the will message
 * @param  WillRetain: Retain flag for the will message
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_SetConfiguration(RCP_Object_t *Obj, uint32_t KeepAlive,
                                       uint32_t DisableCleanSession,
                                       uint8_t *WillTopic, uint8_t *WillMessage,
                                       uint32_t WillQos, uint32_t WillRetain);

/**
 * @brief  Send command to get MQTT configuration
 * @param  Obj: pointer to module handle
 * @param  KeepAlive: Keep alive time in seconds
 * @param  DisableCleanSession: Clean session flag
 * @param  WillTopic: pointer to the will topic
 * @param  WillMessage: pointer to the will message
 * @param  WillQos: QoS level for the will message
 * @param  WillRetain: Retain flag for the will message
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_GetConfiguration(RCP_Object_t *Obj, uint32_t *KeepAlive,
                                       uint32_t *DisableCleanSession,
                                       uint8_t WillTopic[128],
                                       uint8_t WillMessage[128],
                                       uint32_t *WillQos, uint32_t *WillRetain);

/**
 * @brief  Send command to set MQTT SNI
 * @param  Obj: pointer to module handle
 * @param  SNI: pointer to the SNI
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_SetSNI(RCP_Object_t *Obj, uint8_t *SNI);

/**
 * @brief  Send command to get MQTT SNI
 * @param  Obj: pointer to module handle
 * @param  SNI: pointer to the SNI
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_GetSNI(RCP_Object_t *Obj, uint8_t SNI[128]);

/**
 * @brief  Send command to connect to the MQTT broker
 * @param  Obj: pointer to module handle
 * @param  Host: pointer to the hostname broker
 * @param  Port: port number
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_Connect(RCP_Object_t *Obj, uint8_t *Host, uint16_t Port);

/**
 * @brief  Send command to get the connection status to the MQTT broker
 * @param  Obj: pointer to module handle
 * @param  Host: pointer to the hostname broker
 * @param  Port: pointer to the port
 * @param  Scheme: pointer to the scheme
 * @param  State: pointer to the state
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_GetConnectionStatus(RCP_Object_t *Obj, uint8_t *Host,
                                          uint32_t *Port, uint32_t *Scheme,
                                          uint32_t *State);

/**
 * @brief  Send command to disconnect from the MQTT broker
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_Disconnect(RCP_Object_t *Obj);

/**
 * @brief  Send command to subscribe to a topic
 * @param  Obj: pointer to module handle
 * @param  Topic: pointer to the topic
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_Subscribe(RCP_Object_t *Obj, uint8_t *Topic);

/**
 * @brief  Send command to get the list of subscribed topics
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_GetSubscribedTopics(RCP_Object_t *Obj);

/**
 * @brief  Send command to unsubscribe from a topic
 * @param  Obj: pointer to module handle
 * @param  Topic: pointer to the topic
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_Unsubscribe(RCP_Object_t *Obj, uint8_t *Topic);

/**
 * @brief  Send command to publish a message to a topic
 * @param  Obj: pointer to module handle
 * @param  Topic: pointer to the topic
 * @param  Message: pointer to the message
 * @param  Message_len: length of the message
 * @param  Qos: QoS. 0: At most once, 1: At least once, 2: Exactly once
 * @param  Retain: Retain flag
 * @return Operation status
 */
RCP_Status_t RCP_MQTT_Publish(RCP_Object_t *Obj, uint8_t *Topic,
                              uint8_t *Message, uint32_t Message_len,
                              uint32_t Qos, uint32_t Retain);

/** @} */

/* ===================================================================== */
/** @defgroup RCP_BLE_Functions RCP Driver BLE Functions
 * @ingroup  RCP_BLE
 * @{
 */
/* ===================================================================== */
/**
 * @brief  Initialize BLE device as Client/Server or de-initialize
 * @param  Obj: pointer to module handle
 * @param  mode: BLE mode to initialize (1: Client, 2: Server, 3: Dual, 0:
 * De-init)
 * @param  p_recv_data: pointer to received data buffer
 * @param  req_len: length of the received data buffer
 * @return Operation status
 */
RCP_Status_t RCP_Ble_Init(RCP_Object_t *Obj, uint8_t mode, uint8_t *p_recv_data,
                          uint32_t req_len);

/**
 * @brief  De-initialize BLE device
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_DeInit(RCP_Object_t *Obj);

/**
 * @brief  Set/change the pointer where to copy the Recv Data
 * @param  Obj: pointer to module handle
 * @param  p_recv_data: pointer to received data buffer
 * @param  recv_data_buf_size: length of the received data buffer
 * @return Operation status
 * @note   This function shall only be called when executing the callback (never
 * on applicative task)
 */
RCP_Status_t RCP_Ble_SetRecvDataPtr(RCP_Object_t *Obj, uint8_t *p_recv_data,
                                    uint32_t recv_data_buf_size);

/**
 * @brief  Get BLE module initialization mode
 * @param  Obj: pointer to module handle
 * @param  Mode: Current BLE mode (1: Client, 2: Server, 0: De-init)
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetInitMode(RCP_Object_t *Obj, RCP_Ble_Mode_e *Mode);

/**
 * @brief  Set BLE TX power
 * @param  Obj: pointer to module handle
 * @param  power: TX power
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetTxPower(RCP_Object_t *Obj, uint32_t power);

/**
 * @brief  Get BLE TX power
 * @param  Obj: pointer to module handle
 * @param  power: TX power
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetTxPower(RCP_Object_t *Obj, uint32_t *power);

/**
 * @brief  Start BLE advertising
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_AdvStart(RCP_Object_t *Obj);

/**
 * @brief  Stop BLE advertising
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_AdvStop(RCP_Object_t *Obj);

/**
 * @brief  Stop BLE connection
 * @param  Obj: pointer to module handle
 * @param  conn_handle: connection handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_Disconnect(RCP_Object_t *Obj, uint32_t conn_handle);

/**
 * @brief  Exchange BLE MTU length
 * @param  Obj: pointer to module handle
 * @param  conn_handle: connection handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ExchangeMTU(RCP_Object_t *Obj, uint32_t conn_handle);

/**
 * @brief  Set the BLE BD address of the device
 * @param  Obj: pointer to module handle
 * @param  bdaddr: pointer to the BD address array
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetBDAddress(RCP_Object_t *Obj, const uint8_t *bdaddr);

/**
 * @brief  Get the BD address of the module
 * @param  Obj: pointer to module handle
 * @param  BdAddr: pointer to the BD address array
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetBDAddress(RCP_Object_t *Obj, uint8_t *BdAddr);

/**
 * @brief  Set the BLE device name
 * @param  Obj: pointer to module handle
 * @param  name: pointer to the device name
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetDeviceName(RCP_Object_t *Obj, const char *name);

/**
 * @brief  Get the BLE Device name
 * @param  Obj: pointer to module handle
 * @param  DeviceName: string containing the host name
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetDeviceName(RCP_Object_t *Obj, char *DeviceName);

/**
 * @brief  Set the BLE Advertising data
 * @param  Obj: pointer to module handle
 * @param  advdata: pointer to the adv data array
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetAdvData(RCP_Object_t *Obj, const char *advdata);

/**
 * @brief  Set the BLE scan response data
 * @param  Obj: pointer to module handle
 * @param  scanrespdata: pointer to the scan response data array
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetScanRespData(RCP_Object_t *Obj,
                                     const char *scanrespdata);

/**
 * @brief  Set the BLE Advertising parameters
 * @param  Obj: pointer to module handle
 * @param  adv_int_min: minimum advertising interval this parameter is
 * multiplied by 0.625ms
 * @param  adv_int_max: maximum advertising interval this parameter is
 * multiplied by 0.625ms
 * @param  adv_type: Advertising type
 * @param  adv_channel: advertising channel
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetAdvParam(RCP_Object_t *Obj, uint32_t adv_int_min,
                                 uint32_t adv_int_max, uint8_t adv_type,
                                 uint8_t adv_channel);

/**
 * @brief  Enable/disable BLE device scan
 * @param  Obj: pointer to module handle
 * @param  enable: Enable/disable scan
 * @return Operation status
 */
RCP_Status_t RCP_Ble_Scan(RCP_Object_t *Obj, uint8_t enable);

/**
 * @brief  Set the BLE scan parameters
 * @param  Obj: pointer to module handle
 * @param  scan_type: Type of scan (0:passive, 1:active)
 * @param  own_addr_type: type of BLE BD address
 * @param  filter_policy: Scan filter policy
 * @param  scan_interval: scan interval this parameter is multiplied by 0.625ms
 * @param  scan_window: scan window this parameter is multiplied by 0.625ms
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetScanParam(RCP_Object_t *Obj, uint8_t scan_type,
                                  uint8_t own_addr_type, uint8_t filter_policy,
                                  uint32_t scan_interval, uint32_t scan_window);

/**
 * @brief  Get the Scan parameters
 * @param  Obj: pointer to module handle
 * @param  ScanType: pointer to get scan type
 * @param  OwnAddrType: pointer to get BLE BD address type
 * @param  FilterPolicy: pointer to get scan filter
 * @param  ScanInterval: pointer to get scan interval
 * @param  ScanWindow: pointer to get scan window
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetScanParam(RCP_Object_t *Obj, uint32_t *ScanType,
                                  uint32_t *OwnAddrType, uint32_t *FilterPolicy,
                                  uint32_t *ScanInterval, uint32_t *ScanWindow);

/**
 * @brief  Get the Advertising parameters
 * @param  Obj: pointer to module handle
 * @param  AdvIntMin: pointer to get minimum advertising interval
 * @param  AdvIntMax: pointer to get maximum advertising interval
 * @param  AdvType: pointer to get advertising type
 * @param  ChannelMap: pointer to get advertising channel
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetAdvParam(RCP_Object_t *Obj, uint32_t *AdvIntMin,
                                 uint32_t *AdvIntMax, uint32_t *AdvType,
                                 uint32_t *ChannelMap);

/**
 * @brief  Set the BLE Connection parameters
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  conn_int_min: minimum connecting interval this parameter is
 * multiplied by 1.25ms
 * @param  conn_int_max: maximum connecting interval this parameter is
 * multiplied by 1.25ms
 * @param  latency: latency
 * @param  timeout: connection timeout this parameter is multiplied by 10ms
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetConnParam(RCP_Object_t *Obj, uint32_t conn_handle,
                                  uint32_t conn_int_min, uint32_t conn_int_max,
                                  uint32_t latency, uint32_t timeout);

/**
 * @brief  Get the connection parameters
 * @param  Obj: pointer to module handle
 * @param  ConnHandle: pointer to get BLE connection handle
 * @param  ConnIntMin: pointer to get minimum connection interval
 * @param  ConnIntMax: pointer to get maximum connection interval
 * @param  ConnIntCurrent: pointer to get current connection interval
 * @param  Latency: pointer to get latency
 * @param  Timeout: pointer to get connection timeout
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetConnParam(RCP_Object_t *Obj, uint32_t *ConnHandle,
                                  uint32_t *ConnIntMin, uint32_t *ConnIntMax,
                                  uint32_t *ConnIntCurrent, uint32_t *Latency,
                                  uint32_t *Timeout);

/**
 * @brief  Get the connection information
 * @param  Obj: pointer to module handle
 * @param  ConnHandle: pointer to get BLE connection handle
 * @param  RemoteBDAddr: pointer to get the remote device address
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetConn(RCP_Object_t *Obj, uint32_t *ConnHandle,
                             uint8_t *RemoteBDAddr);

/**
 * @brief  Create connection to a remote device
 * @param  Obj: pointer to module handle
 * @param  conn_handle: index of the BLE connection
 * @param  RemoteBDAddr: pointer to the remote device address
 * @return Operation status
 */
RCP_Status_t RCP_Ble_Connect(RCP_Object_t *Obj, uint32_t conn_handle,
                             uint8_t *RemoteBDAddr);
/**
 * @brief  Set the BLE Data length
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  tx_bytes: data packet length. Range [27,251]
 * @param  tx_trans_time: data packet transition time
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetDataLength(RCP_Object_t *Obj, uint32_t conn_handle,
                                   uint32_t tx_bytes, uint32_t tx_trans_time);

/**
 * @brief  Create BLE Service
 * @param  Obj: pointer to module handle
 * @param  service_index: index of the service to create
 * @param  service_uuid: UUID of the service to create
 * @param  uuid_type: UUID type (0: 16-bit or 2: 128-bit)
 * @return Operation status
 */
RCP_Status_t RCP_Ble_CreateService(RCP_Object_t *Obj, uint8_t service_index,
                                   const char *service_uuid, uint8_t uuid_type);

/**
 * @brief  Delete existing BLE Service
 * @param  Obj: pointer to module handle
 * @param  service_index: index of the service to create
 * @return Operation status
 */
RCP_Status_t RCP_Ble_DeleteService(RCP_Object_t *Obj, uint8_t service_index);

/**
 * @brief  List BLE Services created
 * @param  Obj: pointer to module handle
 * @param  ServiceInfo: pointer to get the existing service
 * @param  service_idx: index of the service to get
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetService(RCP_Object_t *Obj,
                                RCP_Ble_Service_t *ServiceInfo,
                                int8_t service_idx);

/**
 * @brief  Create BLE Characteristic
 * @param  Obj: pointer to module handle
 * @param  service_index: index of the service containing the new characteristic
 * @param  char_index: index of the characteristic to create
 * @param  char_uuid: UUID of the characteristic to create
 * @param  uuid_type: UUID type (0: 16-bit or 2: 128-bit)
 * @param  char_property: property of the characteristic to create
 * @param  char_permission: permission of the characteristic to create
 * @return Operation status
 */
RCP_Status_t
RCP_Ble_CreateCharacteristic(RCP_Object_t *Obj, uint8_t service_index,
                             uint8_t char_index, const char *char_uuid,
                             uint8_t uuid_type, uint8_t char_property,
                             uint8_t char_permission);

/**
 * @brief  List BLE Characteristics created
 * @param  Obj: pointer to module handle
 * @param  CharacInfo: pointer to get the existing characteristics
 * @param  service_index: index of the service containing the characteristic to
 * get
 * @param  char_index: index of the characteristic to get
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetCharacteristic(RCP_Object_t *Obj,
                                       RCP_Ble_Characteristic_t *CharacInfo,
                                       int8_t service_index, int8_t char_index);

/**
 * @brief  Register BLE Characteristics
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_RegisterCharacteristics(RCP_Object_t *Obj);

/**
 * @brief  Discover BLE services of remote device
 * @param  Obj: pointer to module handle
 * @param  conn_handle: index of the BLE connection
 * @return Operation status
 */
RCP_Status_t RCP_Ble_RemoteServiceDiscovery(RCP_Object_t *Obj,
                                            uint8_t conn_handle);

/**
 * @brief  Discover BLE Characteristics of a service remote device
 * @param  Obj: pointer to module handle
 * @param  conn_handle: index of the BLE connection
 * @param  service_index: index of the BLE service to discover
 * @return Operation status
 */
RCP_Status_t RCP_Ble_RemoteCharDiscovery(RCP_Object_t *Obj, uint8_t conn_handle,
                                         uint8_t service_index);

/**
 * @brief  Notify the Characteristic Value from the Server to a Client
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  service_index: index of the service containing characteristic to
 * notify
 * @param  char_index: index of the characteristic to notify
 * @param  pdata: pointer to the indication data
 * @param  req_len: length of the indication data
 * @param  SentLen: pointer to the length of the sent data
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ServerSendNotification(RCP_Object_t *Obj,
                                            uint8_t conn_handle,
                                            uint8_t service_index,
                                            uint8_t char_index, uint8_t *pdata,
                                            uint32_t req_len, uint32_t *SentLen,
                                            uint32_t Timeout);

/**
 * @brief  Indicate the Characteristic Value from the Server to a Client
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  service_index: index of the service containing characteristic to
 * indicate
 * @param  char_index: index of the characteristic to indicate
 * @param  pdata: pointer to the notification data
 * @param  req_len: length of the notification data
 * @param  SentLen: pointer to the length of the sent data
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ServerSendIndication(RCP_Object_t *Obj,
                                          uint8_t conn_handle,
                                          uint8_t service_index,
                                          uint8_t char_index, uint8_t *pdata,
                                          uint32_t req_len, uint32_t *SentLen,
                                          uint32_t Timeout);

/**
 * @brief  Set the data when Client read characteristic from the Server
 * @param  Obj: pointer to module handle
 * @param  service_index: index of the service containing characteristic to read
 * @param  char_index: index of the characteristic to read
 * @param  pdata: pointer to the data
 * @param  req_len: length of the data to set
 * @param  SentLen: pointer to the length of the sent data
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ServerSetReadData(RCP_Object_t *Obj, uint8_t service_index,
                                       uint8_t char_index, uint8_t *pdata,
                                       uint32_t req_len, uint32_t *SentLen,
                                       uint32_t Timeout);

/**
 * @brief  Write data to a Server characteristic
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  service_index: index of the service to write data in
 * @param  char_index: index of the characteristic to write data in
 * @param  pdata: pointer to the data to write
 * @param  req_len: length of the data to write
 * @param  SentLen: pointer to the length of the sent data
 * @param  Timeout: timeout in ms
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ClientWriteData(RCP_Object_t *Obj, uint8_t conn_handle,
                                     uint8_t service_index, uint8_t char_index,
                                     uint8_t *pdata, uint32_t req_len,
                                     uint32_t *SentLen, uint32_t Timeout);

/**
 * @brief  Read data from a Server characteristic
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  service_index: index of the service to read data from
 * @param  char_index: index of the characteristic to read data from
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ClientReadData(RCP_Object_t *Obj, uint8_t conn_handle,
                                    uint8_t service_index, uint8_t char_index);

/**
 * @brief  Subscribe to notifications or indications from a Server
 * characteristic
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  char_value_handle: Characteristic value handle
 * @param  char_prop: property of the characteristic to subscribe (1 =
 * notification, 2 = indication)
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ClientSubscribeChar(RCP_Object_t *Obj, uint8_t conn_handle,
                                         uint8_t char_value_handle,
                                         uint8_t char_prop);

/**
 * @brief  Unsubscribe to notifications or indications from a Server
 * characteristic
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  char_value_handle: Characteristic value handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_ClientUnsubscribeChar(RCP_Object_t *Obj,
                                           uint8_t conn_handle,
                                           uint8_t char_value_handle);

/**
 * @brief  Set BLE security parameters
 * @param  Obj: pointer to module handle
 * @param  security_parameter: BLE security parameter
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SetSecurityParam(RCP_Object_t *Obj,
                                      uint8_t security_parameter);

/**
 * @brief  Get BLE security parameters
 * @param  Obj: pointer to module handle
 * @param  SecurityParameter: pointer to get BLE security parameter
 * @return Operation status
 */
RCP_Status_t RCP_Ble_GetSecurityParam(RCP_Object_t *Obj,
                                      uint32_t *SecurityParameter);

/**
 * @brief  Start BLE security
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  security_level: security level. Range: [0,4]
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecurityStart(RCP_Object_t *Obj, uint8_t conn_handle,
                                   uint8_t security_level);

/**
 * @brief  BLE security pass key confirm
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecurityPassKeyConfirm(RCP_Object_t *Obj,
                                            uint8_t conn_handle);

/**
 * @brief  BLE pairing confirm
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecurityPairingConfirm(RCP_Object_t *Obj,
                                            uint8_t conn_handle);

/**
 * @brief  BLE set pass key
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @param  passkey: BLE security pass key
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecuritySetPassKey(RCP_Object_t *Obj, uint8_t conn_handle,
                                        uint32_t passkey);

/**
 * @brief  BLE pairing cancel
 * @param  Obj: pointer to module handle
 * @param  conn_handle: BLE connection handle
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecurityPairingCancel(RCP_Object_t *Obj,
                                           uint8_t conn_handle);

/**
 * @brief  BLE unpair
 * @param  Obj: pointer to module handle
 * @param  RemoteBDAddr: remote device address
 * @param  remote_addr_type: remote address type
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecurityUnpair(RCP_Object_t *Obj, uint8_t *RemoteBDAddr,
                                    uint32_t remote_addr_type);

/**
 * @brief  BLE get paired device list
 * @param  Obj: pointer to module handle
 * @param  RemoteBondedDevices: pointer to remote bonded devices list
 * @return Operation status
 */
RCP_Status_t RCP_Ble_SecurityGetBondedDeviceList(
    RCP_Object_t *Obj, RCP_Ble_Bonded_Devices_Result_t *RemoteBondedDevices);
/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RCP_API_H */
