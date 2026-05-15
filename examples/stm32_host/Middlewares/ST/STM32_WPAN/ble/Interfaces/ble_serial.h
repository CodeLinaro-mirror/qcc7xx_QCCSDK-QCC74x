/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_serial.h
  * @author  MCD Application Team
  * @brief   This header defines the SREIAL BUS functions used by the BLE stack
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

#ifndef BLE_SERIAL_H__
#define BLE_SERIAL_H__

#include "ble_defs.h"

#define BLE_SERIAL_BUFFER_SIZE                  (HCI_ISODATA_PKT_MAX_SIZE) /* MAX_HCI_PCKT_SIZE*/
#define BLE_SERIAL_BUFFER_NB                    (10)

/**
 * @brief
 *   BLE Serial Operation Status Enumeration
 *
 * @details
 *   This enumeration defines the possible return status values for BLE serial interface operations.
 *   It is used to indicate the success or failure of functions related to BLE serial communication,
 *   such as data transmission, reception, or buffer management.
 *
 *   Values:
 *     - BLE_SERIAL_STATUS_NO_ERR (0):
 *         The operation completed successfully with no errors.
 *
 *     - BLE_SERIAL_STATUS_ERR (-1):
 *         The operation failed due to an error (e.g., invalid parameters, buffer overflow, hardware failure).
 *
 *   Usage:
 *     - Functions that perform BLE serial operations return a value of this type to indicate their result.
 *     - The calling code should check the return value and handle errors appropriately.
 */

typedef enum 
{
  BLE_SERIAL_STATUS_NO_ERR = 0 ,
  BLE_SERIAL_STATUS_ERR = -1
} BleSerial_Status;

/**
 * @brief
 *   Initialize the BLE Serial Communication Module
 *
 * @details
 *   This function performs the complete initialization of the BLE serial communication subsystem.
 *   It registers callback functions for transmit, receive, and error events.
 *   It prepares the context structure, creates the necessary FreeRTOS resources, and enables the
 *   BLE serial interface for communication with the BleController.
 *
 * @note
 *   - This function should be called once during system startup before any BLE serial communication occurs.
 *   - Assumes that  Serial Inteface and DMA hardware are already configured by the CubeMX-generated code.
 */

void BleSerial_Init(void);

/**
 * @brief
 *   Enable the BLE Serial Interface to BleController
 *
 * @details
 *   This function sets up the BLE serial communication interface, preparing it for operation. 
 *   It initializes internal state and queues.
 *
 * @note
 *   - This function should be called during system initialization/Reset before any BLE serial communication occurs.
 *   - Assumes that  Serial Inteface and hardware are already configured.
 */

void BleSerial_Enable(void);

/**
 * @brief
 *   BLE Serial Transmit Complete Callback
 *
 * @details
 *   This function is called by the Lower Layers ( Serial Bus Driver/ HAL ) when a transmit (TX) operation for BLE serial data completes. 
 *   It is invoked after the entire data buffer requested for transmission has been sent—**not after each byte, 
 *   but after the whole buffer**.
 *  
 * @param 
 *   void
 *
 * @return
 *   void
 *
 * @note
 *   - This callback is typically registered with the Lower Layers ( Serial Bus Driver/ HAL ) and is invoked automatically when a transmit
 *     operation completes.
 *   - It is essential for enabling non-blocking, event-driven serial communication.
 */

void bleSerial_TxCpltCb ( void );

/**
 * @brief
 *   BLE Serial Receive Complete Callback
 *
 * @details
 *   This function is called by the Lower Layers ( Serial Bus Driver/ HAL ) once receive operation
 *   for BLE serial data completes. It manages the assembly of received data into HCI (Host Controller Interface)
 *   packets, updates the receive queue (BleRxQueue), and notifies the BleSerial task for further processing.
 *
 * @param size
 *   The number of bytes received in the latest transfer.
 *
 * @return
 *   void
 *
 * @note
 *   - This callback is registered with the Lower Layers ( Serial Bus Driver/ HAL ) and is invoked automatically when a receive completes.
 */
void bleSerial_RxCpltCb( uint16_t size );

/**
 * @brief
 *   BLE Serial Error Callback
 *
 * @details
 *   This function is called by the Lower Layers ( Serial Bus Driver/ HAL ) once Error in Serial Bus Occurs . 
 *  
 * @param 
 *   void
 *
 * @return
 *   void
 *
 * @note
 *   - This callback is registered with the Lower Layers ( Serial Bus Driver/ HAL ) and is invoked automatically when an Error Occurs .
 */
void bleSerial_ErrCb( void );


/**
 * @brief
 *   Queue Data for Transmission over the BLE Controller's Serial Interface
 *
 * @details
 *   This function prepares and queues a buffer of data for transmission over the BLE serial interface.
 *   It uses a ring buffer (BleTxRingBuffer) to manage outgoing data, ensuring that transmissions are handled
 *   in a non-blocking, event-driven manner. The function performs input validation, manages buffer
 *   availability, and posts a transmit request event to the BLE serial transmit finite state machine (FSM).
 *
 *   - Validates input parameters:
 *       - Checks that the buffer pointer (aBuf) is not NULL and the buffer length (aBufLength) is greater than zero.
 *       - If validation fails, returns an error status (BLE_SERIAL_STATUS_ERR).
 *
 *   - If parameters are valid:
 *       - Checks if the next slot in the TX ring buffer is free (i.e., not holding untransmitted data).
 *           - If the buffer is still occupied, logs an overflow error and return BLE_SERIAL_STATUS_NO_ERR .
 *           - If the buffer is free:
 *               - Copies the data to the TX ring buffer.
 *               - Updates the buffer size to reflect the new data.
 *               - Advances the ring buffer index using the UPDATE_BLE_TX_RING_BUFFER_AFTER_PUSH macro.
 *               - Posts a transmit request event (BLE_SERIAL_EVT_TX_REQ) to the BLE serial transmit FSM
 *                 using BleSerialTask_TxEvtInd, which will handle the actual transmission asynchronously.
 *       - Returns a success status (BLE_SERIAL_STATUS_NO_ERR) if the data was queued successfully.
 *
 *   Usage:
 *     - This function is called by BLE stack or application code to send data
 *       over the BLE serial interface in a non-blocking manner.
 *     - The actual transmission is handled asynchronously by the BLE serial transmit FSM.
 *
 * @param aBuf
 *   Pointer to the data buffer to be transmitted.
 *
 * @param aBufLength
 *   Length of the data buffer in bytes.
 *
 * @return
 *     - BLE_SERIAL_STATUS_NO_ERR: Data was successfully queued for transmission.
 *     - BLE_SERIAL_STATUS_ERR:    Invalid parameters or TX buffer overflow.
 *
 * @note
 *   - The function relies on a ring buffer (BleTxRingBuffer) for managing outgoing data.
 *   - The BLE serial transmit FSM is responsible for dequeuing and transmitting the data.
 *   - Proper buffer management and event posting are essential for reliable, non-blocking operation.
 */

BleSerial_Status BleSerialTransmit(uint8_t *aBuf, uint16_t aBufLength);

#endif /* BLE_SERIAL_H__ */