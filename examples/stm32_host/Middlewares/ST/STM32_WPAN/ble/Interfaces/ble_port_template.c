/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_port_template.c
  * @author  MCD Application Team
  * @brief   BLE Subsystem ( BleMngr , BleSerial , BleNvm , BleTimer ) Port File
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

/* Includes ------------------------------------------------------------------*/

#include "logging.h"

/* Private includes ----------------------------------------------------------*/

/* BleStack Inlcude Files */
#include "ble_core.h"
#include "blestack.h"
#include "bleplat.h"

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

/* Private macro -------------------------------------------------------------*/

#define BLE_PORT_SERIAL_WAIT_HAL()                    while( BlePortCntxt.BleRcpHandler->gState != HAL_UART_STATE_READY );
                                                 
#define BLE_PORT_SERIAL_HAL_ERROR_ORE()               ( BlePortCntxt.BleRcpHandler->ErrorCode & HAL_UART_ERROR_ORE )                                                                      
#define BLE_PORT_SERIAL_HAL_ERROR_FE()                ( BlePortCntxt.BleRcpHandler->ErrorCode & HAL_UART_ERROR_FE  )
#define BLE_PORT_SERIAL_HAL_ERROR_NE()                ( BlePortCntxt.BleRcpHandler->ErrorCode & HAL_UART_ERROR_NE  )
#define BLE_PORT_SERIAL_HAL_ERROR_PE()                ( BlePortCntxt.BleRcpHandler->ErrorCode & HAL_UART_ERROR_PE  )
#define BLE_PORT_SERIAL_HAL_ERROR_DMA()               ( BlePortCntxt.BleRcpHandler->ErrorCode & HAL_UART_ERROR_DMA )
#define BLE_PORT_SERIAL_HAL_ERROR_RTO()               ( BlePortCntxt.BleRcpHandler->ErrorCode & HAL_UART_ERROR_RTO )
                                                 
#define BLE_PORT_SERIAL_HAL_RECEIVETOIDlE_DMA(...)    do { \
                                                        HAL_StatusTypeDef HalStatus = HAL_UARTEx_ReceiveToIdle_DMA(BlePortCntxt.BleRcpHandler,__VA_ARGS__); \
                                                        /* Disable Half Transfer IT (not used but always set by HAL_UARTEx_ReceiveToIdle_DMA)*/ \
                                                        __HAL_DMA_DISABLE_IT(BlePortCntxt.BleRcpRxDma, DMA_IT_HT); \
                                                        if (HalStatus != HAL_OK) { LogInfo("[BlePort_Serial] Start RX ERROR"); BleErrHandler(); } \
                                                    } while(0);

#define BLE_PORT_SERIAL_HAL_RECEIVE_DMA(...)          do { \
                                                        HAL_StatusTypeDef HalStatus = HAL_UART_Receive_DMA(BlePortCntxt.BleRcpHandler,__VA_ARGS__); \
                                                        /* Disable Half Transfer IT (not used but always set by HAL_UARTEx_ReceiveToIdle_DMA)*/ \
                                                        __HAL_DMA_DISABLE_IT(BlePortCntxt.BleRcpRxDma, DMA_IT_HT); \
                                                        if (HalStatus != HAL_OK) { LogInfo("[BlePort_Serial] Start RX ERROR"); BleErrHandler(); } \
                                                    } while(0);

#define BLE_PORT_SERIAL_HAL_TRANSMIT_DMA(...)         do { \
                                                       HAL_StatusTypeDef HalStatus = HAL_UART_Transmit_DMA(BlePortCntxt.BleRcpHandler,__VA_ARGS__); \
                                                       if (HalStatus != HAL_OK) { LogInfo("[BlePort_Serial] Start TX ERROR"); BleErrHandler(); } \
                                                    }while(0);

#define BLE_PORT_SERIAL_HAL_ABORT_RX()                do { \
                                                       /* Abort ongoing Rx */ \
                                                       HAL_StatusTypeDef HalStatus = HAL_UART_AbortReceive(BlePortCntxt.BleRcpHandler); \
                                                       if (HalStatus != HAL_OK) { LogInfo("[BlePort_Serial] Abort RX ERROR"); BleErrHandler(); } \
                                                    }while(0);
 
/* Private defines -----------------------------------------------------------*/

