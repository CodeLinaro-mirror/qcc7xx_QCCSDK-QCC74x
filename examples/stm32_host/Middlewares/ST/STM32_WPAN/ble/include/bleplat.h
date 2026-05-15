/*****************************************************************************
 * @file    bleplat.h
 *
 * @brief   This file contains the interface of the BLE platform layer
 *          (lower interface of the BLE stack library).
 *          It is included by the STM32WBX BLE stack library.
 *****************************************************************************
 * @attention
 *
 * Copyright (c) 2018-2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 *****************************************************************************
 */

#ifndef BLEPLAT_H__
#define BLEPLAT_H__


#include <stdint.h>

/* Enumerated values used for the return of the functions:
 */
enum
{
  BLEPLAT_OK    =  0, /* The function did the job and returns an OK status */
  BLEPLAT_FULL  = -1, /* The function exits because the HW resource is full */
  BLEPLAT_BUSY  = -2, /* The function is busy and is not available for the
                         requested operation */
  BLEPLAT_EOF   = -3, /* The function exits and notifies the HW resource
                         (memory for example) reached the end */
  BLEPLAT_ERROR = -5  /* The function exits due to some issue (memory
                         corruption or buffer overflow for example) */
};

/* General functions:
 */

/**
 * @brief  This function is called by the BLE stack when it is initialized
 *         or reset (via hci_reset). The user shall call here the functions
 *         to reset the Timer and AES needed for the BLE stack.
 * @param  None
 * @retval None
 */
extern void BLEPLAT_Init( void );

/* Non Volatile Memory (NVM) interface:
 */

/**
 * @brief  Store data in the NVM.
 *         This functions indicates the portion of NVM cache buffer that needs
 *         to be written in NVM.
 * @param  ptr: Pointer to the start of the data portion to be stored
 *              (it points inside the NVM cache buffer)
 * @param  size: Size of valid data in NVM cache buffer (in 64-bit words)
 *               Note: valid data always starts at the beginning of the NVM
 *               cache buffer.
 * @retval None
 */
extern void BLEPLAT_NvmStore( const uint64_t* ptr,
                              uint16_t size );

/* Timer interface:
 */

/**
  * @brief  Start the Timer.
  * @param  id: The timer ID to be started
  * @param  timeout: The timeout needed to stop the timer (in ms)
  * @retval status (BLEPLAT_XX)
  */
extern uint8_t BLEPLAT_TimerStart( uint16_t id,
                                   uint32_t timeout );

/**
  * @brief  Stop the Timer
  * @param  id: The timer ID to be stopped
  * @retval None
  */
extern void BLEPLAT_TimerStop( uint16_t id );

/**
  * @brief  The timer callback function called when the timeout of a given
  *         timer has elapsed
  * @param  id: The timer ID
  * @retval None
  */
extern void BLEPLATCB_TimerExpiry( uint16_t id );


#endif /* BLEPLAT_H__ */
