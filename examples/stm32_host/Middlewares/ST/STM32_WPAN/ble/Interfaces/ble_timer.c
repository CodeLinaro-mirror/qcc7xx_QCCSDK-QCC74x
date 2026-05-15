/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    ble_timer.c
 * @author  MCD Application Team
 * @brief   BLE Timer Management Module
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
 *
 * @section Overview & Software Dynamics
 *
 *   This file implements the **BLE Timer Management** subsystem, providing a dynamic,
 *   event-driven timer service for BLE stack needs, built on top of FreeRTOS.
 *
 *   **Big Picture:**
 *   - The BLE timer module allows the BLE stack to create, start, stop, and reset
 *     multiple software timers, each identified by a unique ID.
 *   - Timers are managed in a fixed-size array, with each timer mapped to a FreeRTOS software timer.
 *   - The module abstracts timer allocation, freeing, and expiry notification, ensuring robust
 *     and efficient timer management for BLE operations.
 *   - When a timer expires, the module notifies the BLE Manager (BleMngr) via an event, enabling
 *     seamless integration with the BLE stack's event-driven architecture.
 *
 *   **Software Dynamics & Interactions:**
 *
 *   - **Timer Creation/Start:** When the BLE stack or application requests a timer, the module allocates
 *     a free slot, creates a FreeRTOS timer, and starts it with the specified timeout.
 *
 *   - **Timer Expiry:** When a timer expires, the FreeRTOS callback (`BleTimerCb`) is invoked, which
 *     notifies the BLE Manager of the expiry event and cleans up the timer slot.
 *
 *   - **Timer Stop/Delete:** Timers can be stopped and deleted by ID at any time, freeing their slot
 *     for future use.
 *
 *   - **Timer Reset:** All timers can be reset and cleaned up.
 *
 *   - **Resource Management:** The module ensures that no more than a fixed maximum number of timers
 *     are active at any time, and provides error handling for resource exhaustion.
 *
 *   - **Integration:** The timer module is tightly integrated with the BLE Manager, which handles
 *     timer expiry events and coordinates with the BLE stack and application.
 *
 *   **Key Components:**
 *     - **BleTimer_t:** Structure representing a single timer instance (FreeRTOS handle + ID).
 *     - **timersList:** Array holding all managed timers.
 *     - **Timer Management Functions:** For starting, stopping, and resetting timers by ID.
 *     - **Callback Handler:** For processing timer expiry and notifying the BLE Manager.
 *     - **Resource Checks:** Ensures safe allocation and cleanup of timer resources.
 *
 *   **How to Navigate:**
 *     - Start with the `BleTimer_t` structure and `timersList` array to understand timer storage.
 *     - Review the timer management functions (`BleTimer_StartTimer`, `BleTimer_StopTimer`, `BleTimer_Rest`)
 *       for allocation, usage, and cleanup logic.
 *     - Examine the callback (`BleTimerCb`) to see how timer expiry is handled and how the BLE Manager is notified.
 *     - All public APIs and key internal functions are documented with detailed comments.
 *
 *   **Typical Flow:**
 *     1. BLE stack requests a timer via `BleTimer_StartTimer`.
 *     2. Timer is allocated, started, and tracked in `timersList`.
 *     3. On expiry, `BleTimerCb` is called, which notifies the BLE Manager and cleans up the timer.
 *     4. Timers can be stopped or reset at any time, freeing resources for new timers.
 *     5. Errors (e.g., no free timers) are logged and handled gracefully.
 *
 *   This architecture enables reliable, scalable, and efficient timer management for BLE operations,
 *   supporting both stack and application needs in a real-time, event-driven system.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "logging.h"

/* BleStack Inlcude Files */
#include "blestack.h"
#include "ble_defs.h"

/* Ble Application Inlcude File */
#include "app_ble.h"

/* BleMngr And Interfaces Inlcude Files */
#include "ble_mngr.h"
#include "ble_port.h"
#include "ble_serial.h"
#include "ble_nvm.h"
#include "ble_timer.h"

/* FreeRTOS Include Files */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"

/* Private typedef -----------------------------------------------------------*/

/**
 * @brief
 *   Compile-time check for FreeRTOS Software Timers support
 */
