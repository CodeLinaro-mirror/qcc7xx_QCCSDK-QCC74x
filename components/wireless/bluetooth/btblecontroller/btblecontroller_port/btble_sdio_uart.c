/**
 * @file btble_sdio_uart.c
 * @brief SDIO adapter layer for BLE controller
 * @details Provides UART-like interface over SDIO for BLE controller
 */

#include "btble_sdio_uart.h"
#include "ring_buffer.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations - transporthci will provide these */
extern int btble_sdio_hci_write(uint8_t *buf, uint32_t len);

/* Ring buffer for received data */
static Ring_Buffer_Type sdioRB;
static uint8_t sdioBuf[1024];

/*****************************************************************************
 * Public API Implementation
 *****************************************************************************/

/**
 * @brief Initialize SDIO UART adapter
 */
void btble_sdio_uart_init(void)
{
    Ring_Buffer_Init(&sdioRB, sdioBuf, sizeof(sdioBuf), NULL, NULL);
}

/**
 * @brief Get number of bytes available in receive buffer
 */
uint32_t btble_sdio_uart_get_rx_count(void)
{
    uint32_t len = Ring_Buffer_Get_Length(&sdioRB);
    return len;
}

/**
 * @brief Copy data to receive buffer (called from transporthci callback)
 */
void btble_sdio_rx_copy(uint8_t *data, uint32_t len)
{
    Ring_Buffer_Write(&sdioRB, data, len);
}

/**
 * @brief Read data from receive buffer
 */
uint32_t btble_sdio_uart_read(uint8_t *data, uint32_t len)
{
    uint32_t cnt;

    cnt = Ring_Buffer_Get_Length(&sdioRB);

    if(cnt < len) {
        len = cnt;
    }

    if (len > 0) {
        Ring_Buffer_Read(&sdioRB, data, len);
        return len;
    }

    return 0;
}

/**
 * @brief Write data to SDIO (sends to host)
 */
void btble_sdio_uart_write(uint8_t *data, uint32_t len)
{
    btble_sdio_hci_write(data, len);
    btble_sdio_uart_tx_event();
}
