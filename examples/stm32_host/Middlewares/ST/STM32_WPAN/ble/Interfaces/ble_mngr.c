/**
 ******************************************************************************
 * @file    ble_mngr.c
 * @author  MCD Application Team
 * @brief   BLE Stack Manager: Event-Driven BLE Stack and Platform Orchestration
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 *
 * @section Big Picture & Software Dynamics
 *
 *   This file implements the **BLE Manager** for orchestrating the BLE stack, platform services,
 *   and application requests in a robust, event-driven, and thread-safe manner using FreeRTOS.
 *
 *   **Core Responsibilities:**
 *   - Acts as the central dispatcher for all BLE stack operations, platform events, and application commands.
 *   - Bridges the BLE stack, application, and hardware abstraction layers (serial, NVM, timers).
 *   - Provides a flexible, asynchronous event queue and callback mechanism for all BLE-related operations.
 *   - Ensures safe, serialized, and prioritized processing of BLE commands, responses, and platform events.
 *
 *   **Software Dynamics & Interactions:**
 *
 *   - **Initialization:** The BLE Manager initializes the BLE stack, NVM, serial, and timer subsystems,
 *     and sets up FreeRTOS resources (task, queue, event group).
 *   
 *   - **Event Posting:** All BLE-related actions (stack commands, timer expiries, serial events, etc.)
 *     are posted as events to the BLE Manager's message queue, from both ISRs and tasks.
 *   
 *   - **Event Processing Loop:** The BLE Manager task blocks on its event queue, waking up to process
 *     each event in order. Each event is dispatched to a handler based on its type.
 *
 *   - **Callback Mechanism:** Each event can carry a callback, which is invoked after the event is processed,
 *     enabling asynchronous notification to the application or other modules.
 *
 *   - **Stack/Platform Coordination:** The manager coordinates stack processing, application command execution,
 *     timers expiry handling, and serial packet processing, ensuring correct sequencing and resource management.
 *
 *   - **Blocking/Unblocking:** The manager provides mechanisms for tasks to block on events (e.g., waiting
 *     for a response) and to be unblocked when the event occurs, using FreeRTOS event groups.
 *
 *   - **Error Handling:** All critical operations are checked for errors, with centralized error handling
 *     and detailed logging to ensure system robustness.
 *
 *   **Key Components:**
 *     - **Event Structures:** Typed event structures for all supported event types, with callback support.
 *     - **BLE Manager Context:** Central structure holding stack buffers, FreeRTOS handles, and state.
 *     - **Initialization/Deinitialization:** Functions to set up and tear down the BLE stack and platform.
 *     - **Event Indication Functions:** APIs for posting events to the manager from any context.
 *     - **Event Processing Loop:** The main BLE Manager task, which processes events and invokes callbacks.
 *     - **Blocking Utilities:** Functions for blocking/unblocking on specific BLE events.
 *     - **Stack/Platform Integration:** Tight integration with BLE stack, serial, NVM, and timer modules.
 *
 *   **How to Navigate:**
 *     - Start with the BLE Manager context and initialization functions to understand resource allocation.
 *     - Review the event structures and event indication functions to see how events are posted and processed.
 *     - Examine the main event processing loop (`BleMngr`) to understand the event-driven workflow.
 *     - Check the event handler (`BleMngrProcEvt`) for logic on how each event type is processed.
 *     - Look at the blocking/unblocking utilities for synchronization mechanisms.
 *     - All public APIs and key internal functions are documented with detailed comments.
 *
 *   **Typical Flow:**
 *     1. Application or platform posts an event (e.g., command, timer expiry, serial packet) to the BLE Manager.
 *     2. BLE Manager task wakes up, processes the event, and invokes any associated callback.
 *     3. BLE Manager coordinates with the BLE stack and platform modules to execute the requested operation.
 *     4. If blocking is required (e.g., waiting for a response), the manager handles synchronization.
 *     5. Errors are logged and handled centrally to ensure system stability.
 *
 *   This architecture enables robust, scalable, and maintainable BLE stack management for complex embedded
 *   and real-time applications, supporting both synchronous and asynchronous workflows.
 *
 *******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "logging.h"

/* Private includes ----------------------------------------------------------*/

/* BleStack Inlcude Files */
#include "ble_core.h"
#include "blestack.h"
#include "bleplat.h"

/* Ble Application Configuration Files */
#include "app_common.h"
#include "ble_conf.h"

/* Ble Application Inlcude File */
#include "app_ble.h"

/* BleMngr And Interfaces Inlcude Files */
#include "ble_port.h"
#include "ble_mngr.h"
#include "ble_serial.h"
#include "ble_nvm.h"
#include "ble_timer.h"

/* FreeRTOS Include Files */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"

/* Private defines -----------------------------------------------------------*/

/* --------- BleMngr Macros  --------- */

#define BLE_MNGR_EVT_BLOCK                                   (1<<5)
#define BLE_MNGR_BLOCK_TIM_MS                                (1000)        
#define BLE_MNGR_MQ_HANDLE                                   (BleMngrCntxt.BleMngrMQHandle)
#define BLE_MNGR_TASK_HANDLE                                 (BleMngrCntxt.BleMngrHandle)
#define BLE_MNGR_MQ_MESSAGE_LEN                              (sizeof(BleMngrEvtAppAciCmndInd)) /* Max Event Size */
#define BLE_MNGR_MQ_MESSAGE_TYPEDEF                          BleMngrEvtAppAciCmndInd

/**
 * This macro executes the BLE stack processing routine by calling BleStack_Process().
 * It also invokes debug entry and exit hooks before and after the processing call.
 * If BleStack_Process() returns 0 (indicating that further processing is required),
 * the macro posts an application event (BleMngr_AppRunProcEvtInd) to schedule
 * another run of the BLE stack process.
 *
 */ 
#define BLE_MNGR_BLESTACK_PROCESS()                          do{ /* Run BleStack_Process , post event to rerun BleStack_Process if ret is 0 */ \
                                                                 uint8_t ret; \
                                                                 BlePort_DebugBleStackProcessEntryHook(); \
                                                                 ret = BleStack_Process(); \
                                                                 BlePort_DebugBleStackProcessExitHook(); \
                                                                 if( ret == 0x00 ) BleMngr_AppRunProcEvtInd( NULL /* callback */ ); \
                                                               }while(0);

/**
 * This macro wraps a call to BleStack_Request(), allowing passing a variable
 * number of arguments to the request function. It also invokes debug entry and
 * exit hooks before and after the request call. The result of BleStack_Request()
 * is assigned to the variable X.
 *
 * @param[out] X         Variable to store the return value of BleStack_Request().
 * @param[in]  ...       Arguments to be passed to BleStack_Request().
 *
 */
#define BLE_MNGR_BLESTACK_REQUEST(X,...)                     do{ /* Run BleStack_Request */ \
                                                                 BlePort_DebugBleStackRequestEntryHook(); \
                                                                 (X) = BleStack_Request(__VA_ARGS__); \
                                                                 BlePort_DebugBleStackRequestExitHook(); \
                                                               }while(0);

/**
 * This macro wraps a call to BleStack_Init(), allowing passing a variable
 * number of arguments to the initialization function. It also invokes debug
 * entry and exit hooks before and after the initialization call. The result of
 * BleStack_Init() is assigned to the variable X.
 *
 * @param[out] X         Variable to store the return value of BleStack_Init().
 * @param[in]  ...       Arguments to be passed to BleStack_Init().
 *
 */
#define BLE_MNGR_BLESTACK_INIT(X,...)                        do{ /* Run BleStack_Init */ \
                                                                 BlePort_DebugBleStackInitEntryHook(); \
                                                                 (X) = BleStack_Init(__VA_ARGS__); \
                                                                 BlePort_DebugBleStackInitExitHook(); \
                                                               }while(0);

/**
 * This macro wraps a call to BLEPLATCB_TimerExpiry(), allowing passing a variable
 * number of arguments to the timer expiry callback. It also invokes debug entry
 * and exit hooks before and after the callback execution.
 *
 * @param[in]  ...   Arguments to be passed to BLEPLATCB_TimerExpiry().
 *
 */