#ifndef configUSE_TIMERS
#error "configUSE_TIMERS has to be defined in FreeRTOSConfig.h and equal 1 to compile this file."
#else
#if(configUSE_TIMERS == 0) 
#error "configUSE_TIMERS has to be defined in FreeRTOSConfig.h and equal 1 to compile this file."
#endif 
#endif

/**
 * @brief
 *   Structure representing a BLE timer instance.
 *
 * @details
 *   Each timer is associated with a FreeRTOS TimerHandle and a unique 16-bit ID.
 *   The timerHandle is used to control the timer, and the id is used to identify
 *   and manage timers in the timersList array. id is passed by BleStack
 */
typedef struct
{
    TimerHandle_t       timerHandle;  /**< FreeRTOS timer object handle */
    uint16_t            id;           /**< Unique identifier for the timer , passed by BleStack */
} BleTimer_t;

/* Private defines -----------------------------------------------------------*/

/**
 * @brief Maximum number of BLE timers that can be managed simultaneously.
 */
#define BLE_TIMERS_MAX_TIMERS_COUNT (20)

/**
 * @brief Return value indicating that a timer was not found in the timers list.
 */
#define BLE_TIMER_NOT_FOUND         (-1)

/* Private variables ---------------------------------------------------------*/

/**
 * @brief Array holding all managed BLE timers.
 *
 * @details
 *   Each entry in this array represents a timer slot. If timerHandle is NULL,
 *   the slot is free. Otherwise, it is in use.
 */
BleTimer_t timersList[BLE_TIMERS_MAX_TIMERS_COUNT];

/* Private functions prototype------------------------------------------------*/

/**
 * @brief Callback function called by FreeRTOS when a timer expires.
 * @param xTimer The handle of the expired timer.
 */
static void BleTimerCb(TimerHandle_t xTimer);

/**
 * @brief Find the index of a free timer slot in the timers list.
 * @return Index of a free slot, or BLE_TIMER_NOT_FOUND if none are available.
 */
static int32_t BleTimer_GetFreeTimerIdx(void);

/**
 * @brief Find the index of a timer by its ID.
 * @param id The timer ID to search for.
 * @return Index of the timer with the given ID, or BLE_TIMER_NOT_FOUND if not found.
 */
static int BleTimer_GetTimerIdxById(int id);

/* Private Functions  ---------------------------------------------------------*/

/**************************************************************************************************************************************************************************************************
 *
 *                                                                                              LOCAL FUNCTIONS
 *
 **************************************************************************************************************************************************************************************************/

/** ---------------------------------------------------------------------------
 * @brief Find the index of a free timer slot.
 * @return Index of a free slot, or BLE_TIMER_NOT_FOUND if none are available.
 * --------------------------------------------------------------------------
*/

static int32_t BleTimer_GetFreeTimerIdx(void)
{
  for (int idx = 0; idx < BLE_TIMERS_MAX_TIMERS_COUNT; idx++)
  {
      if (timersList[idx].timerHandle == NULL)
      {
        return idx;
      }
  }
  return BLE_TIMER_NOT_FOUND ;
}

/** ---------------------------------------------------------------------------
 * @brief Find the index of a timer by its ID.
 * @param id The timer ID to search for.
 * @return Index of the timer with the given ID, or BLE_TIMER_NOT_FOUND if not found.
 * --------------------------------------------------------------------------
*/

static int BleTimer_GetTimerIdxById(int id)
{
  for (int idx = 0; idx < BLE_TIMERS_MAX_TIMERS_COUNT ; idx++)
  {
    if (timersList[idx].timerHandle != NULL && timersList[idx].id == id)
    {
      return idx;
    }
  }
  return BLE_TIMER_NOT_FOUND ;  
}

/** ---------------------------------------------------------------------------
 * @brief FreeRTOS timer callback for BLE timers.
 * @details
 *   Called automatically when a BLE timer expires. Notifies the BLE Manager
 *   of the timer expiry, deletes the timer, and marks the slot as free.
 * @param xTimer The handle of the expired timer.
 * --------------------------------------------------------------------------
*/