/**
 * @brief  BLE port context structure.
 *
 * This structure holds all context information and callback pointers required
 * for 
 * 1- managing the BLE serial interface (e.g., UART or SPI) Port and its associated DMA channels.
 * 2- managing the BLE NVM interface Port.
 * 3- managing the BLE Debug Port.
 *
 * Members:
 * - BleRcpHandler:
 *     Pointer to the hardware peripheral handler structure for the BLE serial interface
 *     (e.g., UART/SPI). Used for configuring and controlling the hardware interface.
 *
 * - BleRcpRxDma:
 *     Pointer to the DMA channel handler structure for receive operations.
 *     Used to manage DMA-based data transfers from the BLE controller.
 *
 * - BleRcpTxDma:
 *     Pointer to the DMA channel handler structure for transmit operations.
 *     Used to manage DMA-based data transfers to the BLE controller.
 *
 */

typedef struct 
{
  BLE_RCP_HANDLER_TYPE_DEF             *BleRcpHandler; /* Hardware peripheral handler   */
  BLE_RCP_DMA_CHANNEL_HANDLER_TYPE_DEF *BleRcpRxDma;   /* DMA handler for RX            */
  BLE_RCP_DMA_CHANNEL_HANDLER_TYPE_DEF *BleRcpTxDma;   /* DMA handler for TX            */
    
}BlePortContext;

/* Private variables ---------------------------------------------------------*/

BlePortContext BlePortCntxt;

/* Global variables ----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static void blePort_SerialTxCpltCallback   (BLE_RCP_HANDLER_TYPE_DEF *handler);
static void blePort_SerialRxCpltCallBack   (BLE_RCP_HANDLER_TYPE_DEF *handler,uint16_t size);
static void blePort_SerialHalErrorCallback (BLE_RCP_HANDLER_TYPE_DEF *handler);

/* External variables --------------------------------------------------------*/

/* Functions Definition ------------------------------------------------------*/

/*===============================================================================================================================================================================================*/

/*------------------------------------------------------------------------------------- BLE PORT , DEBUG  ---------------------------------------------------------------------------------------*/

/*===============================================================================================================================================================================================*/

/**
 * @brief  Debug hook functions for BLE stack and platform events.
 *
 * These functions are used to toggle specific GPIO pins for logic analyzer debugging
 * at key entry and exit points in the BLE stack and platform event processing.
 * When LOGIC_ANALYIZER_DEBUG is enabled, each function toggles its associated GPIO pin
 * to provide a visual indication of event timing and execution flow.
 *
 * Functions and their purposes:
 * - BlePort_DebugBleStackProcessEntryHook / ExitHook:
 *     Toggle the BLE process debug pin on entry/exit of BleStack_Process.
 * - BlePort_DebugBleStackRequestEntryHook / ExitHook:
 *     Toggle the BLE request indication debug pin on entry/exit of BleStack_Request.
 * - BlePort_DebugBleStackIndicationFromControllerEntryHook / ExitHook:
 *     Toggle the BLE controller indication debug pin on entry/exit of BleStack_IndicationFromController.
 * - BlePort_DebugBleCBRequestToControllerEntryHook / ExitHook:
 *     Toggle the BLE callback request-to-controller debug pin on entry/exit of BLECB_RequestToController.
 * - BlePort_DebugBleStackInitEntryHook / ExitHook:
 *     Not used; provided for interface completeness.
 * - BlePort_DebugBlePlatCbTimerExpiryEntryHook / ExitHook:
 *     Not used; provided for interface completeness.
 * - BlePort_DebugBleCbIndicationEntry / ExitHook:
 *     Toggle the BLE callback to toggle debug pin on entry/exit of BLECB_Indication.
 *
 * @note
 * - These hooks are only active when BLE_LOGIC_ANALYIZER_DEBUG is set to 1.
 * - Intended for use with an external logic analyzer to trace BLE stack activity.
 * - Unused hooks are provided for consistency and future extension.
 */