#define BLE_MNGR_BLEPLATCB_TIMEREXPIRY(...)                  do{ /* Run BLEPLATCB_TimerExpiry */ \
                                                                 BlePort_DebugBlePlatCbTimerExpiryEntryHook(); \
                                                                 BLEPLATCB_TimerExpiry(__VA_ARGS__); \
                                                                 BlePort_DebugBlePlatCbTimerExpiryExitHook(); \
                                                               }while(0);

/**
 * This macro wraps a call to BleStack_IndicationFromController(), allowing passing a variable
 * number of arguments to the indication handler. It also invokes debug entry and exit hooks
 * before and after the indication handling call. The result of BleStack_IndicationFromController()
 * is assigned to the variable X.
 *
 * @param[out] X         Variable to store the return value of BleStack_IndicationFromController().
 * @param[in]  ...       Arguments to be passed to BleStack_IndicationFromController().
 */
#define BLE_MNGR_BLESTACK_INDICATIONFROMCONTROLLER(X,...)   do{ /* Run BleStack_IndicationFromController */ \
                                                                BlePort_DebugBleStackIndicationFromControllerEntryHook(); \
                                                                (X) = BleStack_IndicationFromController(__VA_ARGS__); \
                                                                BlePort_DebugBleStackIndicationFromControllerExitHook(); \
                                                              }while(0);
/* --------- BleStack Init Macro  --------- */

#define BLE_STACK_INIT_PARAM_PTR                             ((BleStack_init_t*)&(BleMngrCntxt.pInitParams))
#define BLE_STACK_EVENT_BUF_SIZE                             (CFG_BLE_HOST_EVENT_BUF_SIZE)
#define BLE_CTRL_EVENT_FIFO_BUF_SIZE                         (CFG_BLE_CTRL_EVENT_FIFO_BUF_SIZE)
#define BLE_EXTRA_DATA_BUF_SIZE                              (CFG_BLE_EXTRA_DATA_BUF_SIZE)

/* GATT buffer size (in bytes)*/
#define BLE_GATT_BUF_SIZE                                   BLE_TOTAL_BUFFER_SIZE_GATT(CFG_BLE_NUM_GATT_ATTRIBUTES, \
                                                                                       CFG_BLE_NUM_GATT_SERVICES, \
                                                                                       CFG_BLE_ATT_VALUE_ARRAY_SIZE)
/* Number of allocated memory blocks */            
#define MBLOCK_COUNT                                        (BLE_MBLOCKS_CALC(PREP_WRITE_LIST_SIZE, \
                                                                CFG_BLE_ATT_MTU_MAX, \
                                                                CFG_BLE_NUM_LINK) \
                                                                + CFG_BLE_MBLOCK_COUNT_MARGIN)

#define BLE_DYN_ALLOC_SIZE                                  (BLE_TOTAL_BUFFER_SIZE(CFG_BLE_NUM_LINK, MBLOCK_COUNT, (CFG_BLE_EATT_BEARER_PER_LINK * CFG_BLE_NUM_LINK)))

/* Private typedef -----------------------------------------------------------*/

/**-----------------------------------------------------------------------------
 * @enum   BleEvtType
 * @brief  Enumerates all possible BLE Manager events.
 *
 * This enumeration defines the set of events that can be processed by the BLE Manager.
 * Events are triggered by different sources such as the application, BLE stack,
 * timers, or serial communication modules.
 *
 * Members:
 *   - BLE_MNGR_EVT_NONE:                   No event.
 *   - BLE_MNGR_EVT_INIT_REQ_IND:           Request to initialize the BLE stack            , BleStack_Init() needs to be executed.
 *   - BLE_MNGR_EVT_APP_RUN_PROC_IND:       Request to run BLE stack processing            , BleStack_Process() needs to be executed.
 *   - BLE_MNGR_EVT_APP_ACI_CMND_IND:       Request to execute a BLE stack command         , BleStack_Request() needs to be executed.
 *   - BLE_MNGR_EVT_TIMER_EXPIRED_IND:      Notification that a timer has expired          , BLEPLATCB_TimerExpiry() needs to be executed.
 *   - BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND: Indication of an asynchronous serial packet    , BleStack_IndicationFromController() needs to be executed .
 *---------------------------------------------------------------------------*/

typedef enum 
{
  BLE_MNGR_EVT_NONE = 0 ,
  BLE_MNGR_EVT_INIT_REQ_IND,
  /*--------- BleMngr Externally-Triggred Events (From BleSerial,BleNvm,BleTimer,BleApp)------------*/
  BLE_MNGR_EVT_APP_RUN_PROC_IND,
  BLE_MNGR_EVT_APP_ACI_CMND_IND,
  BLE_MNGR_EVT_TIMER_EXPIRED_IND,
  BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND,
  
}BleEvtType;

/**-----------------------------------------------------------------------------
 * @struct BasicEvt
 * @brief  Basic event data structure for BLE Manager events.
 *
 * This structure encapsulates the essential information for any BLE Manager event,
 * including the event type, a callback function to be invoked after event processing,
 * and a pointer to callback input data.
 *
 * Members:
 *   - Evt:      The event type (from BleEvtType).
 *   - cb:       Callback function pointer, called when event processing is complete.
 *   - CbInpPtr: Pointer to data passed to the callback function.
 *---------------------------------------------------------------------------*/

typedef struct {
  BleEvtType Evt;                  /* Evt That has Occurred */
  void(*cb)(void*);                /* Callback function to be called whenever the Event Proccessing is done */
  void* CbInpPtr;                  /* Pointer to Input to be passed to Callback */
} BasicEvt;

/**-----------------------------------------------------------------------------
 * @struct BleMngrEvtInitReqInd
 * @brief  Event Data to be sent to BleMngr Whenever BleStack_Init needs to be executed.
 *
 * This structure is used when the BLE stack needs to be initialized. It contains
 * the basic event information, a pointer to the BLE stack initialization configuration,
 * and the return status from the initialization function.
 *
 * Members:
 *   - BasicEvt:  Embedded basic event structure (Evt should be BLE_MNGR_EVT_INIT_REQ_IND).
 *   - initPtr:   Pointer to BleStack_init_t structure with BLE stack configuration.
 *   - ret:       Return value from BleStack_Init().
 *---------------------------------------------------------------------------*/

typedef struct {
  BasicEvt BasicEvt;              /* Evt That has Occurred in that case would be BLE_MNGR_EVT_APP_RUN_PROC_IND */
  BleStack_init_t* initPtr;       /* pointer to the BleStack_init_t structure containing the BleStack Configuration */
  tBleStatus ret;                 /* return value from BleStack_Init */
} BleMngrEvtInitReqInd;

/**-----------------------------------------------------------------------------
 * @struct BleMngrEvtAppRunProcInd
 * @brief  Event Data to be sent to BleMngr Whenever BleStack_Process needs to be executed .
 *
 * This structure is used when the BLE stack's main processing function, BleStack_Process() , needs to be
 * executed. It only contains the basic event information.
 *
 * Members:
 *   - BasicEvt:  Embedded basic event structure (Evt should be BLE_MNGR_EVT_APP_RUN_PROC_IND).
 *---------------------------------------------------------------------------*/

typedef struct {
  BasicEvt BasicEvt;              /* Evt That has Occurred in that case would be BLE_MNGR_EVT_APP_RUN_PROC_IND */
} BleMngrEvtAppRunProcInd;

/**-----------------------------------------------------------------------------
 * @struct BleMngrEvtAppAciCmndInd
 * @brief  Event Data to be sent to BleMngr Whenever BleStack_Request needs to be executed.
 *
 * This structure is used when a command needs to be sent to the BLE stack. It contains
 * the basic event information, input and output buffers for the command and response,
 * and the size of the response.
 *
 * Members:
 *   - BasicEvt:  Embedded basic event structure (Evt should be BLE_MNGR_EVT_APP_ACI_CMND_IND).
 *   - InpPtr:    Pointer to input buffer containing the command.
 *   - OutPtr:    Pointer to output buffer for the response.
 *   - size:      Size of the response.
 *---------------------------------------------------------------------------*/

