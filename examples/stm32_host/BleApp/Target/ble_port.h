/**
  ******************************************************************************
  * @file    ble_port.h
  * @author  MCD Application Team
  * @brief   Header for ble_port.c - Hardware Abstraction Layer for SDIO
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef BLE_PORT_H
#define BLE_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Disable logic analyzer debug for now */
#define BLE_LOGIC_ANALYIZER_DEBUG    (0)

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported functions prototypes ---------------------------------------------*/

/* Debug hooks - for logic analyzer tracing */
void BlePort_DebugInit                                      ( void );
void BlePort_DebugBleStackProcessEntryHook                  ( void );
void BlePort_DebugBleStackProcessExitHook                   ( void );
void BlePort_DebugBleStackRequestEntryHook                  ( void );
void BlePort_DebugBleStackRequestExitHook                   ( void );
void BlePort_DebugBleStackInitEntryHook                     ( void );
void BlePort_DebugBleStackInitExitHook                      ( void );
void BlePort_DebugBlePlatCbTimerExpiryEntryHook             ( void );
void BlePort_DebugBlePlatCbTimerExpiryExitHook              ( void );
void BlePort_DebugBleStackIndicationFromControllerEntryHook ( void );
void BlePort_DebugBleStackIndicationFromControllerExitHook  ( void );
void BlePort_DebugBleCBRequestToControllerEntryHook         ( void );
void BlePort_DebugBleCBRequestToControllerExitHook          ( void );
void BlePort_DebugBleCbIndicationEntry                      ( void );
void BlePort_DebugBleCbIndicationExit                       ( void );

/* Serial interface - for HCI data transmission */
void BlePort_SerialInit                                     ( void );
void BlePort_SerialAbortRx                                  ( void );
void BlePort_SerialRxUntilIdle                              ( uint8_t* Buf, uint16_t MaxSize );
void BlePort_SerialRx                                       ( uint8_t* Buf, uint16_t Size );
void BlePort_SerialTx                                       ( uint8_t* Buf, uint16_t Size );

#ifdef __cplusplus
}
#endif

#endif /* BLE_PORT_H */