/*----------------------------------------------*/
/* called on BleStack_Process Entry */
void BlePort_DebugBleStackProcessEntryHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLE_PROC_GPIO_PORT, LOGIC_DEBUG_BLE_PROC_GPIO_PIN);
#endif
}
/* called on BleStack_Process Exit */
void BlePort_DebugBleStackProcessExitHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLE_PROC_GPIO_PORT, LOGIC_DEBUG_BLE_PROC_GPIO_PIN);
#endif  
}
/*----------------------------------------------*/
/* called on BleStack_Request Entry */
void BlePort_DebugBleStackRequestEntryHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLE_REQ_IND_GPIO_PORT, LOGIC_DEBUG_BLE_REQ_IND_GPIO_PIN);
#endif
}
/* called on BleStack_Request Exit */
void BlePort_DebugBleStackRequestExitHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLE_REQ_IND_GPIO_PORT, LOGIC_DEBUG_BLE_REQ_IND_GPIO_PIN);
#endif
}
/*----------------------------------------------*/
/* called on BleStack_IndicationFromController Entry */
void BlePort_DebugBleStackIndicationFromControllerEntryHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PORT, LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PIN);
#endif
}
/* called on BleStack_IndicationFromController Exit */
void BlePort_DebugBleStackIndicationFromControllerExitHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PORT, LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PIN);
#endif
}
/*----------------------------------------------*/
/* called on BLECB_RequestToController Entry */
void BlePort_DebugBleCBRequestToControllerEntryHook ( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PORT, LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PIN);;
#endif
}
/* called on BLECB_RequestToController Exit */  
void BlePort_DebugBleCBRequestToControllerExitHook( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PORT, LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PIN);;
#endif
}
/*----------------------------------------------*/
/* called on BleStack_Init Entry */
void BlePort_DebugBleStackInitEntryHook ( void )
{
  /* NOT USED !! */
}
/* called on BleStack_Init Exit */
void BlePort_DebugBleStackInitExitHook ( void )
{
  /* NOT USED !! */
}
/*----------------------------------------------*/
/* called on BLEPLATCB_TimerExpiry Entry */
void BlePort_DebugBlePlatCbTimerExpiryEntryHook ( void )
{
  /* NOT USED !! */
}
/* called on BLEPLATCB_TimerExpiry Exit */
void BlePort_DebugBlePlatCbTimerExpiryExitHook ( void )
{
  /* NOT USED !! */
}
/*----------------------------------------------*/
/* called on BLECB_Indication Entry */
void BlePort_DebugBleCbIndicationEntry( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLECB_IND_GPIO_PORT, LOGIC_DEBUG_BLECB_IND_GPIO_PIN);;
#endif
}
/* called on BLECB_Indication Entry */
void BlePort_DebugBleCbIndicationExit( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLECB_IND_GPIO_PORT, LOGIC_DEBUG_BLECB_IND_GPIO_PIN);;
#endif
}
/*----------------------------------------------*/
/**
 * @brief  Initializes GPIO pins for BLE logic analyzer debug signals.
 *
 * This function configures a set of GPIO pins as outputs for use with an external
 * logic analyzer, enabling detailed debugging of BLE stack events and state transitions.
 * The function is only compiled and executed if BLE_LOGIC_ANALYIZER_DEBUG is set to 1.
 *
 * The following steps are performed:
 *   - Enables the clocks for all required GPIO ports.
 *   - Sets the initial output level of each debug GPIO pin to RESET (logic low).
 *   - Configures each debug GPIO pin as a push-pull output with no pull-up or pull-down.
 *
 * @note
 * - Only runs if BLE_LOGIC_ANALYIZER_DEBUG is enabled at compile time.
 * - Intended for use in development and debugging environments.
 */