typedef struct {
  
  BasicEvt BasicEvt;               /* Evt That has Occurred in that case would be BLE_MNGR_EVT_APP_ACI_CMND_IND */  
  void* InpPtr;                    /* Input Buffer Containg the Command , going to be passed as is to BleStack_Request */
  void* OutPtr;                    /* Output Buffer Containg the Response , going to be passed as is to BleStack_Request */
  uint16_t size;                   /* size of Response */
} BleMngrEvtAppAciCmndInd;

/**-----------------------------------------------------------------------------
 * @struct BleMngrEvtTimerExpiredInd
 * @brief  Event Data to be sent to BleMngr Whenever BLEPLATCB_TimerExpiry needs to be executed.
 *
 * This structure is used when a timer managed by the BLE platform expires. It contains
 * the basic event information and the ID of the expired timer.
 *
 * Members:
 *   - BasicEvt:  Embedded basic event structure (Evt should be BLE_MNGR_EVT_TIMER_EXPIRED_IND).
 *   - id:        ID of the expired timer , orginally passed from BLEPLAT_TimerStart .
 *---------------------------------------------------------------------------*/

typedef struct {
  BasicEvt BasicEvt;               /* Evt That has Occurred in that case would be BLE_MNGR_EVT_TIMER_EXPIRED_IND */
  uint16_t id;                     /* Expired Timer Id , going to be passed as is to BLEPLATCB_TimerExpiry */
} BleMngrEvtTimerExpiredInd;

/**-----------------------------------------------------------------------------
 * @struct BleMngrEvtSerialAsynchPcktInd
 * @brief  Event Data to be sent to BleMngr Whenever BleStack_IndicationFromController needs to be executed .
 *
 * This structure is used when an asynchronous packet is received from the BLE controller (BleSerial)
 * via the serial interface. It contains the basic event information, a pointer to the
 * received packet, and The returen value from BleStack_IndicationFromController . 
 * This function returns a non-zero value if the indication packet cannot be handled by the BLE stack..
 *
 * Members:
 *   - BasicEvt:  Embedded basic event structure (Evt should be BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND).
 *   - InpPtr:    Pointer to the received packet buffer.
 *   - KeepPckt:  Output from BleStack_IndicationFromController.
 *---------------------------------------------------------------------------*/

typedef struct {
  BasicEvt BasicEvt;               /* Evt That has Occurred in that case would be BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND */
  void* InpPtr;                    /* Input Buffer Containg the Packet Recevied , going to be passed as is to BleStack_IndicationFromController */
  uint8_t KeepPckt;                /* Output From BleStack_IndicationFromController */          
} BleMngrEvtSerialAsynchPcktInd;


/**-----------------------------------------------------------------------------
 * @struct BleMngrContext
 * @brief  BLE Manager Context Structure
 *
 * This structure encapsulates all the context and resources required by the BLE Manager,
 * including BLE stack initialization parameters, memory buffers, and FreeRTOS handles.
 * It is  instantiated once and used throughout the BLE Manager's lifetime.
 *---------------------------------------------------------------------------*/

typedef struct 
{
  /*----------------------------- BLE STACK INITIALIZATION CONTEXT ----------------------------*/
  
  /* BleStack Initilzation Context */
  BleStack_init_t pInitParams;                                              /*  BleStack init variables */
  uint32_t host_buffer[DIVC(BLE_DYN_ALLOC_SIZE, 4)];                         /*  BleStack buffers */
  uint32_t gatt_buffer[DIVC(BLE_GATT_BUF_SIZE, 4)];                          /*  BleStack buffers */
  uint16_t host_event_buffer[DIVC(BLE_STACK_EVENT_BUF_SIZE, 2)];             /*  BleStack buffers */
  uint16_t ctrl_event_fifo_buffer[DIVC(BLE_CTRL_EVENT_FIFO_BUF_SIZE,2)];     /*  BleStack buffers */
  uint8_t  host_extra_data_buffer[BLE_EXTRA_DATA_BUF_SIZE];                  /*  BleStack buffers */
  uint64_t stack_nvm_buffer[CFG_BLE_NVM_SIZE_MAX];                           /*  BleStack buffers */
  
  /*----------------------------- FreeRTOS-RELATED CONTEXT ----------------------------*/
  
  /* BleMngr FreeRTOS Context */
  TaskHandle_t BleMngrHandle;                                           /* FreeRTOS Task Handle */
  QueueHandle_t BleMngrMQHandle;                                        /* FreeRTOS Message Queue Handle */
  EventGroupHandle_t BlockedEvts;                                       /* FreeRTOS Event Group Handle */
  
  /*----------------------------- CALLBACKS-RELATED CONTEXT ----------------------------*/
  uint8_t* ResponsePtr;                                                 /*  BleStack buffer used to recevie Responses From BleController in  */
    
}BleMngrContext;

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/                                               

/* BleMngr General Context Structure */
static BleMngrContext BleMngrCntxt;

/* Global variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/


static void BleStackInit( void );
static void BleMngrEvtInd(void * Evt);
static void BleMngrProcEvt(void* pEvt);
static void BleMngr(void);
static void BleStackInitRetHandlerCb( void* retPtr );
static void BleMngrSerialAsynchPcktEvtCb( void * Ptr );

/* External variables --------------------------------------------------------*/
/* Functions Definition ------------------------------------------------------*/

/**************************************************************************************************************************************************************************************************
 *
 *                                                                                              GLOBAl FUNCTIONS
 *
 **************************************************************************************************************************************************************************************************/

/*===============================================================================================================================================================================================*/

/*------------------------------------------------------------------------------------- INITIALIZATION / DE-INITIALIZATION -----------------------------------------------------------------------*/

/*===============================================================================================================================================================================================*/

/**
 * @brief
 *   Initialize the BLE Manager Subsystem and All Related Platform Components
 *
 * @details
 *   This function performs the complete initialization of the BLE subsystem, including platform-specific
 *   components, serial interface, timers, FreeRTOS resources, and the BLE stack itself. It ensures that
 *   all required resources are allocated and initialized before BLE operations begin.
 *
 *   Initialization steps:
 *     - Initializes the BLE Port Debug Subsystem to allow usage of Debug Hooks.
 *     - Initializes the BLE non-volatile memory (NVM) subsystem using the provided buffer and size.
 *     - Initializes the BLE serial communication interface with BleController .
 *     - Initializes the BLE timer subsystem.
 *     - Creates a FreeRTOS message queue (BLE_MNGR_MQ_HANDLE) for BLE stack event processing.
 *     - Creates the BLE Manager task (BleMngr) with the specified stack size and priority.
 *     - Checks for successful allocation of the message queue and task handle; logs an error and jumps to error handling if allocation fails.
 *     - Creates a FreeRTOS event group (BlockedEvts) for task synchronization (e.g., blocking/unblocking BLE Manager).
 *     - Checks for successful allocation of the event group; logs an error and jumps to error handling if allocation fails.
 *     - Initializes the BLE stack and platform dependencies (NVM, timers, etc.) by calling BleMngrInit().
 *     - Checks for successful BLE stack initialization; logs an error and jumps to error handling if initialization fails.
 *     - If any step fails, calls BleDeInit() to clean up and de-initialize the BLE subsystem.
 *
 *   Usage:
 *     - This function should be called once during system startup before any BLE operations are performed.
 *     - Ensures that all required platform and stack resources are ready for BLE communication and management.
 *
 */

