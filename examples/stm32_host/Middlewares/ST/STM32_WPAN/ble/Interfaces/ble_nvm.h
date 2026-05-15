/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_nvm.h
  * @author  MCD Application Team
  * @brief   This header defines the NVM functions used by the BLE stack
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

#ifndef BLE_NVM_H__
#define BLE_NVM_H__

#define BLE_NVM_BUF_REGISTER(...)
#define BLE_NVM_RESTORE(...)
#define BLE_NVM_WRITE(...)

/**
 * @brief
 *   Initialize the Non-Volatile Memory (NVM) Buffer for the BLE Stack
 *
 * @details
 *   This function prepares a RAM buffer to be used as the NVM storage area for the BLE host stack.
 *   It invalidates the buffer content to ensure a clean state, registers the buffer with the BLE stack,
 *   and then attempts to restore any previously saved NVM data from persistent storage.
 *
 *   Steps:
 *     1. Register the NVM buffer with the BLE stack using the BLE_NVM_BUF_REGISTER macro/function,
 *        providing the buffer index, pointer, and size.
 *     2. Attempt to restore the NVM content for the BLE stack using the BLE_NVM_RESTORE macro/function,
 *        which loads any previously saved data into the buffer.
 *
 *   Usage:
 *     - This function should be called during BLE stack initialization, before any BLE operations that
 *       require access to persistent data (such as bonding information, keys, or configuration).
 *     - The buffer must be large enough to hold all required NVM data for the BLE stack.
 *
 * @param stack_nvm_buffer
 *   Pointer to the RAM buffer to be used for BLE NVM storage.
 *
 * @param size
 *   Size of the NVM buffer in bytes.
 *
 * @return
 *   void
 *
 * @note
 *   - The buffer should be retained across resets if persistent storage is required.
 */

void BleNvm_Init( void * stack_nvm_buffer , uint32_t size );


/**
 * @brief
 *   Restore BLE NVM Data from Persistent Memory
 *
 * @details
 *   This function initiates the process of restoring the BLE stack's non-volatile memory (NVM) buffer
 *   from persistent storage. It calls the BLE_NVM_RESTORE function or macro, passing the buffer index
 *   to load any previously saved NVM data into the RAM buffer used by the BLE stack.
 *
 *   - This operation is performed during BLE stack initialization to ensure that
 *     persistent data (such as bonding information, keys, or configuration) is available to the stack.
 *   - The actual restore mechanism is managed by the NVM manager.
 *
 *   Usage:
 *     - this function is called during system or BLE stack initialization, before any BLE operations
 *       that require access to persistent data.
 *
 * @return
 *   void
 *
 */

void BleNvmRestore( void );


/**
 * @brief
 *   Initiate Storage of BLE NVM Data to Persistent Memory
 *
 * @details
 *   This function starts the process of writing the BLE stack's non-volatile memory (NVM) buffer
 *   to persistent storage. It invokes the BLE_NVM_WRITE function or macro, passing the buffer index
 *   and a callback function (BleNvmCb) to handle completion or retry logic.
 *
 *   - The write operation is typically asynchronous; upon completion, the provided callback
 *     (BleNvmCb) will be called to check the result and potentially retry if needed.
 *   - This function is used to ensure that any changes to the BLE stack's persistent data
 *     (such as bonding information, keys, or configuration) are saved to non-volatile memory.
 *
 *   Usage:
 *     - this function will be called whenever the BLE stack's NVM data needs to be saved, such as after
 *       pairing, bonding, or configuration changes.
 *     - The actual write and callback mechanism is managed by the NVM manager.
 *
 * @return
 *   void
 *
 */

void BleNvmStore(void);

#endif /* BLE_NVM_H__ */