void BlePort_DebugInit( void )
{
#if ( BLE_LOGIC_ANALYIZER_DEBUG == 1 )
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  __HAL_RCC_GPIO_BLE_DEBUG_REQ_PORT_CLK_ENABLE();
  __HAL_RCC_GPIO_BLE_DEBUG_PROC_PORT_CLK_ENABLE();
  __HAL_RCC_GPIO_BLE_DEBUG_CNTRL_IND_PORT_CLK_ENABLE();
  __HAL_RCC_GPIO_BLE_DEBUG_BLECB_REQ_TO_CNTRL_PORT_CLK_ENABLE();
  __HAL_RCC_GPIO_BLE_DEBUG_BLECB_IND_PORT_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOGIC_DEBUG_BLE_REQ_IND_GPIO_PORT, LOGIC_DEBUG_BLE_REQ_IND_GPIO_PIN, GPIO_PIN_RESET); 
  
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PORT, LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PIN, GPIO_PIN_RESET); 
  
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOGIC_DEBUG_BLE_PROC_GPIO_PORT, LOGIC_DEBUG_BLE_PROC_GPIO_PIN, GPIO_PIN_RESET);
  
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PORT, LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PIN, GPIO_PIN_RESET);
 
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LOGIC_DEBUG_BLECB_IND_GPIO_PORT, LOGIC_DEBUG_BLECB_IND_GPIO_PIN, GPIO_PIN_RESET);
  
  /*Configure GPIO pin : BLE_CNTRL_IND_GPIO_PIN */
  GPIO_InitStruct.Pin = LOGIC_DEBUG_BLE_REQ_IND_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LOGIC_DEBUG_BLE_REQ_IND_GPIO_PORT, &GPIO_InitStruct);
  
  /*Configure GPIO pin : BLE_CNTRL_IND_GPIO_PIN */
  GPIO_InitStruct.Pin = LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LOGIC_DEBUG_BLE_CNTRL_IND_GPIO_PORT, &GPIO_InitStruct);
  
  /*Configure GPIO pin : LOGIC_DEBUG_BLE_PROC_GPIO_PIN */
  GPIO_InitStruct.Pin = LOGIC_DEBUG_BLE_PROC_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LOGIC_DEBUG_BLE_PROC_GPIO_PORT, &GPIO_InitStruct);
  
  /*Configure GPIO pin : LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PIN */
  GPIO_InitStruct.Pin = LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LOGIC_DEBUG_BLECB_REQ_TO_CNTRL_GPIO_PORT, &GPIO_InitStruct);
  
  /*Configure GPIO pin : LOGIC_DEBUG_BLECB_IND_GPIO_PIN */
  GPIO_InitStruct.Pin = LOGIC_DEBUG_BLECB_IND_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LOGIC_DEBUG_BLECB_IND_GPIO_PORT, &GPIO_InitStruct);
  
#endif   
}

/*===============================================================================================================================================================================================*/

/*------------------------------------------------------------------------------------- BLE PORT , SERIAL  ---------------------------------------------------------------------------------------*/

/*===============================================================================================================================================================================================*/

/**
 * @brief  Initializes the BLE serial port interface and registers callbacks.
 *
 * This function sets up the BLE serial port context. 
 * It also registers the hardware peripheral and DMA handlers
 * required for BLE serial communication. Finally, it assigns the appropriate
 * hardware abstraction layer (HAL) callbacks for UART events.
 *
 *
 * @note
 * - It is assumed that the UART MSP (MCU Support Package) initialization is handled
 *   by the CubeMX layer. Therefore, there is no need to call MX_USART_UART_Init or
 *   HAL_UART_MspInit in this function.
 */

void BlePort_SerialInit( void )
{

  /*
   * It is assumed that UART MSP Init is done by
   * CubeMX layer. As such, no need to call
   * MX_USART_UART_Init & HAL_UART_MspInit.
   */
  
  HAL_NVIC_SetPriority( BLE_RCP_HW_PERIPHERAL_IRQ , configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, /* sub group */ 0);
  HAL_NVIC_EnableIRQ( BLE_RCP_HW_PERIPHERAL_IRQ );
  
  /* Register HW HAL Handlers */
  BlePortCntxt.BleRcpHandler      = &BLE_RCP_HW_PERIPHERAL_HANDLER;
  BlePortCntxt.BleRcpTxDma        = &BLE_RCP_DMA_TX_CHANNEL_HANDLER;
  BlePortCntxt.BleRcpRxDma        = &BLE_RCP_DMA_RX_CHANNEL_HANDLER;
    
  /* Register HW HAL callbacks */
  BlePortCntxt.BleRcpHandler->TxCpltCallback  = blePort_SerialTxCpltCallback;
  BlePortCntxt.BleRcpHandler->RxEventCallback = blePort_SerialRxCpltCallBack;
  BlePortCntxt.BleRcpHandler->ErrorCallback   = blePort_SerialHalErrorCallback;
  
  return;
}

