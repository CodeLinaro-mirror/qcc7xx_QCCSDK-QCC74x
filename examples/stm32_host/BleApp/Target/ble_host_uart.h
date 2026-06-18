/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_host_uart.h
  * @author  MCD Application Team
  * @brief   Host bus interface - SDIO transport (to be implemented)
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

#ifndef BLE_HOST_H__
#define BLE_HOST_H__

#include <stdio.h>
#include <stdint.h>

/* Set to 1 to enable HCI raw TX/RX hex dump */
#define HCI_RAW_DUMP_ENABLE 0

#if HCI_RAW_DUMP_ENABLE
static inline void hci_log_hex_dump(const char *label, const uint8_t *data, uint16_t len)
{
  char line[64];
  uint16_t i, j, pos, end;

  printf("%s [%u bytes]:\n", label, len);
  for (i = 0; i < len; i += 16) {
    pos = 0;
    end = (i + 16 < len) ? i + 16 : len;
    for (j = i; j < end; j++) {
      pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[j]);
    }
    line[pos] = '\0';
    printf("  %s\n", line);
  }
}
#endif /* HCI_RAW_DUMP_ENABLE */

typedef enum
{
  BLE_HOST_STATUS_NO_ERR = 0 ,
  BLE_HOST_STATUS_ERR = -1,
} ble_host_status;

void BleHostInit(void);
ble_host_status BleHostSend(const uint8_t *aBuf, uint16_t aBufLength);

#endif /* BLE_HOST_H__ */
