/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_ble.h
  * @author  MCD Application Team
  * @brief   Header for ble application
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
#ifndef APP_BLE_H
#define APP_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "app_common.h"
#include "app_conf.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
  
/* Definitions for queue lengths and message sizes */
#define BLE_TM_MQ_RX_FROM_HOST_LEN             (20)
#define BLE_TM_MQ_RX_FROM_BLESTACK_LEN         (20)
    
/* Worest case Events Number to be Recevied over all MQs */
#define BLE_TM_MQ_SET_EVTS_SIZE                (BLE_TM_MQ_RX_FROM_HOST_LEN + \
                                                BLE_TM_MQ_RX_FROM_BLESTACK_LEN)
/* BLE application task configuration */
#define BLE_APP_TASK_STACK_SIZE                (8128)
#define BLE_APP_TASK_PRIO                      ( BLE_MNGR_PRIO - 2 )

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void BleAppNewHostPcktInd( const uint8_t* data, uint16_t length);
void BleAppDeInit( void );
void BleAppInit(void);
uint8_t BleApp_isCompleteHciPackt ( uint8_t* RxBuffer , uint16_t size );
void TM_SysLocalCmd (uint8_t *data);

bool BleAppAciInit(void);
bool BleAppAdvStart(uint8_t adv_type, uint16_t interval_min, uint16_t interval_max,
                    const uint8_t *dev_name, uint8_t dev_name_len);
bool BleAppAdvStop(void);
bool BleAppAddService(void);
bool BleAppGetLastConnectionHandle(uint16_t *connection_handle);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*APP_BLE_H */
