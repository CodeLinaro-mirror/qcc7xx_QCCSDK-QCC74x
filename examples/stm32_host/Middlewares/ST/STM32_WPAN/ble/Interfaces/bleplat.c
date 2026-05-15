/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bleplat.c
  * @author  MCD Application Team
  * @brief   This file implements the platform functions for BLE stack library.
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

#include "app_common.h"

#include "bleplat.h"
#include "blestack.h"
#include "stm32h7rsxx_ll_utils.h"
#include "app_ble.h"
#include "ble_timer.h"
#include "ble_nvm.h"
#include "ble_serial.h"

/* Mandatory to be included , don't remove ! */
#include "ble_wrap.c"


/*can be transferred later to stm32u5_ll_system.h */
uint32_t LL_FLASH_GetUDN(void);
uint32_t LL_FLASH_GetDeviceID(void);
uint32_t LL_FLASH_GetSTCompanyID(void);

/*****************************************************************************/

void BLEPLAT_Init()
{  
  /* Realize a restore */
  BleNvmRestore();
  /* stop all timers */
  BleTimer_Rest();
  /* reset serial bus */
  (void)BleSerial_Enable();
  /* USER CODE BEGIN APP_BLE_Init_Buffers */
  /* USER CODE END APP_BLE_Init_Buffers */
}

/*****************************************************************************/

void BLEPLAT_NvmStore( const uint64_t* ptr,
                              uint16_t size )
{
  UNUSED(ptr);
  UNUSED(size);
  BleNvmStore();
  return;
}

/*****************************************************************************/

uint8_t BLEPLAT_TimerStart( uint16_t id,
                            uint32_t timeout )
{
  return BleTimer_StartTimer( id, timeout );
}

/*****************************************************************************/

void BLEPLAT_TimerStop( uint16_t id )
{
  BleTimer_StopTimer( id );
}
/*****************************************************************************/

/**
  * @brief  Return the Unique Device Number
  * @retval Values between Min_Data=0x00000000 and Max_Data=0xFFFFFFFF
  */
uint32_t LL_FLASH_GetUDN(void)
{
  return (READ_REG(*((uint32_t *)UID_BASE_ADDRESS)));
}
 
/**
  * @brief  Return the Device ID
  *         For STM32U5xx devices, the device ID is 0x0A
  * @retval Values between Min_Data=0x00 and Max_Data=0xFF (ex: Device ID is 0x0A)
  */
uint32_t LL_FLASH_GetDeviceID(void)
{
  return ((READ_REG(*((uint32_t *)UID_BASE_ADDRESS + 1U))) & 0x000000FFU);
}

/**
  * @brief  Return the ST Company ID
  * @note   For STM32U5xxxx devices, the ST Company ID is 0x593150
  * @retval Values between Min_Data=0x00 and Max_Data=0xFFFFFF (ex: ST Company ID is 0x593150)
  */
uint32_t LL_FLASH_GetSTCompanyID(void)
{
  return (((READ_REG(*((uint32_t *)UID_BASE_ADDRESS + 1U))) >> 8U) & 0x00FFFFFFU);
}
