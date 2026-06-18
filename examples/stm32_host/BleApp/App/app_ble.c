/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_ble.c
  * @author  MCD Application Team
  * @brief   BLE Application
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
  * Application Flow Overview
  * ------------------------
  *
  * Terminology Used:
  * -----------------
  * - "HOST": Refers to the Automation GUI program interfacing with the platform via STLink.
  * - "BleStack": Refers to the combined BLE Host Stack and BLE Controller, treated as a single entity.
  *
  * - There are two scenarios to cover :
  *
  * 1. Command/Response Scenario (HOST -> BleApp -> BleStack -> BleApp -> HOST)
  * -----------------------------------------------------------------------
  * 
  *   a) The function BleAppNewHostPcktInd (triggered by BLE HOST Uart Receive) receives data from the HOST,
  *      places it into BleAsynchEventFromHostQueue, and notifies BleApp.
  * 
  *   b) BleApp processes and parses the data from BleAsynchEventFromHostQueue, then sends it to the BleStack via:
  *        - BleMngr_AppAciCmndEvtInd( input, output , BleMngrAciCmndProcCompeleteCb );
  *      At this point, BleApp is preempted and BleStackTask (BleMngr) runs. After processing, control returns to BleApp.
  * 
  *   c) The output data is sent from BleApp back to the HOST using BleHostSend.
  *
  * 2. Asynchronous Events Scenario (BleStack -> BleApp -> HOST)
  * ----------------------------------------------------------
  * 
  *   a) Asynchronous data events (triggered by BLECB_Indication) are placed into BleAsynchEventFromBleStackQueue,
  *      and BleApp is notified.
  * 
  *   b) BleApp processes and parses the data from BleAsynchEventFromBleStackQueue, then sends it to the HOST
  *      using BleHostSend.
  *
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "logging.h"

/* Private includes ----------------------------------------------------------*/

/* BleStack Inlcude Files */
#include "ble_core.h"
#include "blestack.h"
#include "bleplat.h"
#include "ble_hci_le.h"
#include "ble_gap_aci.h"
#include "ble_gatt_aci.h"
#include "ble_std.h"

/* BleMngr And Interfaces Inlcude Files */
#include "ble_mngr.h"
   
/* BLE application and host UART includes */
#include "app_ble.h"
#include "lhci.h"
#include "ble_host_uart.h"

/* FreeRTOS Include Files */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"

/* Private typedef -----------------------------------------------------------*/

/**
 * @brief
 *   Compile-time check for FreeRTOS Queue Sets support
 */   
#ifndef configUSE_QUEUE_SETS
#error "configUSE_QUEUE_SETS has to be defined in FreeRTOSConfig.h and equal 1 to compile this file."
#else
#if(configUSE_QUEUE_SETS == 0) 
#error "configUSE_QUEUE_SETS has to be defined in FreeRTOSConfig.h and equal 1 to compile this file."
#endif 
#endif

/* Event group and blocking configuration */
#define BLE_APP_EVT_BLOCK                      (1<<5)
#define BLE_APP_BLOCK_TIM_MS                   (1000) 

/* BleApp MQ Messages Length */
#define BLE_TM_MQ_RX_FROM_HOST_MESSAGE_LEN     (sizeof(void*))
#define BLE_TM_MQ_RX_FROM_BLESTACK_MESSAGE_LEN (sizeof(BleStackEvt))