/**
 * @brief
 *   BLE Serial HAL Transmit Complete Callback Handler
 *
 * @details
 *   This function is called by the hardware abstraction layer (HAL) when a DMA-based or interrupt-driven
 *   transmit (TX) operation for BLE serial data completes. It is invoked after the entire data buffer
 *   requested for transmission has been sent�**not after each UART frame or byte, but after the whole buffer**.
 *
 *   - For DMA mode (e.g., HAL_UART_Transmit_DMA):
 *       The callback is called after the DMA controller has finished transferring all bytes from memory
 *       to the UART peripheral, and the UART has shifted out the last byte.
 *   - For interrupt mode (e.g., HAL_UART_Transmit_IT):
 *       The callback is called after all bytes have been sent using interrupts.
 *
 *   In this implementation, the BleSerial Callback , bleSerial_TxCpltCb() , Registered is invoked 
 *
 * @param handler
 *   Pointer to the BLE hardware handler structure (BLE_RCP_HANDLER_TYPE_DEF) associated with the
 *   transmit operation. This parameter can be used to identify the UART/SPI instance if needed.
 *
 * @return
 *   void
 *
 * @note
 *   - This callback is registered with the HAL and is invoked automatically when a transmit
 *     operation completes.
 *   - It is essential for enabling non-blocking, event-driven serial communication.
 */
static void blePort_SerialTxCpltCallback (BLE_RCP_HANDLER_TYPE_DEF *handler)
{
   if( handler->Instance == BlePortCntxt.BleRcpHandler->Instance ) 
   {
     bleSerial_TxCpltCb();
   }
   
   return;
}

/**
 * @brief
 *   BLE Serial HAL Receive Complete Callback Handler
 *
 * @details
 *   This function is called by the hardware abstraction layer (HAL) when a DMA-based receive operation
 *   for BLE serial data completes.
 *
 *   In this implementation, the BleSerial Callback , bleSerial_RxCpltCb() , Registered is invoked 
 * 
 * @param handler
 *   Pointer to the BLE hardware handler structure (BLE_RCP_HANDLER_TYPE_DEF) that triggered the callback.
 *   The function only processes callbacks for the handler instance associated with the current BLE serial context.
 *
 * @param size
 *   The number of bytes received in the latest DMA transfer.
 *
 * @return
 *   void
 *
 * @note
 *   - This callback is registered with the HAL and is invoked automatically when a DMA receive completes.
 */

static void blePort_SerialRxCpltCallBack (BLE_RCP_HANDLER_TYPE_DEF *handler,uint16_t size)
{  
  if( handler->Instance == BlePortCntxt.BleRcpHandler->Instance ) 
  {
    bleSerial_RxCpltCb( size );
  }
  return;
}

/**
 * @brief
 *   BLE Serial HAL Error Callback Handler
 *
 * @details
 *   This function is called by the hardware abstraction layer (HAL) when a hardware error occurs
 *   during BLE serial communication. It checks if the error is associated with the current BLE serial
 *   receive/transmit handler instance, and if so, logs detailed information about the specific error(s)
 *   encountered. After logging, it invokes the generic BLE error handler to manage the error condition.
 *
 *   - The function checks the following error types :
 *       - BLE_PORT_SERIAL_HAL_ERROR_ORE:   Overrun Error (data lost because the HW buffer was full)
 *       - BLE_PORT_SERIAL_HAL_ERROR_FE:    Framing Error (incorrect frame format)
 *       - BLE_PORT_SERIAL_HAL_ERROR_NE:    Noise Error (signal noise detected)
 *       - BLE_PORT_SERIAL_HAL_ERROR_PE:    Parity Error (parity check failed)
 *       - BLE_PORT_SERIAL_HAL_ERROR_DMA:   DMA Error (DMA transfer failure)
 *       - BLE_PORT_SERIAL_HAL_ERROR_RTO:   Receive Timeout Error (timeout waiting for data)
 *
 *   - For each error type detected, a corresponding log message is generated for debugging and diagnostics.
 *   - After logging all relevant errors, the function calls registered bleSerial_ErrCb to handle the error condition.
 *
 * @param handler
 *   Pointer to the BLE hardware handler structure (BLE_RCP_HANDLER_TYPE_DEF) that reported the error.
 *   The function only processes errors for the handler instance associated with the current BLE serial context.
 *
 * @return
 *   void
 *
 * @note
 *   - This callback is registered with the HAL and is invoked automatically on hardware errors.
 */

