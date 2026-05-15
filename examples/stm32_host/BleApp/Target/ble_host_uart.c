/**
  ******************************************************************************
  * @file    ble_host_uart.c
  * @author  GPM Application Team
  * @brief   Host bus interface porting layer - SDIO Transport Implementation
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "app_conf.h"
#include "app_common.h"
#include "ble_std.h"
#include "logging.h"
#include "ble_host_uart.h"
#include "app_ble.h"
#include "blestack.h"
#include "bt_hci.h"

#include "FreeRTOS.h"
/* SDIO Transport Implementation
 *
 * This file bridges the STM32 BLE Stack and the bt_hci SDIO transport layer.
 *
 * Architecture:
 *   STM32 BLE Stack -> BleHostSend() -> bt_hci_send() -> RCP/SDIO
 *   RCP/SDIO -> bt_hci callback -> BleMngr_BleSerialRxCb() -> STM32 BLE Stack
 *
 * Key points:
 *   - bt_hci delivers complete HCI packets (no byte-by-byte accumulation)
 *   - RX packets must be copied as BleMngr processes asynchronously
 *   - Memory is allocated with pvPortMalloc() and freed by BleMngr
 */

/* Private function prototypes -----------------------------------------------*/
static void hci_transport_rx_callback(const uint8_t *data, uint16_t len);
static ble_host_status BleHostUartEnable(void);

/* Private variables ---------------------------------------------------------*/
static volatile bool hci_rx_processing = false;  /* Prevent re-entrancy */

/* External function from BLE Manager */
extern void BleMngr_BleSerialRxCb(uint8_t *Buf, uint16_t Size);

/*==============================================================================
 * Private Functions
 *===========================================================================*/


/**
  * @brief HCI RX callback from bt_hci transport
  * @param data: Received HCI packet (H4 format: type + payload)
  * @param len: Packet length
  * @note This function is called from bt_hci context
  */
static void hci_transport_rx_callback(const uint8_t *data, uint16_t len)
{
  if (data == NULL || len == 0) {
    return;
  }

  /* Prevent re-entrancy */
  if (hci_rx_processing) {
    LogError("[BT_HCI_RX] Dropped packet (already processing)\n");
    return;
  }
  hci_rx_processing = true;

#if HCI_RAW_DUMP_ENABLE
  LogInfo("[BT_HCI_RX] Received %u bytes: type=0x%02X\n", len, data[0]);
  hci_log_hex_dump("[HCI RX RAW]", data, len);
#endif

  /* Allocate memory for the packet - BleMngr will free it */
  uint8_t *hci_rx_buffer = pvPortMalloc(len);
  if (hci_rx_buffer == NULL) {
    LogError("[BT_HCI_RX] Failed to allocate %u bytes\n", len);
    hci_rx_processing = false;
    return;
  }

  /* Copy the packet */
  memcpy(hci_rx_buffer, data, len);

  /* Forward complete HCI packet directly to BLE Manager
   * This bypasses ble_serial.c's byte-by-byte accumulation logic */
  BleMngr_BleSerialRxCb(hci_rx_buffer, len);

  hci_rx_processing = false;
}

static ble_host_status BleHostUartEnable(void)
{
  /* Transport is initialized in BleHostInit() */
  return BLE_HOST_STATUS_NO_ERR;
}

/*==============================================================================
 * Public Functions
 *===========================================================================*/

ble_host_status BleHostSend(const uint8_t *aBuf, uint16_t aBufLength)
{
  /* Standalone mode: no PC HOST connected, nothing to send */
  (void)aBuf;
  (void)aBufLength;
  return BLE_HOST_STATUS_NO_ERR;
}

void BleHostInit(void)
{
  /* Initialize HCI transport layer with RX callback */
  if (!bt_hci_init(hci_transport_rx_callback)) {
    LogError("HCI transport init failed");
    return;
  }

  LogInfo("BLE Host transport initialized over SDIO (bt_hci)");
  BleHostUartEnable();
}
