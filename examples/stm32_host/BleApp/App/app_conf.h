/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_conf.h
  * @author  MCD Application Team
  * @brief   Application configuration file for STM32WPAN Middleware.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_CONF_H
#define APP_CONF_H

/* Includes ------------------------------------------------------------------*/
#include "ble_defs.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/******************************************************************************
 * BLE Application Config
 ******************************************************************************/

/**< generic parameters ******************************************************/
/**
 * Define Tx Power
 */
#define CFG_TX_POWER                      (0x19) /* 0x19 <=> -0.3 dBm */

/**
 * Definition of public BD Address,
 * when CFG_BD_ADDRESS = 0x000000000000 the BD address is generated based on Unique Device Number.
 */
#define CFG_BD_ADDRESS                    (0x000000000000)

/**
 * Define BD_ADDR type: define proper address. Can only be GAP_PUBLIC_ADDR (0x00) or GAP_STATIC_RANDOM_ADDR (0x01)
 */
#define CFG_BD_ADDRESS_DEVICE             (GAP_PUBLIC_ADDR)

/**
 * Define privacy: PRIVACY_DISABLED or PRIVACY_ENABLED
 */
#define CFG_PRIVACY                       (PRIVACY_DISABLED)

/**
 * Define BLE Address Type
 * Bluetooth address types defined in ble_legacy.h
 * if CFG_PRIVACY equals PRIVACY_DISABLED, CFG_BLE_ADDRESS_TYPE has 2 allowed values: GAP_PUBLIC_ADDR or GAP_STATIC_RANDOM_ADDR
 * if CFG_PRIVACY equals PRIVACY_ENABLED, CFG_BLE_ADDRESS_TYPE has 2 allowed values: GAP_RESOLVABLE_PRIVATE_ADDR or GAP_NON_RESOLVABLE_PRIVATE_ADDR
 */
#define CFG_BD_ADDRESS_TYPE               (GAP_PUBLIC_ADDR)

#define ADV_INTERVAL_MIN                  (80)
#define ADV_INTERVAL_MAX                  (100)
#define ADV_LP_INTERVAL_MIN               (1600)
#define ADV_LP_INTERVAL_MAX               (4000)
#define ADV_TYPE                          ADV_IND
#define ADV_FILTER                        HCI_ADV_FILTER_ACC_LIST_USED_FOR_ALL

/**
 * Define IO Authentication
 */
#define CFG_BONDING_MODE                  (1)
#define CFG_USED_FIXED_PIN                (0) /* 0->fixed pin is used ; 1->No fixed pin used*/
#define CFG_FIXED_PIN                     (111111)
#define CFG_ENCRYPTION_KEY_SIZE_MAX       (16)
#define CFG_ENCRYPTION_KEY_SIZE_MIN       (8)

/**
 * Define Input Output capabilities
 */
#define CFG_IO_CAPABILITY                 (IO_CAP_DISPLAY_YES_NO)

/**
 * Define Man In The Middle modes
 */
#define CFG_MITM_PROTECTION               (MITM_PROTECTION_REQUIRED_AS_MANDATORY)

/**
 * Define Secure Connections Support
 */
#define CFG_SC_SUPPORT                    (SC_PAIRING_OPTIONAL)

/**
 * Define Keypress Notification Support
 */
#define CFG_KEYPRESS_NOTIFICATION_SUPPORT (KEYPRESS_NOT_SUPPORTED)

/**
*   Identity root key used to derive IRK and DHK(Legacy)
*/
#define CFG_BLE_IR  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

/**
* Encryption root key used to derive LTK(Legacy) and CSRK
*/
#define CFG_BLE_ER  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

/* USER CODE BEGIN Generic_Parameters */

/* USER CODE END Generic_Parameters */

/*****************************************************/
/**< specific parameters */
/*****************************************************/

/* USER CODE BEGIN Specific_Parameters */

/* USER CODE END Specific_Parameters */

/******************************************************************************
 * BLE Stack
 ******************************************************************************/
/**
 * BLE stack options, bitmap to active or not some features at BleStack_Init() function call.
 * 
 * possible values are :
 
   BLE_OPTIONS_LL_ONLY                      
   BLE_OPTIONS_NO_SVC_CHANGE_DESC           
   BLE_OPTIONS_DEV_NAME_READ_ONLY           
   BLE_OPTIONS_EXTENDED_ADV                 
   BLE_OPTIONS_CS_ALGO_2                    
   BLE_OPTIONS_REDUCED_DB_IN_NVM            
   BLE_OPTIONS_GATT_CACHING                 
   BLE_OPTIONS_POWER_CLASS_1                
   BLE_OPTIONS_APPEARANCE_WRITABLE          
   BLE_OPTIONS_ENHANCED_ATT               
 */
