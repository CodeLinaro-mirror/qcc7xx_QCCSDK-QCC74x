/**
  ******************************************************************************
  * @file    ble_port.c
  * @author  MCD Application Team
  * @brief   BLE Subsystem Hardware Abstraction Layer - SDIO Transport
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

/* Includes ------------------------------------------------------------------*/
#include "ble_port.h"
#include "ble_host_uart.h"
#include "ble_serial.h"
#include "bt_hci.h"
#include <stdio.h>

/* SDIO Transport Implementation
 *
 * This file provides the hardware abstraction layer between ST BLE Stack
 * and the SDIO transport layer (bt_hci).
 *
 * Architecture:
 *   STM32 BLE Stack -> ble_serial.c -> ble_port.c -> bt_hci.c -> RCP/SDIO
 *
 * The bt_hci layer handles:
 *   - Complete HCI packet delivery (no byte-by-byte accumulation needed)
 *   - Asynchronous callbacks for received packets
 *   - Thread-safe buffer management
 */

/*===============================================================================================================================================*/
/*------------------------------------------------------------------------------------- BLE PORT , DEBUG  ---------------------------------------------------------------------------------------*/
/*===============================================================================================================================================*/

void BlePort_DebugInit( void )
{
  /* Debug GPIO init - not needed for SDIO */
}

void BlePort_DebugBleStackProcessEntryHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackProcessExitHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackRequestEntryHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackRequestExitHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackIndicationFromControllerEntryHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackIndicationFromControllerExitHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleCBRequestToControllerEntryHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleCBRequestToControllerExitHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackInitEntryHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleStackInitExitHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBlePlatCbTimerExpiryEntryHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBlePlatCbTimerExpiryExitHook( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleCbIndicationEntry( void )
{
  /* Debug hook - not used */
}

void BlePort_DebugBleCbIndicationExit( void )
{
  /* Debug hook - not used */
}

/*===============================================================================================================================================*/
/*------------------------------------------------------------------------------------- BLE PORT , SERIAL  ---------------------------------------------------------------------------------------*/
/*===============================================================================================================================================*/

/**
  * @brief  Initializes the SDIO interface for BLE communication
  * @note   SDIO is initialized in ble_host_uart.c via BleHostInit()
  *         This is a placeholder as the stack expects this function
  */
void BlePort_SerialInit( void )
{
  /* SDIO init is handled by BleHostInit() in ble_host_uart.c
   * which calls bt_hci_init() with appropriate callbacks
   */
}

/**
  * @brief  Transmits data over SDIO
  * @param  Buf: Pointer to data buffer (HCI H4 packet: type + payload)
  * @param  Size: Number of bytes to transmit
  */
void BlePort_SerialTx( uint8_t* Buf, uint16_t Size )
{
  if (!Buf || Size == 0) {
    return;
  }

  /* Log outgoing HCI packets for debugging */
  const char* type_str = "UNKNOWN";
  if (Size >= 1) {
    switch (Buf[0]) {
      case 0x01: type_str = "CMD"; break;
      case 0x02: type_str = "ACL"; break;
      case 0x03: type_str = "SCO"; break;
      case 0x04: type_str = "EVT"; break;
      default: type_str = "UNKNOWN"; break;
    }
  }

#if HCI_RAW_DUMP_ENABLE
  printf("[BlePort_Tx] type=0x%02X (%s), len=%u\n", Buf[0], type_str, Size);
  hci_log_hex_dump("[HCI TX RAW]", Buf, Size);
#endif

  /* Send HCI packet via bt_hci transport */
  if (!bt_hci_send(Buf, Size)) {
    printf("[BlePort_Tx] FAILED: bt_hci_send error\n");
  }
}

/**
  * @brief  Receives data over SDIO
  * @param  Buf: Pointer to buffer for received data
  * @param  Size: Number of bytes to receive
  * @note   Not used - bt_hci delivers complete packets via callback
  */
void BlePort_SerialRx( uint8_t* Buf, uint16_t Size )
{
  /* RX is handled by bt_hci callback - not used */
  (void)Buf;
  (void)Size;
}

/**
  * @brief  Receives data over SDIO until idle condition
  * @param  Buf: Pointer to buffer for received data
  * @param  MaxSize: Maximum number of bytes to receive
  * @note   Not used - bt_hci delivers complete packets via callback
  */
void BlePort_SerialRxUntilIdle( uint8_t* Buf, uint16_t MaxSize )
{
  /* RX is handled by bt_hci callback - not used
   * The ST BLE stack expects this function, but bt_hci delivers
   * complete HCI packets asynchronously via callback
   */
  (void)Buf;
  (void)MaxSize;
}

/**
  * @brief  Aborts any ongoing SDIO receive operation
  * @note   Not implemented for bt_hci transport
  */
void BlePort_SerialAbortRx( void )
{
  /* bt_hci manages RX internally - abort not needed */
}