void BleMngrInit( void )
{          
  /* Initializes the BLE Port Debug Subsystem to allow usage of Debug Hooks */
  BlePort_DebugInit();
  
  /* start BLE - related platform aspects */
  BleNvm_Init(BleMngrCntxt.stack_nvm_buffer, CFG_BLE_NVM_SIZE_MAX );
  BleSerial_Init();
  // NOTE: BleTimer is based on FreeRTOS Software Timer , hence there is no BleTimer_Init to call
  
  /*create a message queue for BleMngr Task */
  BLE_MNGR_MQ_HANDLE = xQueueCreate((UBaseType_t)BLE_MNGR_MQ_LEN,(UBaseType_t)BLE_MNGR_MQ_MESSAGE_LEN);
  
  /* Register BLE Stack tasks */
  (void)xTaskCreate((TaskFunction_t)BleMngr,
                    "BleMngr",
                    BLE_MNGR_STACK_SIZE,
                    NULL,
                    BLE_MNGR_PRIO,
                    &BLE_MNGR_TASK_HANDLE);

  if((NULL == BLE_MNGR_MQ_HANDLE ) || (NULL == BLE_MNGR_TASK_HANDLE) )
  {
    LogInfo(" BleMngr Task Allocations Failed !! ");
    goto ERR ;
  }
  
  /* create an event group to block BleMngr on */
  BleMngrCntxt.BlockedEvts = xEventGroupCreate();
  if( NULL == BleMngrCntxt.BlockedEvts )
  {
    LogInfo(" BleMngr Event Group Allocations Failed !! ");
    goto ERR ;
  }
  
  /* Initialize the BLE Stack */
   BleStackInit();
  
  return;
ERR :     
    BleMngrDeInit();
}


/**
  * @brief  DeInit BleMngr.
  * @param  void  
  * @retval void
  */

void BleMngrDeInit(void)
{
  BleErrHandler();
}

/**
 * @brief
 *   BLE Error Handler (Weak Default Implementation)
 *
 * @details
 *   This function is called whenever a critical error or unexpected condition occurs in the BLE. 
 *   The default implementation is marked as __weak, allowing
 *   the user to override it with a custom error handler elsewhere in the application.
 *
 *   - The default behavior is to trigger a FreeRTOS assertion failure using configASSERT(0),
 *     which  halts the system or enters a debug state, depending on the configuration.
 *   - This function can be overridden to provide custom error handling, such as logging,
 *     system reset, or recovery actions.
 */

__WEAK void BleErrHandler( void )
{
  configASSERT( 0 );
}

/*===============================================================================================================================================================================================*/

/*------------------------------------------------------------------------------------- Blocking Utilities For BleMngr -----------------------------------------------------------------------*/

/*===============================================================================================================================================================================================*/

/**
 * @brief
 *   Block the BLE Manager
 *
 * @details
 *   This function blocks the BleMngr by waiting for the BLE_MNGR_EVT_BLOCK bit to be set Later. 
 *
 *   - Uses xEventGroupWaitBits to wait for the BLE_MNGR_EVT_BLOCK bit:
 *       - Waits for the specified bit (BLE_MNGR_EVT_BLOCK) to be set in the event group.
 *       - Clears the bit automatically upon exit (pdTRUE).
 *       - Waits for any matching bit (pdFALSE for "wait for all bits").
 *       - Uses a timeout specified by BLE_MNGR_BLOCK_TIM_MS (converted to ticks).
 *   - If the bit is not set within the timeout period:
 *       - Logs a timeout error message.
 *       - Invokes the error handler (BleErrHandler) to handle the failure.
 *
 *   Usage:
 *     - This function is called when a BleMngr needs to block, waiting for
 *       a response from the BLE controller before proceeding.
 *     - The corresponding unblock is performed by BleMngrUnBlock().
 *     - Check BleSerialRxFsm , RspRx : RxReq Event Handling , for more inforamtion.
 *
 * @note
 *   - The event group and bit mask (BLE_MNGR_EVT_BLOCK) must be properly initialized before use.
 *   - Proper error handling is implemented for timeout conditions.
 *   - This function should only be called from a task context (not from an ISR).
 */

void BleMngrBlock( void )
{
  EventBits_t EvtBits = 0 ;
  
  /* block over the reception */
  EvtBits = xEventGroupWaitBits(BleMngrCntxt.BlockedEvts,
                                  BLE_MNGR_EVT_BLOCK,
                                  pdTRUE,
                                  pdFALSE,
                                  pdMS_TO_TICKS(BLE_MNGR_BLOCK_TIM_MS));

  if (!( EvtBits & BLE_MNGR_EVT_BLOCK ))
  {
    LogInfo(" BleMngrBlock Timeout !! ");
    BleErrHandler(); 
  }
}

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
 *   - If called from an interrupt context (as determined by xPortIsInsideInterrupt()):
 *       - Uses xEventGroupSetBitsFromISR to safely set the event group bit from an ISR.
 *       - Checks if setting the bit has unblocked a higher-priority task (xHigherPriorityTaskWoken).
 *         If so, calls portYIELD_FROM_ISR to request a context switch, ensuring prompt task execution.
 *
 *   - If called from a normal task context:
 *       - Uses xEventGroupSetBits to set the event group bit.
 *
 *   Usage:
 *     - This function is called to unblock BleMngr Waiting on a Reponse From BleController over previously transmitted Command ,
       - check BLECB_RequestToController for more information.
 *
 * @note
 *   - Ensures correct FreeRTOS API usage for both ISR and task contexts.
 *   - The event group and bit mask (BLE_MNGR_EVT_BLOCK) must be properly initialized before use.
 *   - No return value; errors are handled by the FreeRTOS API.
 */