#define CFG_BLE_OPTIONS             (0 | \
                                     0 | \
                                     0 | \
                                     0 | \
                                     0 | \
                                     0 | \
                                     0 | \
                                     0 | \
                                     0)

/**
 * Maximum number of simultaneous connections and advertising that the device will support.
 * This setting should not exceed the number of BLE connection supported by BLE host stack.
 */
#define CFG_BLE_NUM_LINK            (2)

/**
 * Maximum number of Services that can be stored in the GATT database.
 * Note that the GAP and GATT services are automatically added so this parameter should be 2 plus the number of user services
 */
#define CFG_BLE_NUM_GATT_SERVICES   (8)

/**
 * Maximum number of Attributes
 * (i.e. the number of characteristic + the number of characteristic values + the number of descriptors, excluding the services)
 * that can be stored in the GATT database.
 * Note that certain characteristics and relative descriptors are added automatically during device initialization
 * so this parameters should be 9 plus the number of user Attributes
 */
#define CFG_BLE_NUM_GATT_ATTRIBUTES (68)

/**
 * Maximum supported ATT_MTU size
 * This setting should be aligned with ATT_MTU value configured in the ble host
 */
#define CFG_BLE_ATT_MTU_MAX         (247)

/**
 * Size of the storage area for Attribute values
 *  This value depends on the number of attributes used by application. In particular the sum of the following quantities (in octets) should be made for each attribute:
 *  - attribute value length
 *  - 5, if UUID is 16 bit; 19, if UUID is 128 bit
 *  - 2, if server configuration descriptor is used
 *  - 2*DTM_NUM_LINK, if client configuration descriptor is used
 *  - 2, if extended properties is used
 *  The total amount of memory needed is the sum of the above quantities for each attribute.
 */
#define CFG_BLE_ATT_VALUE_ARRAY_SIZE  (1344)

/**
 * Maximum numbers of bearers that can be created for Enhanced ATT per ACL links
 */
#define CFG_BLE_EATT_BEARER_PER_LINK  (0)

/**
 * depth of the PREPARE WRITE queue when PREPARE WRITE REQUEST
 */
#define CFG_BLE_ATTR_PREPARE_WRITE_VALUE_SIZE       (30)

#define CFG_BLE_MBLOCK_COUNT_MARGIN                 (0x15)

#define PREP_WRITE_LIST_SIZE                        (BLE_DEFAULT_PREP_WRITE_LIST_SIZE)

/**
 * Number of allocated memory blocks used to transmit and receive data packets
 */
#define CFG_BLE_MBLOCK_COUNT          (BLE_MBLOCKS_CALC(PREP_WRITE_LIST_SIZE, \
                                       CFG_BLE_ATT_MTU_MAX, CFG_BLE_NUM_LINK) \
                                       + CFG_BLE_MBLOCK_COUNT_MARGIN)

/**
 * Size of the buffer used to store BLE host events
 */
#define CFG_BLE_HOST_EVENT_BUF_SIZE   (4096)
   
/**
 * Size of the buffer used to store BLE host events
 */
#define CFG_BLE_EXTRA_DATA_BUF_SIZE   (4096)

/**
 * Size of the Controller events FIFO buffer.
 */
#define CFG_BLE_CTRL_EVENT_FIFO_BUF_SIZE (4096)

/**
 * Appearance of device set into BLE GAP
 */
#define CFG_GAP_APPEARANCE            (GAP_APPEARANCE_UNKNOWN)

/**
 * Connection Oriented Channel parameters
 */
#define CFG_BLE_COC_NBR_MAX           (64)
#define CFG_BLE_COC_MPS_MAX           (248)
#define CFG_BLE_COC_INITIATOR_NBR_MAX (32)

/**
 * PHY preferences
 */
#define CFG_PHY_PREF                  (0)
#define CFG_PHY_PREF_TX               (HCI_TX_PHYS_LE_2M_PREF)
#define CFG_PHY_PREF_RX               (HCI_RX_PHYS_LE_2M_PREF)

/* USER CODE BEGIN BLE_Stack */

/* USER CODE END BLE_Stack */