static void blePort_SerialHalErrorCallback (BLE_RCP_HANDLER_TYPE_DEF *handler)
{
  if( handler->Instance == BlePortCntxt.BleRcpHandler->Instance ) 
  {
    if ( BLE_PORT_SERIAL_HAL_ERROR_ORE() )  LogInfo("blePort_Serial : Overrun Error  \n");
    if ( BLE_PORT_SERIAL_HAL_ERROR_FE()  )  LogInfo("blePort_Serial : Framing Error  \n");
    if ( BLE_PORT_SERIAL_HAL_ERROR_NE()  )  LogInfo("blePort_Serial : Noise Error    \n");
    if ( BLE_PORT_SERIAL_HAL_ERROR_PE()  )  LogInfo("blePort_Serial : Parity Error   \n");
    if ( BLE_PORT_SERIAL_HAL_ERROR_DMA() )  LogInfo("blePort_Serial : DMA Error      \n");
    if ( BLE_PORT_SERIAL_HAL_ERROR_RTO() )  LogInfo("blePort_Serial : RTO Error      \n");
    
    bleSerial_ErrCb();
  }
  return;
}

/**
 * @brief  Receives data over BLE serial until an idle condition is detected.
 *
 * This function initiates a DMA-based UART receive operation that continues
 * until an idle line condition is detected or the specified maximum buffer size
 * is reached. The received data is stored in the provided buffer.
 *
 * @param[out] Buf      Pointer to the buffer where received data will be stored.
 * @param[in]  MaxSize  Maximum number of bytes to receive.
 *
 * @note
 * - The function uses a hardware abstraction macro (BLE_PORT_SERIAL_HAL_RECEIVETOIDlE_DMA)
 *   to start the receive operation.
 * - Reception stops when either the buffer is full or an idle line is detected.
 */

void BlePort_SerialRxUntilIdle(uint8_t* Buf,uint16_t MaxSize )
{
  BLE_PORT_SERIAL_HAL_RECEIVETOIDlE_DMA(Buf , MaxSize );
  return;
}

/**
 * @brief  Receives a specified amount of data over BLE serial .
 *
 * This function initiates a DMA-based UART receive operation to read a fixed
 * number of bytes from the BLE serial interface. The received data is stored
 * in the provided buffer.
 *
 * @param[out] Buf   Pointer to the buffer where the received data will be stored.
 * @param[in]  Size  Number of bytes to receive.
 *
 * @note
 * - The function uses a hardware abstraction macro (BLE_PORT_SERIAL_HAL_RECEIVE_DMA)
 *   to start the receive operation.
 * - Reception completes when the specified number of bytes has been received.
 */
void BlePort_SerialRx(uint8_t* Buf,uint16_t Size )
{
  BLE_PORT_SERIAL_HAL_RECEIVE_DMA(Buf , Size );
  return;
}

/**
 * @brief  Transmits data over BLE serial .
 *
 * This function initiates a DMA-based UART transmit operation to send a specified
 * number of bytes over the BLE serial interface. Before starting the transmission,
 * it waits for any previous UART transmission to complete to ensure the interface
 * is ready.
 *
 * @param[in] Buf   Pointer to the buffer containing the data to be transmitted.
 * @param[in] Size  Number of bytes to transmit.
 *
 * @note
 * - The function first waits for the UART hardware to become available by calling
 *   BLE_PORT_SERIAL_WAIT_HAL().
 * - It then starts the DMA-based transmission using BLE_PORT_SERIAL_HAL_TRANSMIT_DMA().
 * - Transmission is asynchronous; completion is typically signaled via a callback.
 */
void BlePort_SerialTx(uint8_t* Buf,uint16_t Size )
{
   BLE_PORT_SERIAL_WAIT_HAL();
   BLE_PORT_SERIAL_HAL_TRANSMIT_DMA(Buf, Size);
   BLE_PORT_SERIAL_WAIT_HAL();
   return;
}

/**
 * @brief  Aborts any ongoing BLE serial receive operation.
 *
 * This function immediately aborts any active UART receive operation on the BLE
 * serial interface. It is typically used to stop reception in case of errors,
 * timeouts, or when a new receive operation needs to be started.
 *
 * @note
 * - The function uses a hardware abstraction macro (BLE_PORT_SERIAL_HAL_ABORT_RX)
 *   to perform the abort operation.
 * - After calling this function, the receive buffer may contain incomplete data.
 */

void BlePort_SerialAbortRx( void )
{
  BLE_PORT_SERIAL_HAL_ABORT_RX();
  return;
}

/*===============================================================================================================================================================================================*/

/*------------------------------------------------------------------------------------- BLE PORT , NVM  -----------------------------------------------------------------------------------------*/

/*===============================================================================================================================================================================================*/