void BleMngrUnBlock( void )
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xPortIsInsideInterrupt())
  {
    xEventGroupSetBitsFromISR(BleMngrCntxt.BlockedEvts,
                              BLE_MNGR_EVT_BLOCK,
                              &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
  else xEventGroupSetBits(BleMngrCntxt.BlockedEvts, BLE_MNGR_EVT_BLOCK);
}

/*===============================================================================================================================================================================================*/

/*------------------------------------------------------------------------------------- EVENT-INDICATION For BleMngr -----------------------------------------------------------------------*/

/*===============================================================================================================================================================================================*/

/* BLE_MNGR_EVT_INIT_REQ_IND */

/**
 * @brief  Indicate to BLE Manager that BleStack_Init needs to be executed .. Indicate to BleMngr that BLE_MNGR_EVT_INIT_REQ_IND has to processed !.
 *
 * This function allocates and initializes a BLE Manager event structure for the
 * initialization request indication (BLE_MNGR_EVT_INIT_REQ_IND). It sets up the
 * event type, callback, and initialization pointer, and then dispatches the event
 * to the BLE Manager event handler.
 *
 * @param[in]  initPtr  Pointer to the BLE stack initialization structure.
 * @param[in]  cb       Callback function to be called upon event completion.
 *
 * @note
 * - The 'ret' field is initialized to 0 and is expected to be filled by BleStack_init.
 */

void BleMngr_InitReqEvtInd( BleStack_init_t* initPtr , void(*cb)(void*) )
{
  BleMngrEvtInitReqInd* pEvt;
  BLE_MNGR_MQ_MESSAGE_TYPEDEF Evt;
  memset(&Evt , 0 , BLE_MNGR_MQ_MESSAGE_LEN);
  pEvt               = (BleMngrEvtInitReqInd*)&Evt;
  pEvt->BasicEvt.Evt = BLE_MNGR_EVT_INIT_REQ_IND;
  pEvt->BasicEvt.cb  = cb;
  pEvt->initPtr      = initPtr;
  pEvt->ret          = 0; /*Will be filled by BleStack_init */
  BleMngrEvtInd(pEvt);
  return;
}

/*BLE_MNGR_EVT_APP_RUN_PROC_IND*/ 

/**
 * @brief
 *   Indicate a "Request to Run BleStack_Process()" Event to the BLE Manager .. Indicate to BleMngr that BLE_MNGR_EVT_APP_RUN_PROC_IND has to processed !.
 *
 * @details
 *   This function creates and posts a BLE Manager event to request the BLE Manager task to run
 *   BleStack_Process(). It allocates an event structure, initializes
 *   its fields with the event type and an optional callback, and posts it to the BLE Manager's
 *   event queue for processing in the BLE Manager task context.
 *
 *   Steps:
 *         - Sets the event type to BLE_MNGR_EVT_APP_RUN_PROC_IND.
 *         - Assigns the provided callback function pointer (cb) to the event.
 *         - Posts the event to the BLE Manager's event queue using BleMngrEvtInd.
 *
 *   Usage:
 *     - This function can be called by application to request that the BLE Manager
 *       task run BleStack_Process() to process any "aci_XXXX" Commands (ex : aci_gatt_update_char_value , aci_gap_pass_key_resp .. etc) issued by the application , 
 *       with optional notification via callback.
 *
 * @param cb
 *   Callback function pointer to be called after the event is processed. The callback receives a void*
 *   argument, which can be used to pass status or result information.
 *
 * @return
 *   void
 *
 */

void BleMngr_AppRunProcEvtInd( void(*cb)(void*) )
{
  BleMngrEvtAppRunProcInd* pEvt;
  BLE_MNGR_MQ_MESSAGE_TYPEDEF Evt;
  memset(&Evt , 0 , BLE_MNGR_MQ_MESSAGE_LEN);
  pEvt               = (BleMngrEvtAppRunProcInd*)&Evt;
  pEvt->BasicEvt.Evt = BLE_MNGR_EVT_APP_RUN_PROC_IND;
  pEvt->BasicEvt.cb  = cb;  
  BleMngrEvtInd(pEvt);
  return;
}

/*--------------------------------------------------------------------------*/

/*BLE_MNGR_EVT_APP_ACI_CMND_IND*/

/**
 * @brief
 *   Indicate an Application ACI Command Event to the BLE Manager , Indicate to BleMngr that BLE_MNGR_EVT_APP_ACI_CMND_IND has to processed !
 *
 * @details
 *   This function creates and posts a BLE Manager event for an Application Controller Interface (ACI)
 *   command request. It allocates an event structure, initializes its fields with the
 *   input and output pointers, and an optional callback, and posts it to the BLE Manager's event queue
 *   for processing in the BLE Manager task context.
 *
 *   Steps:
 *         - Sets the event type to BLE_MNGR_EVT_APP_ACI_CMND_IND.
 *         - Assigns the provided callback function pointer (cb) to the event.
 *         - Sets the input pointer (InpPtr) and output pointer (OutPtr) in the event structure.
 *         - Initializes the size field to 0 (to be filled by BleStack_Request during processing).
 *         - Posts the event to the BLE Manager's event queue using BleMngrEvtInd.
 *
 *   Usage:
 *     - This function is called by application code ( Usually TransparentMode Application ) when an ACI command needs to be sent to
 *       the BLE stack and the result is to be processed asynchronously.
 *     - The callback function can be used for notification or further processing once the event is handled.
 *
 * @param InpPtr
 *   Pointer to the input data for the ACI command.
 *
 * @param OutPtr
 *   Pointer to the output buffer where the response/result will be stored.
 *
 * @param cb
 *   Callback function pointer to be called after the event is processed. The callback receives a void*
 *   argument, which can be used to pass status or result information.
 *
 * @return
 *   void
 *
 */

void BleMngr_AppAciCmndEvtInd( void* InpPtr , void* OutPtr , void(*cb)(void*) )
{
  BleMngrEvtAppAciCmndInd* pEvt;
  BLE_MNGR_MQ_MESSAGE_TYPEDEF Evt;
  memset(&Evt , 0 , BLE_MNGR_MQ_MESSAGE_LEN);
  pEvt               = (BleMngrEvtAppAciCmndInd*)&Evt;
  pEvt->BasicEvt.Evt = BLE_MNGR_EVT_APP_ACI_CMND_IND;
  pEvt->BasicEvt.cb  = cb;
  pEvt->InpPtr       = InpPtr;
  pEvt->OutPtr       = OutPtr;
  pEvt->size         = 0; /*Will be filled by BleStack_request */
  BleMngrEvtInd(pEvt);
  return;
}

/*--------------------------------------------------------------------------*/

/* BLE_MNGR_EVT_TIMER_EXPIRED_IND */

/**
 * @brief
 *   Indicate a Timer Expiry Event to the BLE Manager , Indicate to BleMngr that BLE_MNGR_EVT_TIMER_EXPIRED_IND has to processed ! 
 *
 * @details
 *   This function creates and posts a BLE Manager event indicating that a timer has expired.
 *   It allocates an event structure, initializes its fields with the timer ID and
 *   an optional callback, and posts it to the BLE Manager's event queue for processing in the
 *   BLE Manager task context.
 *
 *   Steps:
 *         - Sets the event type to BLE_MNGR_EVT_TIMER_EXPIRED_IND.
 *         - Assigns the provided callback function pointer (cb) to the event.
 *         - Sets the timer ID (id) in the event structure.
 *         - Posts the event to the BLE Manager's event queue using BleMngrEvtInd.
 *
 *   Usage:
 *     - This function is called by timer management code or timer callbacks when a BLE-related
 *       timer expires and the BLE stack (BleMngr) to be notified.
 *     - The callback function allows for notification or further processing once the event is handled.
 *
 * @param id
 *   The identifier of the expired timer.
 *
 * @param cb
 *   Callback function pointer to be called after the event is processed. The callback receives a void*
 *   argument, which can be used to pass status or result information.
 *
 * @return
 *   void
 *
 */

void BleMngr_TimerExpiredEvtInd( uint32_t id , void(*cb)(void*) )
{
  BleMngrEvtTimerExpiredInd* pEvt;
  BLE_MNGR_MQ_MESSAGE_TYPEDEF Evt;
  memset(&Evt , 0 , BLE_MNGR_MQ_MESSAGE_LEN);
  pEvt               = (BleMngrEvtTimerExpiredInd*)&Evt;
  pEvt->BasicEvt.Evt = BLE_MNGR_EVT_TIMER_EXPIRED_IND;
  pEvt->BasicEvt.cb  = cb;
  pEvt->id           = id;
  BleMngrEvtInd(pEvt);
  return;
}

/*--------------------------------------------------------------------------*/

/* BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND */

/**
 * @brief
 *   Indicate a Serial Asynchronous Packet Event to the BLE Manager , Indicate to BleMngr that BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND has to processed !
 *
 * @details
 *   This function creates and posts a BLE Manager event for an asynchronous serial packet received
 *   from the BLE controller. It allocates an event structure, initializes its fields,
 *   and posts it to the BLE Manager's event queue for processing in the BLE Manager task context.
 *   A callback function can be specified, which will be invoked after the event is processed.
 *
 *   Steps:
 *         - Sets the event type to BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND.
 *         - Assigns the provided callback function pointer (cb) to the event.
 *         - Sets the input pointer (InpPtr) to the provided data.
 *         - Initializes KeepPckt to 0 (default). KeepPckt will be used to save output From BleStack_IndicationFromController
 *         - Posts the event to the BLE Manager's event queue using BleMngrEvtInd.
 *
 *   Usage:
 *     - This function is called by the BleSerial when an asynchronous
 *       packet is received and needs to be processed by the BLE stack.
 *     - The callback function allows for notification or further processing once the event is handled.
 *
 * @param InpPtr
 *   Pointer to the input data (e.g., the received packet buffer) to be associated with the event.
 *
 * @param cb
 *   Callback function pointer to be called after the event is processed. The callback receives a void*
 *   argument, which can be used to pass "KeepPckt" information to BleSerial .
 *
 * @return
 *   void
 *
 */

void BleMngr_SerialAsynchPcktEvtInd( void* InpPtr , void(*cb)(void*))
{
  BleMngrEvtSerialAsynchPcktInd* pEvt;
  BLE_MNGR_MQ_MESSAGE_TYPEDEF Evt;
  memset(&Evt , 0 , BLE_MNGR_MQ_MESSAGE_LEN);
  pEvt               = (BleMngrEvtSerialAsynchPcktInd*)&Evt;
  pEvt->BasicEvt.Evt = BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND;
  pEvt->BasicEvt.cb  = cb;
  pEvt->InpPtr       = InpPtr;
  pEvt->KeepPckt     = 0 ;
  BleMngrEvtInd(pEvt);
  return;
}

/**************************************************************************************************************************************************************************************************
 *
 *                                                                                            LOCAL FUNCTIONS
 *
 **************************************************************************************************************************************************************************************************/

/**
 * @brief  Callback handler for BLE stack initialization return status.
 *
 * This function is intended to be used as a callback to handle the return value
 * from the BLE stack initialization process. It checks the status pointed to by
 * retPtr, and if the initialization did not succeed (i.e., the status is not
 * BLE_STATUS_SUCCESS), it calls the error handler.
 *
 * @param[in] retPtr  Pointer to a tBleStatus value returned by the BLE stack
 *                    initialization function , BleStack_Init().
 *
 * @note
 * - If the initialization fails, BleErrHandler() is invoked.
 * - The function does nothing if initialization is successful.
 */

static void BleStackInitRetHandlerCb ( void* retPtr )
{
   tBleStatus ret = *(tBleStatus *)retPtr ;
   /* if BleStack_Init return value is not BLE_STATUS_SUCCESS , call BleErrHandler */
   if ( ret != BLE_STATUS_SUCCESS ) BleErrHandler();
   
   return;
}

/**
 * @brief Callback for asynchronous packet event completion.
 * 
 * @details
 *   Invoked after BleStack_IndicationFromController completes .
 *   If the BLE stack could not process the event (non-zero return), logs error and triggers error handler.
 * @param Ptr Pointer to return value from BleStack_IndicationFromController 
 */

static void BleMngrSerialAsynchPcktEvtCb( void * Ptr )
{
  if ( *((uint8_t*)Ptr)  != 0 ) /* return value From BleStack_IndicationFromController */ 
  {
    /* BleStack Can't Processes The Event Currently , Error !! */
    LogInfo(" BleMngrAsynchPcktEvtCb : ret != 0 !! ");
    BleErrHandler(); 
  }
}

/**
 * @brief
 *   Initialize the BLE Stack Parameters
 *
 * @details
 *   This function prepares and initializes the BLE stack by configuring all required memory buffers,
 *   parameters, and options. It fills the BLE stack initialization structure with pointers to
 *   statically allocated buffers, sets up GATT and COC (Connection-Oriented Channel) parameters,
 *   and applies configuration constants. After all parameters are set, it calls the BLE stack
 *   initialization function to start the BLE stack.
 *
 *   Initialization steps:
 *     - Clears the BLE stack initialization structure to ensure all fields are set to default (zero).
 *     - Sets up main BLE stack configuration:
 *         - Assigns memory buffers for stack RAM, event FIFOs, NVM cache, control events, and extra data.
 *         - Sets buffer sizes and counts using configuration macros and context structure members.
 *     - Configures GATT (Generic Attribute Profile) parameters:
 *         - Assigns GATT buffer, sets attribute and service counts, MTU, and other GATT-specific sizes.
 *     - Configures COC (Connection-Oriented Channel) parameters:
 *         - Sets maximum numbers and sizes for COC connections and initiators.
 *     - Sets additional options and disables debug by default.
 *     - Provides user code sections for custom parameter adjustments if needed.
 *     - Calls the BLE stack initialization function (`BleStack_Init`) with the filled parameter structure.
 *     - Returns the status code from the BLE stack initialization as a uint8_t.
 *
 * @return
 *   void
 * 
 *
 * @note
 *   - This function must be called before any BLE operations are performed.
 *   - All buffer pointers and sizes must be valid and properly allocated before calling this function.
 */

static void BleStackInit( void )
{    
  /* Default stack init: all 0 */
  memset( BLE_STACK_INIT_PARAM_PTR, 0, sizeof(BleStack_init_t) );
  
  /* Set main configuration */  
  BLE_STACK_INIT_PARAM_PTR->bleStartRamAddress          = (uint8_t*)(BleMngrCntxt.host_buffer);
  BLE_STACK_INIT_PARAM_PTR->total_buffer_size           = BLE_DYN_ALLOC_SIZE;
  BLE_STACK_INIT_PARAM_PTR->host_event_fifo_buffer      = BleMngrCntxt.host_event_buffer;
  BLE_STACK_INIT_PARAM_PTR->host_event_fifo_buffer_size = sizeof(BleMngrCntxt.host_event_buffer)/sizeof(BleMngrCntxt.host_event_buffer[0]);
  BLE_STACK_INIT_PARAM_PTR->nvm_cache_buffer            = BleMngrCntxt.stack_nvm_buffer;
  BLE_STACK_INIT_PARAM_PTR->nvm_cache_max_size          = CFG_BLE_NVM_SIZE_MAX;
  BLE_STACK_INIT_PARAM_PTR->nvm_cache_size              = CFG_BLE_NVM_SIZE_MAX - 1;
  BLE_STACK_INIT_PARAM_PTR->ctrl_event_fifo_buffer      = BleMngrCntxt.ctrl_event_fifo_buffer ; 
  BLE_STACK_INIT_PARAM_PTR->ctrl_event_fifo_buffer_size = sizeof(BleMngrCntxt.ctrl_event_fifo_buffer)/sizeof(BleMngrCntxt.ctrl_event_fifo_buffer[0]);
  BLE_STACK_INIT_PARAM_PTR->extra_data_buffer           = BleMngrCntxt.host_extra_data_buffer;
  BLE_STACK_INIT_PARAM_PTR->extra_data_buffer_size      = BLE_EXTRA_DATA_BUF_SIZE;
  BLE_STACK_INIT_PARAM_PTR->numOfLinks                  = CFG_BLE_NUM_LINK;
  BLE_STACK_INIT_PARAM_PTR->mblockCount                 = CFG_BLE_MBLOCK_COUNT;
  
  /* Set GATT configuration */
  BLE_STACK_INIT_PARAM_PTR->bleStartRamAddress_GATT     = (uint8_t*)(BleMngrCntxt.gatt_buffer);
  BLE_STACK_INIT_PARAM_PTR->total_buffer_size_GATT      = BLE_GATT_BUF_SIZE;
  BLE_STACK_INIT_PARAM_PTR->numAttrRecord               = CFG_BLE_NUM_GATT_ATTRIBUTES;
  BLE_STACK_INIT_PARAM_PTR->numAttrServ                 = CFG_BLE_NUM_GATT_SERVICES;
  BLE_STACK_INIT_PARAM_PTR->attrValueArrSize            = CFG_BLE_ATT_VALUE_ARRAY_SIZE;
  BLE_STACK_INIT_PARAM_PTR->prWriteListSize             = CFG_BLE_ATTR_PREPARE_WRITE_VALUE_SIZE;
  BLE_STACK_INIT_PARAM_PTR->attMtu                      = CFG_BLE_ATT_MTU_MAX;
  BLE_STACK_INIT_PARAM_PTR->max_add_eatt_bearers        = CFG_BLE_EATT_BEARER_PER_LINK * CFG_BLE_NUM_LINK;
  BLE_STACK_INIT_PARAM_PTR->gatt_long_write_buffer      = BleMngrCntxt.host_extra_data_buffer;
  
  /* Set COC configuration */
  BLE_STACK_INIT_PARAM_PTR->max_coc_nbr                 = CFG_BLE_COC_NBR_MAX;
  BLE_STACK_INIT_PARAM_PTR->max_coc_mps                 = CFG_BLE_COC_MPS_MAX;
  BLE_STACK_INIT_PARAM_PTR->max_coc_initiator_nbr       = CFG_BLE_COC_INITIATOR_NBR_MAX;
  
  /* Set options and debug */
  BLE_STACK_INIT_PARAM_PTR->options                     = CFG_BLE_OPTIONS;
  BLE_STACK_INIT_PARAM_PTR->debug                       = 0U;
    
  /* Indicate the BleMngr Task that a BleStack_Init() needs to be executed ! */ 
  BleMngr_InitReqEvtInd( BLE_STACK_INIT_PARAM_PTR , BleStackInitRetHandlerCb );
  
  return ;
}

/**
 * @brief
 *   Indicate (Post) an Event to the BLE Manager Task
 *
 * @details
 *   This function posts an event to the BLE Manager's message queue, allowing the BLE Manager
 *   task to process the event in its main execution context. It supports both interrupt and non-interrupt
 *   (task) contexts, ensuring safe and efficient event delivery regardless of where the event is generated.
 *
 *   - If called from an interrupt context (as determined by xPortIsInsideInterrupt()):
 *       - Uses xQueueSendFromISR to safely post the event to the queue.
 *       - Checks if posting the event has unblocked a higher-priority task (xHigherPriorityTaskWoken).
 *         If so, calls portYIELD_FROM_ISR to request a context switch, ensuring prompt task execution.
 *
 *   - If called from a normal task context:
 *       - Uses xQueueSend to post the event to the queue without waiting (timeout = 0).
 *       - If the queue is full or the send fails, logs an error and invokes the error handler (BleErrHandler).
 *
 * @param Evt
 *   the event structure to be posted to the BLE Manager's message queue.
 *
 * @return
 *   void
 *
 * @note
 *   - This function is a key part of the event-driven architecture for BLE management.
 */
static void BleMngrEvtInd(void * Evt)
{  
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xPortIsInsideInterrupt())
  {
    if(pdFALSE == xQueueSendFromISR(BLE_MNGR_MQ_HANDLE,
                      Evt, 
                      &xHigherPriorityTaskWoken))
    {
      LogInfo(" BleMngrEvtInd is Broken !! ");
      BleErrHandler();    
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
  else
  {
    if(pdFALSE == xQueueSend( BLE_MNGR_MQ_HANDLE,
                              Evt,
                              0 /*xTicksToWait*/ ))
    {
      LogInfo(" BleMngrEvtInd is Broken !! ");
      BleErrHandler();    
    }
  }
}

/**
 * @brief
 *   BLE Manager Event Processing Function
 *
 * @details
 *   This function processes a single event received by the BLE Manager. It determines the event type,
 *   casts the event pointer to the appropriate structure, and performs the required action for each
 *   supported event type. It also prepares the callback input argument for each event, which will be
 *   used by the BLE Manager's main processing loop to invoke any registered callback.
 *
 *   Event Handling:
 *    
 *     - BLE_MNGR_EVT_INIT_REQ_IND:
 *         - Casts the event pointer to BleMngrEvtInitReqInd.
 *         - Calls BleStack_Init with pointer to the (BleStack_init_t) structure.
 *         - Sets the callback input pointer (CbInpPtr) to the address of the result return value .
 *     
 *     - BLE_MNGR_EVT_APP_RUN_PROC_IND:
 *         - Casts the event pointer to BleMngrEvtAppRunProcInd.
 *         - Sets the callback input pointer (CbInpPtr) to 0 (no argument).
 *
 *     - BLE_MNGR_EVT_APP_ACI_CMND_IND:
 *         - Casts the event pointer to BleMngrEvtAppAciCmndInd.
 *         - Calls BleStack_Request with the input and output pointers.
 *         - Stores the result size in the event structure.
 *         - Sets the callback input pointer (CbInpPtr) to the address of the result size.
 *
 *     - BLE_MNGR_EVT_TIMER_EXPIRED_IND:
 *         - Casts the event pointer to BleMngrEvtTimerExpiredInd.
 *         - Calls BLEPLATCB_TimerExpiry with the timer ID.
 *         - Sets the callback input pointer (CbInpPtr) to 0 (no argument).
 *
 *     - BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND:
 *         - Casts the event pointer to BleMngrEvtSerialAsynchPcktInd.
 *         - Calls BleStack_IndicationFromController with the input pointer.
 *         - Stores the result (KeepPckt) in the event structure.
 *         - Sets the callback input pointer (CbInpPtr) to the address of KeepPckt.
 *
 *     - Default:
 *         - Logs an error message and invokes the error handler if an unknown event type is encountered.
 *
 *   Usage:
 *     - This function is called by the BLE Manager's main event loop for each event dequeued from the
 *       message queue.
 *     - It ensures that each event is handled according to its type and that the callback input argument
 *       is set appropriately for later use.
 *
 * @param pEvt
 *   Pointer to the event structure (must be cast to the correct type based on the event).
 *
 * @return
 *   void
 *
 * @note
 *   - Proper casting of the event pointer is essential for correct operation.
 *   - The function assumes that all event structures begin with a BasicEvt member.
 */

static void BleMngrProcEvt(void* pEvt)
{
   BleEvtType Evt  = ((BasicEvt *)pEvt)->Evt ;
   switch(Evt)
   {
     /* =====================================================  BLE_MNGR_EVT_INIT_REQ_IND EventHandling ====================================== */
     case BLE_MNGR_EVT_INIT_REQ_IND :
     {
       /* cast the recevied Event to correct Event Format */
       BleMngrEvtInitReqInd* Ind = (BleMngrEvtInitReqInd *)pEvt;
       /* call BleStack's API */
       BLE_MNGR_BLESTACK_INIT(Ind->ret,((BleStack_init_t*)(Ind->initPtr)));
       /* Input Argument to be passed to Callback Function */
       Ind->BasicEvt.CbInpPtr = (void*)&(Ind->ret) ;
       break;
     }     
     /* =====================================================  BLE_MNGR_EVT_APP_RUN_PROC_IND EventHandling ====================================== */
     case BLE_MNGR_EVT_APP_RUN_PROC_IND :
     {
       /* cast the recevied Event to correct Event Format */
       BleMngrEvtAppRunProcInd* Ind = (BleMngrEvtAppRunProcInd *)pEvt;
       /* Input Argument to be passed to Callback Function */
       Ind->BasicEvt.CbInpPtr = 0 ;
       break;
     }
     /* =====================================================  BLE_MNGR_EVT_APP_ACI_CMND_IND EventHandling ====================================== */
     case BLE_MNGR_EVT_APP_ACI_CMND_IND :
     {
       /* cast the recevied Event to correct Event Format */
       BleMngrEvtAppAciCmndInd* Ind = (BleMngrEvtAppAciCmndInd *)pEvt;
       /* call BleStack's API */
       BLE_MNGR_BLESTACK_REQUEST(Ind->size,Ind->InpPtr, Ind->OutPtr);
       /* Input Argument to be passed to Callback Function */
       Ind->BasicEvt.CbInpPtr = (void*)&(Ind->size);
       break;
     }
     /* =====================================================  BLE_MNGR_EVT_TIMER_EXPIRED_IND EventHandling ========================================= */ 
     case BLE_MNGR_EVT_TIMER_EXPIRED_IND :
     {
       /* cast the recevied Event to correct Event Format */
       BleMngrEvtTimerExpiredInd* Ind = (BleMngrEvtTimerExpiredInd *)pEvt;
       /* call BleStack's API */ 
       BLE_MNGR_BLEPLATCB_TIMEREXPIRY(Ind->id);
       /* Input Argument to be passed to Callback Function */
       Ind->BasicEvt.CbInpPtr = 0 ;
       break;
     }   
     /* =====================================================  BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND EventHandling ========================================= */ 
     case BLE_MNGR_EVT_SERIAL_ASYNCH_PCKT_IND :
     {
       /* cast the recevied Event to correct Event Format */
       BleMngrEvtSerialAsynchPcktInd* Ind = (BleMngrEvtSerialAsynchPcktInd *)pEvt;
       /* call BleStack's API */
       BLE_MNGR_BLESTACK_INDICATIONFROMCONTROLLER(Ind->KeepPckt,Ind->InpPtr);
       vPortFree(Ind->InpPtr);
       Ind->InpPtr = NULL;
       /* Input Argument to be passed to Callback Function */
       Ind->BasicEvt.CbInpPtr = (void*)&(Ind->KeepPckt);
       break;
     }        
     /* =====================================================  Default EventHandling ========================================= */ 
      default:
      {
        LogInfo(" BleMngrProcEvt is Broken !! ");
        BleErrHandler();
        break;   
      }
   }
  
  return ;
}

/**
 * @brief
 *   BLE Manager Main Event Processing Loop
 *
 * @details
 *   This function implements the main event loop for the BLE Manager task. It continuously waits for
 *   events to arrive in the BLE Manager's message queue and processes them in a thread-safe, event-driven
 *   manner. The loop ensures that all BLE-related events are handled in order, and that the BLE stack
 *   is invoked as needed for further processing.
 *
 *   Event Processing Steps (per event):
 *     1. Waits indefinitely (portMAX_DELAY) for an event pointer (pEvt) to be received from the
 *        BLE Manager message queue (BLE_MNGR_MQ_HANDLE) using xQueueReceive.
 *     2. Processes the event by calling BleMngrProcEvt(pEvt), which handles the event-specific logic.
 *     3. Invokes BleStack_Process() to process any pending BLE stack operations. If the return value is 0x00,
 *        it posts an event to rerun BleStack_Process by calling BleMngr_AppRunProcEvtInd(NULL).
 *     4. If the event structure (assumed to be of type BasicEvt) specifies a callback function (cb),
 *        the callback is invoked with the provided input argument (CbInpPtr).
 *
 *   - The loop is designed to run perpetually as long as events are successfully received from the queue.
 *   - If the loop is ever exited (i.e., xQueueReceive fails or the queue is deleted), this indicates a
 *     critical error or system misconfiguration. In such a case, an error message is logged and the
 *     generic BLE error handler (BleErrHandler) is invoked.
 *
 * @note
 *   - This function is executed as the main function of a dedicated FreeRTOS task for BLE management.
 *   - Ensures serialized, in-order, and safe processing of BLE events and stack operations.
 */

static void BleMngr(void)
{
  void* pEvt = pvPortMalloc(BLE_MNGR_MQ_MESSAGE_LEN);
  if(pEvt)
  {
    while(pdTRUE == xQueueReceive(BLE_MNGR_MQ_HANDLE,pEvt, portMAX_DELAY/*xTicksToWait*/))
    {       
      /* 1) Do Event Processing */
      BleMngrProcEvt(pEvt);
      /* 2) Run BleStack_Proces , post event to rerun BleStack_Process if ret is 0 */
      BLE_MNGR_BLESTACK_PROCESS();      
      /* 3) Call callback function Specified by Event and Pass it it the Input Argument */
      if(((BasicEvt*)pEvt)->cb) ((BasicEvt*)pEvt)->cb( ((BasicEvt*)pEvt)->CbInpPtr );
      /* 4) reset message buffer */
      memset(pEvt , 0 , BLE_MNGR_MQ_MESSAGE_LEN);
    }
  }
  /* Loop is Broken , Error */
  LogInfo(" BleMngr Loop is Broken !! ");
  if(pEvt) vPortFree(pEvt);
  BleErrHandler();
}

/**************************************************************************************************************************************************************************************************
 *
 *                                                                                              GLOBAl CALLBACKS FUNCTIONS
 *
 **************************************************************************************************************************************************************************************************/

/**
 * @brief
 * Callback called by the BLE stack (from BleStack_Process() context)
 * to send a request to the Controller. The request is a BLE standard packet
 * that can be either an ACI/HCI command or an ACL data.
 * The packet data is obtained by the pointer "data" and "length" in bytes.
 * If a response is expected ("response_flag" != 0), the response data must be
 * returned via "data" that must be filled with a standard BLE response event
 * packet. In this case, the function is blocking and the result must always
 * be a success.
 *
 * @details
 *   This function sends a data buffer to the BLE controller over the serial interface
 *   and, if requested, blocks until a response is received. It supports both synchronous (with response)
 *   and asynchronous (no response) request modes.
 *
 *   - If the response_flag is set (non-zero):
 *       - The function blocks until a response is received from the controller.
 *       - The response is written back into the provided data buffer.
 *       - Always returns BLE_STATUS_SUCCESS (as required by BLE stack).
 *
 *   - If the response_flag is not set (zero):
 *       - The function sends the data and returns immediately.
 *       - If transmission fails, returns BLE_STATUS_FAILED to allow BLE stack to retry.
 *
 *   - Always returns BLE_STATUS_SUCCESS to indicate the request was processed.
 *
 * @param data
 *   Pointer to the data buffer to send a command to the BleController ( if required , it's same data buffer to receive response into ).
 *
 * @param length
 *   Length of the data buffer in bytes.
 *
 * @param response_flag
 *   If non-zero, the function will block and wait for a response from the controller.
 *   If zero, the function will send the data and return immediately.
 *
 * @return
 *   uint8_t
 *     - BLE_STATUS_SUCCESS: The request was sent (and response received if requested).
 *     - BLE_STATUS_FAILED:  The request failed (only in async mode).
 */

uint8_t BLECB_RequestToController( uint8_t* data,
                                          uint16_t length,
                                          uint8_t response_flag )
{  
  /* hook to track entry For Debugging */
  BlePort_DebugBleCBRequestToControllerEntryHook();
  
  uint8_t ret = BLE_STATUS_SUCCESS ; 
  
  if ( (data == NULL) || (length == 0) ) 
  {
    LogInfo(" BleMngr :  BLECB_RequestToController -> data = NULL OR length = 0 !");
     ret = BLE_STATUS_FAILED;
  }
  else if(response_flag) /* Will block untill and Response is Received */
  {
    /* save buffer address to check and be used once a response is received */
    BleMngrCntxt.ResponsePtr = data ;
    
    /* - data input argment will be sent , make sure that the BleSerial Transmit is Done as 
         retruning BLE_STATUS_FAILED to BleStack is not allowed in that case !!. 
         This is a BleStack Constraint .. Instead busy-wait untill BleSerialTransmit retruns BLE_SERIAL_STATUS_NO_ERR
    */
    while ( BLE_SERIAL_STATUS_ERR == BleSerialTransmit(data,length) );
    
    /* block BleMngr over the reception of Response */
    BleMngrBlock();
    
    /* reset ResponsePtr */
    BleMngrCntxt.ResponsePtr = NULL;
    
  }
  else
  {
     /* data input argment will be send over Serial Interface */     
    if ( BLE_SERIAL_STATUS_ERR == BleSerialTransmit(data,length) )
    {
      /* retruning BLE_STATUS_FAILED to BleStack is allowed in that case , indicate Failure to BleStack to retransmit */
      ret = BLE_STATUS_FAILED ;
    }
  }
  
  /* hook to track exit For Debugging */
  BlePort_DebugBleCBRequestToControllerExitHook();
  
  return ret ;
}

/**
 * @brief
 * Callback invoked when a packet is received from the BLE serial interface.
 * @details
 *   - If the packet is a response (to a previous request), copies it to the response buffer and unblocks the manager.
 *   - If the packet is an asynchronous event, notifies the BLE manager to process it.
 * @param Buf Pointer to received data buffer
 * @param Size Size of received data in bytes
 */

void BleMngr_BleSerialRxCb( uint8_t* Buf , uint16_t Size )
{
  if( BleStack_IsResponse(Buf) )
  {
     /* transfer response to the buffer passed by BleStack in BLECB_RequestToController */
    memcpy( BleMngrCntxt.ResponsePtr , Buf , Size );
    vPortFree(Buf);
    /* Unblock BleMngr over the reception of Response */
    BleMngrUnBlock();
  }
  else
  {
    /* Indicate to BleMngr that there is a need to run BleStack_IndicationFromController */
    BleMngr_SerialAsynchPcktEvtInd( (void*)Buf , BleMngrSerialAsynchPcktEvtCb );
  }
}

/**
 * @brief
 * Callback invoked when a packet transmit is done over the BLE serial interface.
 * @details
 *   - once the packet is transmitted , notify the BLE manager.
 * @param Buf Pointer to received data buffer
 * @param Size Size of received data in bytes
 */
void BleMngr_BleSerialTxCb( void )
{
  /* Post HwTxDone to BleMngr , run BleStack_Proccess() */
  BleMngr_AppRunProcEvtInd( NULL /* callback */ );
}