#define BLE_CFG_HCI_EVENT_MASK     ( \
                                    0x0000000000000000ULL | /* No events specified */ \
                                    0x0000000000000010ULL | /* Disconnection Complete Event */ \
                                    0x0000000000000080ULL | /* Encryption Change Event */ \
                                    0x0000000000000800ULL | /* Read Remote Version Information Complete Event */ \
                                    0x0000000000008000ULL | /* Hardware Error Event */ \
                                    0x0000000002000000ULL | /* Data Buffer Overflow Event */ \
                                    0x0000800000000000ULL | /* Encryption Key Refresh Complete Event */ \
                                    0x2000000000000000ULL   /* LE Meta-Event */ \
                                   )

#define BLE_CFG_HCI_LE_EVENT_MASK  ( \
                                    0x0000000000000000ULL | /* No events specified */ \
                                    0x0000000000000001ULL | /* LE Connection Complete event */ \
                                    0x0000000000000004ULL   /* LE Connection Update Complete event */ \
                                   )

/******************************************************************************
 * Low Power
 *
 *  When CFG_LPM_LEVEL is set to:
 *   - 0 : Low Power Mode is not activated, RUN mode will be used.
 *   - 1 : Low power active, mode(s) selected with CFG_LPM_mode_SUPPORTED
 *   - 2 : In addition log and debug are disabled to reach lowest power figures.
 ******************************************************************************/

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/

#define LOW_POWER_DISABLE           0
#define LOW_POWER_SLEEP_ENABLE      1
#define LOW_POWER_STOP_ENABLE       2
#define LOW_POWER_STDBY_ENABLE      3

/** Low power configuration [0: disable / 1: sleep / 2: stop / 3: standby] */
#define CFG_LPM_LEVEL               (LOW_POWER_DISABLE)

/* USER CODE BEGIN Low_Power 0 */

/* USER CODE END Low_Power 0 */
/**
 * Supported requester to the MCU Low Power Manager - can be increased up  to 32
 * It list a bit mapping of all user of the Low Power Manager
 */
typedef enum
{
  CFG_LPM_APP,
  /* USER CODE BEGIN CFG_LPM_Id_t */
  /* USER CODE END CFG_LPM_Id_t */
} CFG_LPM_Id_t;

/* USER CODE BEGIN Low_Power 1 */

/* USER CODE END Low_Power 1 */

/******************************************************************************
 * RTC
 ******************************************************************************/

/* USER CODE BEGIN RTC */

/* USER CODE END RTC */

/*****************************************************************************
 * Logs
 *
 *****************************************************************************/
 /* Logs are enabled by defualt */
/* Usart used by LOG */
#define LOG_UART_HANDLER            
/* USER CODE BEGIN Logs */
/* USER CODE END Logs */
#define LOG_OUTPUT_PRINTF           0
#define LOG_OUTPUT_UART             1
#define LOG_OUTPUT_ITM              2
/** Select output log mode [0: printf / 1: UART / 2: ITM] */
#define LOG_OUTPUT_MODE             LOG_OUTPUT_ITM
/******************************************************************************
 * NVM configuration
 ******************************************************************************/

#define CFG_SNVMA_START_SECTOR_ID     ((FLASH_SIZE / FLASH_PAGE_SIZE) - 2u)

#define CFG_SNVMA_START_ADDRESS       (FLASH_BASE + (FLASH_PAGE_SIZE * (CFG_SNVMA_START_SECTOR_ID)) - FLASH_PAGE_SIZE)

/* Number of 64-bit words in NVM flash area */
#define CFG_BLE_NVM_SIZE_MAX          (((2048/8)-4))
   
/* USER CODE BEGIN NVM_Configuration */

/* USER CODE END NVM_Configuration */

/******************************************************************************
 * Debugger
 *
 * Enable/Disable MCU Debugger pins (dbg serial wires)
 * @note  by HW serial wires are ON by default, need to put them OFF to save power
 *
 ******************************************************************************/
#define CFG_DEBUGGER_LEVEL                     (1)

/******************************************************************************
 * BSP configuration
 ******************************************************************************/
/* USER CODE BEGIN Defines */
#define CFG_BSP_ON_NUCLEO                       (1)

/**
 * User interaction
 * When CFG_LED_SUPPORTED is set, LEDS are activated if requested
 * When CFG_BUTTON_SUPPORTED is set, the push button are activated if requested
 */
#define CFG_LED_SUPPORTED                       (1)
#define CFG_BUTTON_SUPPORTED                    (1)

/* USER CODE BEGIN Defines_2 */
/* USER CODE END Defines_2 */

#endif /*APP_CONF_H */