/* Default mask + LE meta events (bit 61) */
static uint8_t default_evt_mask[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0x00, 0x20};
static uint8_t default_le_evt_mask[] = {0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief BLE HCI/ACI Events passed to BLE Application through BLECB_Indication
 */
typedef struct
{
  uint32_t PcktSize;  /* size of the packet    */
  uint8_t* pPkct;     /* pointer of the Packet */
}BleStackEvt;

/**
 * @brief BLE Application Main Context Structure
 * Holds all handles and context needed for BLE application task operation.
 */
typedef struct 
{  
  /* BleApp Task Operation Context */
  uint16_t AciCmndRetSize;                                          /* return value from running BleStack_Request */
  /*---------------------------------------------------------------------------------------------*/
  /* BleApp FreeRTOS Context */
  TaskHandle_t       BlAppHandle;                                    /* FreeRTOS Task Handle */
  QueueHandle_t      BleAsynchEventFromHostQueue;                    /* FreeRTOS Message Queue Handle */
  QueueHandle_t      BleAsynchEventFromBleStackQueue;                /* FreeRTOS Message Queue Handle */
  QueueSetHandle_t   BleTmAsynchEventsMQSet;                         /* FreeRTOS Message Queue Set Handle */
  EventGroupHandle_t BlockedEvts;                                    /* FreeRTOS Event Group Handle */  
  /*---------------------------------------------------------------------------------------------*/
}BleAppContext;

/* Private defines -----------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
BleAppContext BleAppCntxt;

static struct {
  uint16_t gap_service_handle;
  uint16_t dev_name_char_handle;
  uint16_t appearance_char_handle;
} ble_gap_ctx;

/* Global variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void BleAppBlock( void );
static void BleAppUnBlock( void );
static void BleAppEvtInd( QueueHandle_t Queue , void * Evt);
static void BleMngrAciCmndProcCompeleteCb ( void* ptr );
static uint8_t ProcessHostPacket(uint8_t* pPkt);
static void BleAppNewBleStackPcktInd( const uint8_t* data, uint16_t length);
static void BleAppTask ( void* ptr );
/* External variables --------------------------------------------------------*/
/* Functions Definition ------------------------------------------------------*/

/*************************************************************
 *
 * BLE ADV / GAP FUNCTIONS
 *
 *************************************************************/
static bool bt_hci_init_default(void)
{
	tBleStatus status = hci_set_event_mask(default_evt_mask);
	if (status != BLE_STATUS_SUCCESS) {
		LogError("hci_set_event_mask: x%02x", status);
		return false;
	}

	status = hci_le_set_event_mask(default_le_evt_mask);
	if (status != BLE_STATUS_SUCCESS) {
		LogError("hci_le_set_event_mask: x%02x", status);
		return false;
	}

	return true;
}

bool BleAppAciInit(void)
{
  if (!bt_hci_init_default()) {
    LogError("bt_hci_init_default failed\n");
		return false;
	}
  if (aci_gatt_init() != BLE_STATUS_SUCCESS) {
    LogError("aci_gatt_init failed\n");
    return false;
  }
  if (aci_gap_init(GAP_PERIPHERAL_ROLE, 0, 10,
                   &ble_gap_ctx.gap_service_handle,
                   &ble_gap_ctx.dev_name_char_handle,
                   &ble_gap_ctx.appearance_char_handle) != BLE_STATUS_SUCCESS) {
    LogError("aci_gap_init failed\n");
    return false;
  }
  return true;
}

bool BleAppAdvStart(uint8_t adv_type, uint16_t interval_min, uint16_t interval_max,
                    const uint8_t *dev_name, uint8_t dev_name_len)
{
  const uint8_t empty[1] = {0};
  if (hci_le_set_scan_response_data(0, empty) != BLE_STATUS_SUCCESS) {
    LogError("hci_le_set_scan_response_data failed\n");
    return false;
  }
  if (aci_gap_set_discoverable(adv_type, interval_min, interval_max,
                               0x00 /*public*/, 0x00 /*no filter*/,
                               dev_name_len, dev_name,
                               0, NULL, 0, 0) != BLE_STATUS_SUCCESS) {
    LogError("aci_gap_set_discoverable failed\n");
    return false;
  }
  LogInfo("BLE advertising started\n");
  return true;
}

bool BleAppAdvStop(void)
{
  if (aci_gap_set_non_discoverable() != BLE_STATUS_SUCCESS) {
    LogError("aci_gap_set_non_discoverable failed\n");
    return false;
  }
  LogInfo("BLE advertising stopped\n");
  return true;
}

bool BleAppAddService(void)
{
  tBleStatus status;
  uint16_t service_handle;
  uint16_t char_handle;

  /* Random UUIDs:
   * service: dccfffff-69bd-4b2d-920a-50fa1c603362
   * char:    f487e10f-acd1-4881-823c-3bb118dbaf45
   */
  const Service_UUID_t service_uuid = {
    .Service_UUID_128 = {0x62, 0x33, 0x60, 0x1c, 0xfa, 0x50, 0x0a, 0x92,
                         0x2d, 0x4b, 0xbd, 0x69, 0xff, 0xff, 0xcf, 0xdc}};
  const Char_UUID_t char_uuid = {
    .Char_UUID_128 = {0x45, 0xaf, 0xdb, 0x18, 0xb1, 0x3b, 0x3c, 0x82,
                      0x81, 0x48, 0xd1, 0xac, 0x0f, 0xe1, 0x87, 0xf4}};

  status = aci_gatt_add_service(UUID_TYPE_128, &service_uuid, PRIMARY_SERVICE,
                                6, &service_handle);
  if (status != BLE_STATUS_SUCCESS) {
    LogError("aci_gatt_add_service failed: 0x%02x\n", status);
    return false;
  }

  status = aci_gatt_add_char(service_handle, UUID_TYPE_128, &char_uuid, 20,
                             CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
                             ATTR_PERMISSION_NONE, GATT_NOTIFY_ATTRIBUTE_WRITE,
                             16, 0x01, &char_handle);
  if (status != BLE_STATUS_SUCCESS) {
    LogError("aci_gatt_add_char failed: 0x%02x\n", status);
    return false;
  }

  LogInfo("GATT service added, service_handle=0x%04x char_handle=0x%04x\n",
          service_handle, char_handle);
  return true;
}

/* Override weak HCI event callbacks */
tBleStatus hci_le_connection_complete_event(uint8_t Status, uint16_t Connection_Handle,
    uint8_t Role, uint8_t Peer_Address_Type, const uint8_t *Peer_Address,
    uint16_t Conn_Interval, uint16_t Conn_Latency, uint16_t Supervision_Timeout,
    uint8_t Central_Clock_Accuracy)
{
  LogInfo("BLE connected, handle=0x%04x status=0x%02x\n", Connection_Handle, Status);
  return BLE_STATUS_SUCCESS;
}

tBleStatus hci_disconnection_complete_event(uint8_t Status, uint16_t Connection_Handle,
    uint8_t Reason)
{
  LogInfo("BLE disconnected, handle=0x%04x reason=0x%02x\n", Connection_Handle, Reason);
  return BLE_STATUS_SUCCESS;
}

tBleStatus hci_le_connection_update_complete_event(uint8_t Status, uint16_t Connection_Handle,
    uint16_t Conn_Interval, uint16_t Conn_Latency, uint16_t Supervision_Timeout)
{
  LogInfo("BLE conn updated, handle=0x%04x status=0x%02x interval=%u latency=%u timeout=%u\n",
          Connection_Handle, Status, Conn_Interval, Conn_Latency, Supervision_Timeout);
  return BLE_STATUS_SUCCESS;
}

tBleStatus hci_le_data_length_change_event(uint16_t Connection_Handle,
    uint16_t MaxTxOctets, uint16_t MaxTxTime,
    uint16_t MaxRxOctets, uint16_t MaxRxTime)
{
  LogInfo("BLE data len changed, handle=0x%04x tx=%u/%uus rx=%u/%uus\n",
          Connection_Handle, MaxTxOctets, MaxTxTime, MaxRxOctets, MaxRxTime);
  return BLE_STATUS_SUCCESS;
}

tBleStatus hci_le_phy_update_complete_event(uint8_t Status, uint16_t Connection_Handle,
    uint8_t TX_PHY, uint8_t RX_PHY)
{
  LogInfo("BLE PHY updated, handle=0x%04x status=0x%02x tx_phy=%u rx_phy=%u\n",
          Connection_Handle, Status, TX_PHY, RX_PHY);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_gatt_attribute_modified_event(uint16_t Connection_Handle,
    uint16_t Attr_Handle, uint16_t Offset,
    uint16_t Attr_Data_Length, const uint8_t *Attr_Data)
{
  LogInfo("GATT attr modified, handle=0x%04x attr=0x%04x offset=%u len=%u\n",
          Connection_Handle, Attr_Handle, Offset, Attr_Data_Length);
  return BLE_STATUS_SUCCESS;
}

/* ACI_HAL_END_OF_RADIO_ACTIVITY_EVENT callback function */
tBleStatus aci_hal_end_of_radio_activity_event( uint8_t Last_State,
                                                uint8_t Next_State,
                                                uint32_t Next_State_SysTime,
                                                uint8_t Last_State_Slot,
                                                uint8_t Next_State_Slot )
{
  LogInfo("Radio activity: 0x%02x -> 0x%02x sys_time=%u us\n",
          Last_State, Next_State, Next_State_SysTime);
  return BLE_STATUS_SUCCESS;
}

/**
 * @brief De-initializes the BLE application.
 * Currently not used.
 */
void BleAppDeInit( void )
{
  /* NOT USED !! */
}

/**
 * @brief Initializes the BLE application, including BLE stack, queues, event groups, and task.
 */
void BleAppInit(void)
{  
   /* Initialize the HOST UART Handler */
  BleHostInit();
  /* Initialize the BLE Stack and Initialize the BLE Platform dependecies (NVM , Timers ... etc) */
  BleMngrInit();
     
  /****************************************************************************************/

   /* Queue to receive traffic to be sent from HOST to BleApp by UART */
  BleAppCntxt.BleAsynchEventFromHostQueue = xQueueCreate((UBaseType_t)BLE_TM_MQ_RX_FROM_HOST_LEN, 
                                                         (UBaseType_t)BLE_TM_MQ_RX_FROM_HOST_MESSAGE_LEN);

  /*Buffer to receive traffic to be sent from BleStack to BleApp by BLECB_Indication */ 
  BleAppCntxt.BleAsynchEventFromBleStackQueue = xQueueCreate((UBaseType_t)BLE_TM_MQ_RX_FROM_BLESTACK_LEN,
                                                             (UBaseType_t)BLE_TM_MQ_RX_FROM_BLESTACK_MESSAGE_LEN);
  
  if( ( NULL == BleAppCntxt.BleAsynchEventFromHostQueue ) || ( NULL == BleAppCntxt.BleAsynchEventFromBleStackQueue ) )
  {
    LogInfo(" BLE TM Allocations (MQs) Failed !! ");
    BleErrHandler();
  }

  /*****************************************************************************************/
  
  /* Create a queue set to multiplex events from both queues */
  BleAppCntxt.BleTmAsynchEventsMQSet = xQueueCreateSet( BLE_TM_MQ_SET_EVTS_SIZE /* uxEventQueueLength */ );
  
  if( ( NULL == BleAppCntxt.BleTmAsynchEventsMQSet ) )
  {
    LogInfo(" BLE TM Allocations (MQSet) Failed !! ");
    BleErrHandler();
  }
  else /* Add queues to the set */
  {
      if(pdPASS != xQueueAddToSet(BleAppCntxt.BleAsynchEventFromHostQueue, BleAppCntxt.BleTmAsynchEventsMQSet))
      {
        LogInfo(" BLE TM Adding MQ to MQSet Failed !! ");
        BleErrHandler();
      }

      if(pdPASS != xQueueAddToSet(BleAppCntxt.BleAsynchEventFromBleStackQueue, BleAppCntxt.BleTmAsynchEventsMQSet))
      {
        LogInfo(" BLE TM Adding MQ to MQSet Failed !! ");
        BleErrHandler();
      }
  }
  
  /*****************************************************************************************/
  
  /* Create event group for task synchronization (blocking/unblocking) */
    BleAppCntxt.BlockedEvts = xEventGroupCreate();
    if( NULL ==BleAppCntxt.BlockedEvts )
    {
      LogInfo(" BleApp Event Group Allocations Failed !! ");
      BleErrHandler();
    }
    
   /* Register BLE application (BleApp) task with FreeRTOS */  
  (void)xTaskCreate((TaskFunction_t)BleAppTask,
                    "BleAppTask",
                    BLE_APP_TASK_STACK_SIZE,
                    NULL,
                    BLE_APP_TASK_PRIO,
                    &(BleAppCntxt.BlAppHandle));

  if( (NULL == BleAppCntxt.BlAppHandle ) )
  {
    LogInfo(" BLE TM Task Creation Failed !! ");
    BleErrHandler();
  }
  
 
  return;
}

/**
 * @brief Checks if a received HCI packet is complete based on its type and length.
 * @param RxBuffer Pointer to the received buffer.
 * @param size     Size of the received buffer.
 * @return 1 if packet is complete, 0 otherwise.
 * @Note check this wiki page for more info about local commands and response structures 
 * URL : https://wiki.st.com/stm32mcu/wiki/Connectivity:STM32WBA_TransparentMode
 */
uint8_t BleApp_isCompleteHciPackt ( uint8_t* RxBuffer , uint16_t size ) 
{

    uint16_t len = 0;
    uint8_t isCompletePckt = 1 ; /* defualt is that the pckt is compelete */
        
    if(size < HCI_EVENT_HDR_SIZE) /* smallest header possible */
    {
      isCompletePckt = 0 ; /* Packet Length can not be calculated  yet */
    }
    else /* Packet Length can be calculated  */
    {
      switch(RxBuffer[0])
      { 
        
        case HCI_COMMAND_PKT_TYPE : /* packet len is @ byte_3 , data[2] */
        {
          if( size < HCI_COMMAND_HDR_SIZE )
          {
            isCompletePckt = 0;
            return isCompletePckt ;
          }
          else
          {
            // buffer[0] = Packet Type (should be 0x01)
            // buffer[1,2] = OpCode (2 bytes)
            // buffer[3] = Parameter Length (N)      
            len = RxBuffer[3] ;
            len += HCI_COMMAND_HDR_SIZE ; /* the very first three byte till 'length' feild in the packt */            
          }
          break;
        }
        
        /*
         HCI Event Packet
         ----------------------------------------
         Byte 0: Packet Type (0x04)
         Byte 1: Event Code
         Byte 2: Parameter Total Length (N)
         Byte 3..(3+N-1): Parameters (N bytes)

         Example:
         [0x04][Event_Code][Param_Length][Parameters...]        
        */
        
        case HCI_EVENT_PKT_TYPE : /* packet len is @ byte_3 , data[2] */
        {
          len = RxBuffer[2] ;
          len += HCI_EVENT_HDR_SIZE ; /* the very first three byte till 'length' feild in the packt */
          break;
        }
        
        /*        
         HCI ACL Data Packet
         --------------------------------------
         Byte 0: Packet Type (0x02)
         Byte 1-2: Handle and Flags (2 bytes, little-endian)
           - Bits 0-11: Connection Handle
           - Bits 12-13: PB Flag (Packet Boundary)
           - Bits 14-15: BC Flag (Broadcast)
         Byte 3-4: Data Total Length (2 bytes, little-endian, value = N)
         Byte 5..(5+N-1): Data (N bytes)

         Example:
         [0x02][Handle+Flags_LSB][Handle+Flags_MSB][Data_Length_LSB][Data_Length_MSB][Data...]        
        */      
        
        case HCI_ACLDATA_PKT_TYPE : /* packet len is @ byte_4 and byte_5 , data[3] combined with data[4] */
        {
          if( size < HCI_ACLDATA_HDR_SIZE )
          {
            isCompletePckt = 0;
            return isCompletePckt ;
          }
          else
          {
            /* For ACL data, there is no need to check byte_4 as data length should not exceed 251 bytes
              
               so instead of
              
               len = ((uint16_t)aPkt->Buf[3])|(((uint16_t)aPkt->Buf[4]) << 8); 

               This will be used ! 

               len = ((uint8_t)aPkt->Buf[3]);

              */
            len = (RxBuffer[3]);
            len += HCI_ACLDATA_HDR_SIZE ; /* the very first five bytes till 'length' feild in the packt */
            break;
          }
        }
        
        /*
         3. HCI ISO Data Packet
         ----------------------------------------------------------
         Byte 0: Packet Type (0x03)
         Byte 1-2: Handle and Flags (2 bytes, little-endian)
           - Bits 0-11: Connection Handle
           - Bits 12-15: Reserved
         Byte 3: Data Total Length (N)
         Byte 4..(4+N-1): Data (N bytes)

         Example:
         [0x03][Handle+Flags_LSB][Handle+Flags_MSB][Data_Length][Data...]
        */
                
        case HCI_ISODATA_PKT_TYPE : /* packet len is @ byte_4 and byte_5 , data[3] combined with data[4] , 
                                       last 2 Most Significant Bits are not included !! , to be dropped */
        {
          if( size < HCI_ISODATA_HDR_SIZE )
          {
            isCompletePckt = 0;
            return isCompletePckt ;
          }
          else
          {
            len = (RxBuffer[3]) /*LSBs*/ | ((uint16_t)(((uint8_t)RxBuffer[4] & (uint8_t)0x3f)<< 8)) /*MSB*/;
            len += HCI_ISODATA_HDR_SIZE; /* the very first five bytes till 'length' feild in the packt */
            break;
          }
        }
                
        case TL_LOCCMD_PKT_TYPE : 
        {
          uint16_t lcmd_opcode =(uint16_t)(RxBuffer[1] | (RxBuffer[2]<<8));
          
          switch(lcmd_opcode)
          {
            case LHCI_OPCODE_C1_WRITE_REG:
            {
              len = 13 ;
              len += 2;
              break;                
             }

            case LHCI_OPCODE_C1_READ_REG:
            {
              len = 5 ;
              len += 2;
              break;                
             }

            case LHCI_OPCODE_C1_DEVICE_INF:
            {
              len = 0 ;
              len += 2;
              break;                
             }

            case LHCI_OPCODE_C1_RF_CONTROL_ANTENNA_SWITCH:
            {
              len = 1 ;
              len += 2;
              break;                
             }

            default:
            {
                 LogInfo(" BleApp_isCompleteHciPackt :  undefined cmd_op for TL_LOCCMD_PKT_TYPE !! ");
                 BleErrHandler();
                 break;
            }  
          }
          break;
        }
        
        case TL_LOCRSP_PKT_TYPE : 
        {
          if( size < TL_EVT_CS_PAYLOAD_SIZE )
          {
            isCompletePckt = 0;
            return isCompletePckt;
          }
          else
          {
            len = TL_EVT_CS_PAYLOAD_SIZE;
            len += 4 ;
            break;
          }
        }
        
        default:
        {
          LogInfo(" BleApp_isCompleteHciPackt :  undefined Packet Indicator !! ");
          BleErrHandler();
          break;
        }
      }
      /*compare the len with the size of received data */
      if ((size /* actual size */ < len /* calculated size */)) isCompletePckt = 0 ;
    }
    return isCompletePckt ; 
}

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/

/**
 * @brief Blocks the BLE application task until an event is set or timeout occurs.
 */
static void BleAppBlock( void )
{
  EventBits_t EvtBits = 0 ;
  
  EvtBits = xEventGroupWaitBits(BleAppCntxt.BlockedEvts,
                                  BLE_APP_EVT_BLOCK,
                                  pdTRUE,
                                  pdFALSE,
                                  pdMS_TO_TICKS(BLE_APP_BLOCK_TIM_MS));

  if (!( EvtBits & BLE_APP_EVT_BLOCK ))
  {
    LogInfo(" BleAppBlock Timeout !! ");
    BleErrHandler(); 
  }
}

/**
 * @brief Unblocks the BLE application task by setting the event bit.
 *        Handles both ISR and task context.
 */

static void BleAppUnBlock( void )
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xPortIsInsideInterrupt())
  {
    xEventGroupSetBitsFromISR(BleAppCntxt.BlockedEvts,
                              BLE_APP_EVT_BLOCK,
                              &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
  else xEventGroupSetBits(BleAppCntxt.BlockedEvts, BLE_APP_EVT_BLOCK);
}

/**
 * @brief Sends an event to the specified queue, handling both ISR and task context.
 * @param Queue Queue handle to send to.
 * @param Evt   Pointer to event data.
 */
static void BleAppEvtInd( QueueHandle_t Queue , void * Evt)
{  
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xPortIsInsideInterrupt())
  {
    if(pdFALSE == xQueueSendFromISR(Queue,
                      Evt, 
                      &xHigherPriorityTaskWoken))
    {
      LogInfo(" BleAppEvtInd is Broken !! ");
      BleErrHandler();    
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
  else
  {
    if(pdFALSE == xQueueSend( Queue,
                              Evt,
                              0 /*xTicksToWait*/ ))
    {
      LogInfo(" BleAppEvtInd is Broken !! ");
      BleErrHandler();    
    }
  }
}

/*************************************************************
 *
 * BleAsynchEventFromHostQueue Processing FUNCTIONS
 *
 *************************************************************/

/**
 * @brief Callback called when BLE stack command processing is complete.
 *        Stores the return size and unblocks the BLE application task.
 * @param ptr Pointer to return size.
 */

static void BleMngrAciCmndProcCompeleteCb ( void* ptr )
{
  /* save return value */
  BleAppCntxt.AciCmndRetSize = *((uint16_t*)ptr);
  /* proccessing of BleStack is Compelete */
  BleAppUnBlock();
}

/**
 * @brief Processes a packet received from the HOST.
 *        Handles command and local command/response packets.
 * @param pPkt Pointer to packet data.
 * @return 0 on success.
 */
static uint8_t ProcessHostPacket(uint8_t* pPkt)
{
  uint8_t packet_type = pPkt[0];
    
  if(( packet_type == HCI_COMMAND_PKT_TYPE ) || ( packet_type == TL_LOCCMD_PKT_TYPE ) || ( packet_type == TL_LOCRSP_PKT_TYPE ))
  {
     if((packet_type == TL_LOCCMD_PKT_TYPE) || (packet_type == TL_LOCRSP_PKT_TYPE))
     {
       TM_SysLocalCmd(&(pPkt[1]));
     }
          
     /*
     * BleStack_Request
     *
     * @brief This function gives a request from application to the BLE stack.
     * The input parameter is a buffer (in) of bytes in the BLE standard format:
     * HCI/ACI command packet or ACL data packet.
     * The response packet is returned in buffer_out and the size (in bytes)
     * of the response is given by the function return value.
     */
      
     BleMngr_AppAciCmndEvtInd( (void*)pPkt , (void*)pPkt , BleMngrAciCmndProcCompeleteCb );
     /* Wait proccessing of BleStack */
     BleAppBlock();
   }
   return 0 ;
}

/**
 * @brief Called when a new packet is received from the HOST.
 *        Validates packet type and enqueues it for processing.
 * @param data   Pointer to packet data.
 * @param length Length of packet.
 */

void BleAppNewHostPcktInd( const uint8_t* data, uint16_t length)
{    
  if ((data[0] == HCI_EVENT_PKT_TYPE)   || (data[0] == HCI_ISODATA_PKT_TYPE) || (data[0] == HCI_ACLDATA_PKT_TYPE) || 
      (data[0] == HCI_COMMAND_PKT_TYPE) || (data[0] == TL_LOCCMD_PKT_TYPE)   || (data[0] == TL_LOCRSP_PKT_TYPE)) 
  {
    BleAppEvtInd( BleAppCntxt.BleAsynchEventFromHostQueue , (void*)&data );
  }
  else
  {
    LogInfo("BleAppNewHostPcktInd : Corrupted Packet !\n");
    BleErrHandler();
  }
  return;
}

/*************************************************************
 *
 * BleAsynchEventFromBleStackQueue Processing FUNCTIONS
 *
 *************************************************************/
/**
 * @brief Called when a new packet is received from the BLE stack.
 *        Allocates memory, copies data, and enqueues for processing.
 * @param data   Pointer to packet data.
 * @param length Length of packet.
 */
static void BleAppNewBleStackPcktInd( const uint8_t* data, uint16_t length)
{  
  if ((data[0] == HCI_EVENT_PKT_TYPE)   || (data[0] == HCI_ISODATA_PKT_TYPE) || (data[0] == HCI_ACLDATA_PKT_TYPE) || 
      (data[0] == HCI_COMMAND_PKT_TYPE) || (data[0] == TL_LOCCMD_PKT_TYPE)   || (data[0] == TL_LOCRSP_PKT_TYPE)) 
  {
    void* pPkt = NULL; BleStackEvt BleEvt;
    pPkt = pvPortMalloc( length );
    if ( pPkt )
    {
      BleEvt.PcktSize = length;
      BleEvt.pPkct    = pPkt;
      memcpy(pPkt, data , length);
      BleAppEvtInd( BleAppCntxt.BleAsynchEventFromBleStackQueue , &BleEvt );
    }
    else
    {
      LogInfo("BleAppNewBleStackPcktInd : Alloc failed\n");
      BleErrHandler();
    }
  }
  else
  {
    LogInfo("BleAppNewBleStackPcktInd : Corrupted Packet !\n");
    BleErrHandler();
  }
  
  return;  
}

/*
 * BLECB_Indication
 *
 * @brief Callback called by the BLE stack (from BleStack_Process() context)
 * to send an indication to the application. The indication is a BLE standard
 * packet that can be either an ACI/HCI event or an ACL data.
 * The parameters "ext_data" and "ext_length" are not used.
 */
tBleStatus BLECB_Indication( const uint8_t* data,
                          uint16_t length,
                          const uint8_t* ext_data,
                          uint16_t ext_length )
{
  UNUSED(ext_data);
  UNUSED(ext_length);
  
  uint16_t total_length = (length+ext_length);
                                          
#if ( LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLECB_IND_GPIO_PORT, LOGIC_DEBUG_BLECB_IND_GPIO_PIN);
#endif      
    
  BleAppNewBleStackPcktInd( data, total_length );
  
#if ( LOGIC_ANALYIZER_DEBUG == 1 )       
  HAL_GPIO_TogglePin(LOGIC_DEBUG_BLECB_IND_GPIO_PORT, LOGIC_DEBUG_BLECB_IND_GPIO_PIN);
#endif

  return BLE_STATUS_SUCCESS ;
}

/**
 * @brief Main BLE application task.
 *        Waits for events from either the HOST or BLE stack and processes them.
 * @param ptr Unused.
 */
static void BleAppTask ( void* ptr )
{
  void* pPkt;
  BleStackEvt BleEvt;
  
  while( 1 )
  {
    /* NOTE : the Following FreeRTOS utility needs configUSE_QUEUE_SETS to be defined in FreeRTOSConfig.h and equal 1 */
    
    /* Block until any queue in the set has data */ 
    QueueSetMemberHandle_t activatedQueue = xQueueSelectFromSet(BleAppCntxt.BleTmAsynchEventsMQSet, portMAX_DELAY);
    
    /* Determine which queue was activated and process */
    if (activatedQueue == BleAppCntxt.BleAsynchEventFromBleStackQueue) 
    {        
      if ( pdTRUE == xQueueReceive(BleAppCntxt.BleAsynchEventFromBleStackQueue, &BleEvt, 0 /* No Blockibg */) )
      {
          BLE_EventProcess(BleEvt.pPkct);
          BleHostSend(BleEvt.pPkct,(uint16_t)(BleEvt.PcktSize));
          vPortFree(BleEvt.pPkct);
          BleEvt.PcktSize = 0 ;
      }
      else
      {
        LogInfo(" BleAppTask : xQueueReceive over BleTmAsynchEventsMQSet is Broken !! ");
        BleErrHandler();
      }    
    } 
    else if (activatedQueue == BleAppCntxt.BleAsynchEventFromHostQueue) 
    {         
      if ( pdTRUE == xQueueReceive(BleAppCntxt.BleAsynchEventFromHostQueue, &pPkt, 0 /* No Blockibg */) ) 
      {
        if(0 == ProcessHostPacket((uint8_t*)pPkt))
        {
          BleHostSend((uint8_t*)pPkt, BleAppCntxt.AciCmndRetSize);
          BleAppCntxt.AciCmndRetSize = 0 ;
        }
        else
        {
          LogInfo(" BleAppTask : ProcessHostPacket() is Broken !! ");
          BleErrHandler();
        } 
      }
      else
      {
        LogInfo(" BleAppTask : xQueueReceive over BleAsynchEventFromHostQueue is Broken !! ");
        BleErrHandler();
      }
    }
    else /* activatedQueue is not BleAsynchEventFromHostQueue nor BleAsynchEventFromBleStackQueue */
    {
      /*Loop is Broken , Error*/
      LogInfo(" BleAppTask Loop is Broken !! ");
      BleErrHandler();
    }
  }  
}
