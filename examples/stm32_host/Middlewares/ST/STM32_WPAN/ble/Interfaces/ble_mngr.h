/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_mngr.h
  * @author  MCD Application Team
  * @brief   Header for ble_mngr.c
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef BLE_MNGR_H
#define BLE_MNGR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "blestack.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
 
/* USER CODE BEGIN ET */
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */
/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */
/* USER CODE END EV */
/* Exported macro ------------------------------------------------------------*/
 
#define BLE_MNGR_MQ_LEN          (30)

#define BLE_MNGR_STACK_SIZE      (4096)

#define BLE_MNGR_PRIO            ( configMAX_PRIORITIES - 2 )
  
/* Exported functions prototypes ---------------------------------------------*/
  
/*-----------------  INITIALIZATION / DE-INITIALIZATION For BleMngr ----------*/

/**
 * @brief
 *   Initialize the BLE Manager Subsystem and All Related Platform Components
 *
 * @details
 *   This function performs the complete initialization of the BLE subsystem, including platform-specific
 *   components, serial interface, timers, FreeRTOS resources, and the BLE stack itself. It ensures that
 *   all required resources are allocated and initialized before BLE operations begin.
 *
 *   - Initializes BLE NVM, serial interface, and timers.
 *   - Creates the BLE Manager message queue and task.
 *   - Initializes the BLE stack and platform dependencies.
 *
 * @note
 *   - Should be called once during system startup before any BLE operations.
 *   - If initialization fails at any step, the function performs cleanup.
 *
 * @param  void  
 * @retval void
 */
void BleMngrInit(void);

/**
 * @brief
 *   De-initialize the BLE Manager and Release All Resources
 *
 * @details
 *   This function de-initializes the BLE Manager, releasing all allocated resources,
 *   deleting tasks, queues, and event groups, and resetting the BLE context.
 *   It is typically called during system shutdown or when BLE functionality is no longer required.
 *
 * @param  void  
 * @retval void
 */
void BleMngrDeInit(void);

/**
 * @brief
 *   BLE Manager and Interfaces Error Handler (Weak Default Implementation)
 *
 * @details
 *   This function is called whenever a critical error or unexpected condition occurs in the BLE
 *   management or communication code. The default implementation is marked as __weak, allowing
 *   the user to override it with a custom error handler elsewhere in the application.
 *
 *   - The default behavior is to trigger a FreeRTOS assertion failure using configASSERT(0).
 *   - This function can be overridden to provide custom error handling, such as logging,
 *     system reset, or recovery actions.
 *
 * @note
 *   - The __weak attribute allows the function to be redefined by the user without linker errors.
 *   - It is recommended to provide a project-specific implementation for production systems.
 *
 * @param  void  
 * @retval void
 */
__WEAK void BleErrHandler(void);

/*----------- Blocking Utilities For BleMngr --------------*/

/**
 * @brief
 *   Block the BLE Manager
 *
 * @details
 *   This function blocks the BleMngr Tasks
 *
 *
 *   Usage:
 *     - This function is called when a BleMngr needs to block, waiting for
 *       a response from the BLE controller before proceeding.
 *     - The corresponding unblock is performed by BleMngrUnBlock().
 *     - Check BleSerialRxFsm , RspRx : RxReq Event Handling , for more inforamtion.
 *
 * @note
 *   - This function should only be called from a task context (not from an ISR).
 */
void BleMngrBlock(void);

/**
 * @brief
 *   Unblock BLE Manager  
 *
 * @details
 *   This function sets the BLE_MNGR_EVT_BLOCK bit in the BLE Manager's event group to unblock
 *   it. 
 *
 *   It supports both interrupt and non-interrupt
 *   (task) contexts, ensuring safe and efficient signaling regardless of where the unblock
 *   request is generated.
 *
 *   Usage:
 *     - This function is called to unblock BleMngr Waiting on a Reponse From BleController over previously transmitted Command ,
 *     - check BLECB_RequestToController for more inforamtion.
 *
 */

void BleMngrUnBlock(void);

/*----------------- EVENT-INDICATION For BleMngr ----------*/