static void BleTimerCb(TimerHandle_t xTimer)
{  
  BleTimer_t* timerPtr = (BleTimer_t*)pvTimerGetTimerID(xTimer);
  
  if (timerPtr->timerHandle == NULL)
  {
    LogInfo(" BleTimer : a Timer With ( timerHandle = NULL ) expired ! ");
    BleErrHandler();
  }
  else
  {
    /* Notify BLE Manager of timer expiry */
    BleMngr_TimerExpiredEvtInd( timerPtr->id , 0 /* callback */); 
    xTimerDelete(timerPtr->timerHandle, 0);
    timerPtr->timerHandle = NULL;
    timerPtr->id = 0 ;
  }
}

/**************************************************************************************************************************************************************************************************
 *
 *                                                                                              GLOBAl FUNCTIONS
 *
 **************************************************************************************************************************************************************************************************/

/** ---------------------------------------------------------------------------
 * @brief Stop and delete a BLE timer by its ID.
 * @details
 *   If the timer with the given ID exists, it is stopped and deleted, and its
 *   slot in the timers list is marked as free.
 * @param id The ID of the timer to stop.
 * --------------------------------------------------------------------------
*/

void BleTimer_StopTimer(uint16_t id)
{
  int32_t timerIdx = BleTimer_GetTimerIdxById(id);
  
  if ( timerIdx == BLE_TIMER_NOT_FOUND )
  {
    return;
  }
  
  if(timersList[timerIdx].timerHandle != NULL)
  {
    xTimerStop(timersList[timerIdx].timerHandle, 0 /*xTicksToWait*/);
    xTimerDelete(timersList[timerIdx].timerHandle, 0);
    timersList[timerIdx].timerHandle = NULL;
    timersList[timerIdx].id = 0;
  }
}

/** ---------------------------------------------------------------------------
 * @brief Start a BLE timer with a given ID and timeout.
 * @details
 *   If a timer with the same ID already exists, it is stopped first.
 *   A new timer is created and started. If no free timer slots are available,
 *   an error is logged and BLE_STATUS_INSUFFICIENT_RESOURCES is returned.
 * @param id      The unique ID for the timer , passed by BleStack .
 * @param timeout Timeout in milliseconds.
 * @return BLE_STATUS_SUCCESS on success, error code otherwise.
 * --------------------------------------------------------------------------
*/

uint8_t BleTimer_StartTimer(uint16_t id, uint32_t timeout)
{
  BleTimer_t* timer = NULL;
  int timerIdx = 0;
  
  /* If the timer's id already exists, stop it */
  BleTimer_StopTimer(id);
  
  timerIdx = BleTimer_GetFreeTimerIdx();
 
  if (timerIdx == BLE_TIMER_NOT_FOUND)
  {
    LogInfo(" BleTimer : No Sufficient Timers ");
    return BLE_STATUS_INSUFFICIENT_RESOURCES;
  }
  else
  {
    timer = (BleTimer_t*)&(timersList[timerIdx]);
  }

  timer->id = id;

  timer->timerHandle = xTimerCreate(NULL, /* no Timer Name */
                                   (pdMS_TO_TICKS(timeout)),
                                    pdFALSE, /*ONESHOT*/
                                    (void *)&timersList[timerIdx],
                                    (TimerCallbackFunction_t)BleTimerCb);
  if(timer->timerHandle == NULL)
  {
    return BLE_STATUS_FAILED;
  }

  /* Start the newly created timer instance */
  if(pdFAIL == xTimerStart(timer->timerHandle, 0 /*xTicksToWait*/))
  {
    xTimerDelete(timer->timerHandle, 0);
    timer->timerHandle = NULL;
    return BLE_STATUS_FAILED;
  }
  
  return BLE_STATUS_SUCCESS;
}

/** ---------------------------------------------------------------------------
 * @brief Reset all BLE timers.
 * @details
 *   Stops and deletes all active timers, and clears the timers list.
 *   Typically called during BLE de-initialization or system reset.
 * --------------------------------------------------------------------------
*/

void BleTimer_Rest(void)
{
  int32_t idx = 0; 
  BleTimer_t* timerPtr;
  
  while( ++idx < BLE_TIMERS_MAX_TIMERS_COUNT )
  {  
   timerPtr = (BleTimer_t*)&(timersList[idx]);
   if( timerPtr->timerHandle != NULL )
   {
      xTimerDelete(timerPtr->timerHandle, 0);
      timerPtr->timerHandle = NULL;
      timerPtr->id = 0 ;
   }
  }
  
  memset(timersList , 0 , sizeof(timersList));
}

