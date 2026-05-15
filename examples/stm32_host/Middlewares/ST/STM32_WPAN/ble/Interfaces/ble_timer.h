/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_timer.h
  * @author  MCD Application Team
  * @brief   This header defines the timer functions used by the BLE stack
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

#ifndef BLE_TIMER_H__
#define BLE_TIMER_H__
/* ---------------------------------------------------------------------------
 * @brief Reset all BLE timers.
 * @details
 *   Stops and deletes all active timers, and clears the timers list.
 *   Typically called during BLE de-initialization or system reset.
 * --------------------------------------------------------------------------*/
void BleTimer_Rest(void);

/* ---------------------------------------------------------------------------
 * @brief Start a BLE timer with a given ID ( Passed by BleStack ) and timeout.
 * @details
 *   If a timer with the same ID already exists, it is stopped first.
 *   A new timer is created and started. If no free timer slots are available,
 *   an error is logged and BLE_STATUS_INSUFFICIENT_RESOURCES is returned.
 * @param id      The unique ID for the timer , passed by BleStack .
 * @param timeout Timeout in milliseconds.
 * @return BLE_STATUS_SUCCESS on success, error code otherwise.
 * --------------------------------------------------------------------------*/
uint8_t BleTimer_StartTimer( uint16_t id , uint32_t ms_timeout );

/* ---------------------------------------------------------------------------
 * @brief Stop and delete a BLE timer by its ID.
 * @details
 *   If the timer with the given ID exists, it is stopped and deleted, and its
 *   slot in the timers list is marked as free.
 * @param id The ID of the timer to stop.
 * --------------------------------------------------------------------------*/
void BleTimer_StopTimer( uint16_t id );

#endif /* BLE_TIMER_H__ */