/**
 * @brief  
 *       Indicate to BLE Manager that BleStack_Init needs to be executed.
 *
 * This function allocates and initializes a BLE Manager event structure for the
 * initialization request indication (BLE_MNGR_EVT_INIT_REQ_IND). It sets up the
 * event type, callback, and initialization pointer, and then dispatches the event
 * to the BLE Manager event handler. If memory allocation fails, it logs an error
 * and invokes the error handler.
 *
 * @param[in]  initPtr  Pointer to the BLE stack initialization structure.
 * @param[in]  cb       Callback function to be called upon event completion.
 *
 * @note
 * - The function allocates memory for the event using pvPortMalloc.
 * - The 'ret' field is initialized to 0 and is expected to be filled by BleStack_init.
 * - If memory allocation fails, an error is logged and BleErrHandler() is called.
 */

void BleMngr_InitReqEvtInd( BleStack_init_t* initPtr , void(*cb)(void*) );

/**
 * @brief
 *   Indicate to BLE Manager that BleStack_Process needs to be executed.
 *
 * @details
 *   This function creates and posts a BLE Manager event to request the BLE Manager task to run
 *   a specific process or operation. It dynamically allocates an event structure, initializes
 *   its fields with the event type and an optional callback, and posts it to the BLE Manager's
 *   event queue for processing in the BLE Manager task context.
 *
 * @param  cb
 *   Callback function pointer to be called once the event is processed.
 * @retval void
 */
void BleMngr_AppRunProcEvtInd(void(*cb)(void*));

/**
 * @brief
 *   Indicate to BLE Manager that BleStack_Request needs to be executed.
 *
 * @details
 *   This function creates and posts a BLE Manager event for an Application Controller Interface (ACI)
 *   command request. It dynamically allocates an event structure, initializes its fields with the
 *   input and output pointers, and an optional callback, and posts it to the BLE Manager's event queue
 *   for processing in the BLE Manager task context.
 *
 * @param  InpPtr
 *   Pointer to the input data for the HCI/ACI command.
 * @param  OutPtr
 *   Pointer to the output buffer where the response/result will be stored.
 * @param  cb
 *   Callback function pointer to be called once the event is processed. The callback receives a void*
 *   argument, which can be used to pass status or result information.
 * @retval void
 */
void BleMngr_AppAciCmndEvtInd(void* InpPtr, void* OutPtr, void(*cb)(void*));

/**
 * @brief
 *   Indicate to BLE Manager that BLEPLATCB_TimerExpiry needs to be executed.
 *
 * @details
 *   This function creates and posts a BLE Manager event indicating that a timer has expired.
 *   It dynamically allocates an event structure, initializes its fields with the timer ID and
 *   an optional callback, and posts it to the BLE Manager's event queue for processing in the
 *   BLE Manager task context.
 *
 * @param  id
 *   The identifier of the expired timer.
 * @param  cb
 *   Callback function pointer to be called once the event is processed.
 * @retval void
 */
void BleMngr_TimerExpiredEvtInd(uint32_t id, void(*cb)(void*));

/**
 * @brief
 *   Indicate to BLE Manager that BleStack_IndicationFromController needs to be executed.
 *
 * @details
 *   This function creates and posts a BLE Manager event for an asynchronous serial packet received
 *   from the BLE controller. It dynamically allocates an event structure, initializes its fields,
 *   and posts it to the BLE Manager's event queue for processing in the BLE Manager task context.
 *   A callback function can be specified, which will be invoked after the event is processed.
 *
 * @param  InpPtr
 *   Buffer pointer to be passed to BleStack_IndicationFromController.
 * @param  cb
 *   Callback to be called once the event is processed. The return from BleStack_IndicationFromController
 *   will be passed to cb as input argument.
 * @retval void
 */
void BleMngr_SerialAsynchPcktEvtInd(void* InpPtr, void(*cb)(void*));

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/**
 * @brief
 * Callback invoked when a packet is received from the BLE serial interface.
 * @details
 *   - If the packet is a response (to a previous request), copies it to the response buffer and unblocks the manager.
 *   - If the packet is an asynchronous event, notifies the BLE manager to process it.
 * @param Buf Pointer to received data buffer
 * @param Size Size of received data in bytes
 */
void BleMngr_BleSerialRxCb( uint8_t* Buf , uint16_t Size );

/**
 * @brief
 * Callback invoked when a packet transmit is done over the BLE serial interface.
 * @details
 *   - once the packet is transmitted , notify the BLE manager.
 * @param Buf Pointer to received data buffer
 * @param Size Size of received data in bytes
 */
void BleMngr_BleSerialTxCb( void );

#ifdef __cplusplus
}
#endif

#endif /* BLE_MNGR_H */
