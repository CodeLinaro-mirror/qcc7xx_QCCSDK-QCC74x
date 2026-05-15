/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_port_template.h
  * @author  MCD Application Team
  * @brief   Header for ble_port.c
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
#ifndef BLE_PORT_H
#define BLE_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_LOGIC_ANALYIZER_DEBUG    (1)
  
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
  
/* Exported constants --------------------------------------------------------*/

/* External variables --------------------------------------------------------*/
  
/* Exported macro ------------------------------------------------------------*/

#include "stm32u5xx_hal.h"

extern UART_HandleTypeDef                                               huart2;
extern DMA_HandleTypeDef                                                handle_GPDMA1_Channel2;
extern DMA_HandleTypeDef                                                handle_GPDMA1_Channel3;

#define BLE_RCP_HW_PERIPHERAL_IRQ                                       LPUART1_IRQn
#define BLE_RCP_HANDLER_TYPE_DEF                                        UART_HandleTypeDef
#define BLE_RCP_DMA_CHANNEL_HANDLER_TYPE_DEF                            DMA_HandleTypeDef
#define BLE_RCP_HW_PERIPHERAL_HANDLER                                   huart2
#define BLE_RCP_DMA_RX_CHANNEL_HANDLER                                  handle_GPDMA1_Channel2
#define BLE_RCP_DMA_TX_CHANNEL_HANDLER                                  handle_GPDMA1_Channel3

#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )

/* GPIO PIN to Track BleStack_IndicationFromController */
#define LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PIN                              GPIO_PIN_15   /* D2 Pin in ARDUINO -included Zio connector (CN10) pinout */
#define LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PORT                             GPIOF
#define __HAL_RCC_GPIO_BLE_DEBUG_CNTRL_IND_PORT_CLK_ENABLE()            __HAL_RCC_GPIOF_CLK_ENABLE();

/* GPIO PIN to Track BleStack_Request */
#define LOGIC_DEBUG_BLE_REQ_IND_GPIO_PIN                                GPIO_PIN_13   /* D3 Pin in ARDUINO -included Zio connector (CN10) pinout */
#define LOGIC_DEBUG_BLE_REQ_IND_GPIO_PORT                               GPIOE
#define __HAL_RCC_GPIO_BLE_DEBUG_REQ_PORT_CLK_ENABLE()                  __HAL_RCC_GPIOE_CLK_ENABLE();

/* GPIO PIN to Track BleStack_Process */
#define LOGIC_DEBUG_BLE_PROC_GPIO_PIN                                   GPIO_PIN_14    /* D4 Pin in ARDUINO -included Zio connector (CN10) pinout */
#define LOGIC_DEBUG_BLE_PROC_GPIO_PORT                                  GPIOF
#define __HAL_RCC_GPIO_BLE_DEBUG_PROC_PORT_CLK_ENABLE()                 __HAL_RCC_GPIOF_CLK_ENABLE();

/* GPIO PIN to Track BLECB_RequestToController */
#define LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PIN                         GPIO_PIN_11    /* D5 Pin in ARDUINO -included Zio connector (CN10) pinout */
#define LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PORT                        GPIOE
#define __HAL_RCC_GPIO_BLE_DEBUG_BLECB_REQ_TO_CNTRL_PORT_CLK_ENABLE()   __HAL_RCC_GPIOE_CLK_ENABLE();

/* GPIO PIN to Track BLECB_Indication */
#define LOGIC_DEBUG_BLECB_IND_GPIO_PIN                                  GPIO_PIN_9    /* D6 Pin in ARDUINO -included Zio connector (CN10) pinout */
#define LOGIC_DEBUG_BLECB_IND_GPIO_PORT                                 GPIOE
#define __HAL_RCC_GPIO_BLE_DEBUG_BLECB_IND_PORT_CLK_ENABLE()            __HAL_RCC_GPIOE_CLK_ENABLE();

#endif
  
/* Exported functions prototypes ---------------------------------------------*/

/* Initializes GPIO pins for BLE logic analyzer debug signals.*/
void BlePort_DebugInit                                      ( void );
/* Initializes the BLE serial port interface and registers callbacks. */
void BlePort_SerialInit                                     ( void );
/* Aborts any ongoing BLE serial receive operation. */
void BlePort_SerialAbortRx                                  ( void );
/* Receives data over BLE serial until an idle condition is detected. */
void BlePort_SerialRxUntilIdle                              ( uint8_t* Buf,uint16_t MaxSize );
/* Receives a specified amount of data over BLE serial */
void BlePort_SerialRx                                       ( uint8_t* Buf,uint16_t Size );
/* Transmits data over BLE serial */
void BlePort_SerialTx                                       ( uint8_t* Buf,uint16_t Size );
/* called on BleStack_Process Entry */
void BlePort_DebugBleStackProcessEntryHook                  ( void );
/* called on BleStack_Process Exit */
void BlePort_DebugBleStackProcessExitHook                   ( void );
/* called on BleStack_Request Entry */
void BlePort_DebugBleStackRequestEntryHook                  ( void );
/* called on BleStack_Request Exit */
void BlePort_DebugBleStackRequestExitHook                   ( void );
/* called on BleStack_Init Entry */
void BlePort_DebugBleStackInitEntryHook                     ( void );
/* called on BleStack_Init Exit */
void BlePort_DebugBleStackInitExitHook                      ( void );
/* called on BLEPLATCB_TimerExpiry Entry */
void BlePort_DebugBlePlatCbTimerExpiryEntryHook             ( void );
/* called on BLEPLATCB_TimerExpiry Exit */
void BlePort_DebugBlePlatCbTimerExpiryExitHook              ( void );
/* called on BleStack_IndicationFromController Entry */
void BlePort_DebugBleStackIndicationFromControllerEntryHook ( void );
/* called on BleStack_IndicationFromController Exit */
void BlePort_DebugBleStackIndicationFromControllerExitHook  ( void );
/* called on BLECB_RequestToController Entry */
void BlePort_DebugBleCBRequestToControllerEntryHook         ( void );
/* called on BLECB_RequestToController Exit */
void BlePort_DebugBleCBRequestToControllerExitHook          ( void );
/* called on BLECB_Indication Entry */
void BlePort_DebugBleCbIndicationEntry                      ( void );
/* called on BLECB_Indication Exit */
void BlePort_DebugBleCbIndicationExit                       ( void );

#ifdef __cplusplus
}
#endif

#endif /* BLE_PORT_H */
