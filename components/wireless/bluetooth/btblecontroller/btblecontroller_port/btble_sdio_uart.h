/**
 * @file btble_sdio_uart.h
 * @brief SDIO adapter layer for BLE controller
 * @details Provides UART-like interface over SDIO for BLE controller
 */

#ifndef __BTBLE_SDIO_UART_H__
#define __BTBLE_SDIO_UART_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize SDIO UART adapter
 */
void btble_sdio_uart_init(void);

/**
 * @brief Get number of bytes available in receive buffer
 * @return Number of bytes available
 */
uint32_t btble_sdio_uart_get_rx_count(void);

/**
 * @brief Copy data to receive buffer (called from transporthci callback)
 * @param data Pointer to data
 * @param len Length of data
 */
void btble_sdio_rx_copy(uint8_t *data, uint32_t len);

/**
 * @brief Read data from receive buffer
 * @param data Buffer to read data into
 * @param len Maximum number of bytes to read
 * @return Number of bytes actually read
 */
uint32_t btble_sdio_uart_read(uint8_t *data, uint32_t len);

/**
 * @brief Write data to SDIO (sends to host)
 * @param data Pointer to data to send
 * @param len Length of data
 */
void btble_sdio_uart_write(uint8_t *data, uint32_t len);

/**
 * @brief RX event callback (called when data is received)
 * This is implemented in btblecontroller_port_uart.c
 */
void btble_sdio_uart_rx_event(void);

/**
 * @brief TX event callback (called when data is sent)
 * This is implemented in btblecontroller_port_uart.c
 */
void btble_sdio_uart_tx_event(void);

#endif /* __BTBLE_SDIO_UART_H__ */
