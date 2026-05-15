/**
 ******************************************************************************
 * @file    ble_serial.c
 * @author  GPM Application Team
 * @brief   BLE Controller serial-bus interface implementation
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
 * @section Overview
 *
 *   This file implements the **BLE Serial Communication Layer** for the BLE Controller,
 *   providing a robust, event-driven, and thread-safe interface between the BLE stack,
 *   the application, and the Serial Bus Driver or HAL using FreeRTOS.
 *
 *******************************************************************************/

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

/* Private define ---------------------------------------------------------------------------------------------------------------------------------*/
   
/**
 * @brief BLE Packet Buffer Structure
 * @details
 *   Represents a single BLE packet buffer for RX/TX over the serial interface.
 *   - Buf: Byte array for packet data (size: BLE_SERIAL_BUFFER_SIZE)
 *   - size: Number of valid bytes in Buf (used as index for last element)
 */
typedef struct {
  uint8_t  Buf[BLE_SERIAL_BUFFER_SIZE];
  uint16_t size; /* can be used as Idx for last element */
} BlePckt_t;

/**
 * @brief Main Receive Ring Buffer Structure
 * @details
 *   - Pckts: Array of BlePckt_t, each for a received packet
 *   - InIdx: Index for next packet to store (enqueue)
 */

typedef struct {
  BlePckt_t Pckts[BLE_SERIAL_BUFFER_NB];
  uint8_t InIdx;
}BleRxRingBuf_t;

typedef struct 
{
  /*----------------------------- EVENT-RELATED CONTEXT ------------------------------*/

  BleRxRingBuf_t                   BleRxRingBuffer;             /* Ring buffer to receive data in */
    
  /*----------------------------- HAL/Serial Bus Driver-RELATED CONTEXT ------------------------------*/

  uint16_t                        CurrSize;                     /* Current size of received data */
  
} BleSerial_Contxt_t;

/* Private function prototypes ----------------------------------------------------------------------------------------------------------------------------*/

/**
 * @brief Checks if a complete HCI packet has been received in the buffer.
 * @param aPkt Pointer to the packet buffer
 * @param curr_size Current size of received data
 * @return 1 if complete, 0 otherwise
 */
static uint8_t bleSerial_IsCompleteHciPckt( const BlePckt_t* aPkt , uint16_t curr_size );

/* Private variables --------------------------------------------------------------------------------------------------------------------------------------*/

static BleSerial_Contxt_t BleSerialCntxt;

/* Private macro ------------------------------------------------------------------------------------------------------------------------------------------*/
                                            
#define  UPDATE_BLE_RX_RING_BUFFER_AFTER_POP()         /* NOT USED !! */
                                            
#define  UPDATE_BLE_RX_RING_BUFFER_AFTER_PUSH()        BleSerialCntxt.BleRxRingBuffer.InIdx++; \
                                                       BleSerialCntxt.BleRxRingBuffer.InIdx = BleSerialCntxt.BleRxRingBuffer.InIdx % BLE_SERIAL_BUFFER_NB; \
                                                       /* reset CurrSize to receive new HCI Packet */ \
                                                       BleSerialCntxt.CurrSize = 0 ; \
                                                         
/* Private Functions  ---------------------------------------------------------*/

/**
 * @brief BLE Serial TX Complete Callback
 * @details
 *   Called when a TX operation is complete. Notifies BLE Manager to process the event.
 */                                                       
void bleSerial_TxCpltCb ( void )
{
  BleMngr_BleSerialTxCb();
}

/**
 * @brief BLE Serial RX Complete Callback
 * @param size Number of bytes received in the latest RX operation
 * @details
 *   Handles incoming data, checks for complete HCI packets, and manages the RX ring buffer.
 *   - Accumulates received data in the current buffer.
 *   - If a complete HCI packet is detected, notifies BLE Manager and prepares for next RX.
 *   - If incomplete, sets up RX to append more data.
 */
void bleSerial_RxCpltCb ( uint16_t size )
{
  BlePckt_t* pPckt;
  
  /* Increment CurrSize with currently total received size */
  BleSerialCntxt.CurrSize += size ;
    
  /* pointer to current packet buffer used by RX */
  pPckt = (BlePckt_t*)&(BleSerialCntxt.BleRxRingBuffer.Pckts[BleSerialCntxt.BleRxRingBuffer.InIdx]);
    
  /* check if the buffer is containing a complete HCI Packet */
  if( bleSerial_IsCompleteHciPckt( pPckt , BleSerialCntxt.CurrSize ) )
  {
    /* we got complete HCI packet , save the total size of packet */
    pPckt->size = BleSerialCntxt.CurrSize;
    
    /*update the BleRxRingBuffer , resets CurrSize */
    UPDATE_BLE_RX_RING_BUFFER_AFTER_PUSH();
    
    /* Restart Rx for next HCI Data ASAP */
    BlePort_SerialRxUntilIdle(BleSerialCntxt.BleRxRingBuffer.Pckts[BleSerialCntxt.BleRxRingBuffer.InIdx].Buf, BLE_SERIAL_BUFFER_SIZE);    

    /* indicate to BleMngr That a packet has been received and further processing is needed */
    BleMngr_BleSerialRxCb( pPckt->Buf , pPckt->size );
  }
  else
  {
    /* Set RxAddress to Append Next Data to Current Buffer untill it's compeleted */
    uint8_t* RxAddress = &(pPckt->Buf[BleSerialCntxt.CurrSize]);
    /* Restart Rx for next HCI Data */
    BlePort_SerialRxUntilIdle( RxAddress , BLE_SERIAL_BUFFER_SIZE);
  }
}

/**
 * @brief BLE Serial Error Callback
 * @details
 *   Invoked by lower layers (Serial Bus Driver/HAL) on serial bus error.
 *   Calls the BLE error handler.
 */

void bleSerial_ErrCb( void )
{
  BleErrHandler(); 
}

/**
 * @brief Transmit data over BLE serial interface
 * @param aBuf Pointer to data buffer
 * @param aBufLength Length of data to transmit
 * @return BLE_SERIAL_STATUS_NO_ERR on success, BLE_SERIAL_STATUS_ERR on error
 */
BleSerial_Status BleSerialTransmit( uint8_t *aBuf, uint16_t aBufLength)
{
  BleSerial_Status ret = BLE_SERIAL_STATUS_NO_ERR;
  
  if ((aBufLength > 0) && (aBuf != NULL))
  { 
    BlePort_SerialTx(aBuf , aBufLength);
  }
  else
  {
    ret = BLE_SERIAL_STATUS_ERR;
  }
  
  return ret;
}

/**
 * @brief Enable BLE Serial Interface
 * @details
 *   - Clears RX ring buffer and resets current size.
 *   - Aborts any ongoing RX.
 *   - Starts RX to receive data until buffer is full or idle.
 */
void BleSerial_Enable( void )
{    
  memset(&(BleSerialCntxt.BleRxRingBuffer),0,sizeof(BleSerialCntxt.BleRxRingBuffer));
  
  BleSerialCntxt.CurrSize = 0;
  
  /*Abort any ongoing Rx */
  BlePort_SerialAbortRx();

  /* - Start Reception , Receive until buffer size has been reached or no activity on Rx channel 
     - When HCI frame is received, callback bleSerial_RxCpltCb is triggered
  */   
  BlePort_SerialRxUntilIdle(BleSerialCntxt.BleRxRingBuffer.Pckts[BleSerialCntxt.BleRxRingBuffer.InIdx].Buf, BLE_SERIAL_BUFFER_SIZE );

  return;
}

/**
 * @brief Initialize BLE Serial Interface
 * @details
 *   - Clears the BLE serial context.
 *   - Registers serial bus event handlers.
 *   - Enables BLE serial interface for the BLE controller.
 */
void BleSerial_Init(void)
{  
  memset(&BleSerialCntxt,0,sizeof(BleSerialCntxt));
  
  /* Register the Serial Bus Events Handlers */
  BlePort_SerialInit();
  
  /* Enable BLE Serial Interface to BleController */    
  BleSerial_Enable();
  
  return;
}

/**
 * @brief Check if a complete HCI packet has been received
 * @param aPkt Pointer to packet buffer
 * @param size Number of bytes currently received
 * @return 1 if complete, 0 otherwise
 * @details
 *   - Determines packet type (Event, ACL, ISO) and calculates expected length.
 *   - Compares expected length with received size to determine completeness.
 */
static uint8_t bleSerial_IsCompleteHciPckt( const BlePckt_t* aPkt , uint16_t size )
{
    uint16_t len = 0;
    uint8_t isCompletePckt = 1 ; /* defualt is that the pckt is compelete */
        
    if(size < HCI_EVENT_HDR_SIZE) /* smallest header possible */
    {
      isCompletePckt = 0 ; /* Packet Length can not be calculated  yet */
    }
    else /* Packet Length can be calculated  */
    {
      switch(aPkt->Buf[0])
      { 
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
          len = aPkt->Buf[2] ;
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
            len = (aPkt->Buf[3]);
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
            len = ((uint16_t)aPkt->Buf[3]) /*LSBs*/ | ((uint16_t)(((uint8_t)aPkt->Buf[4] & (uint8_t)0x3f)<< 8)) /*MSB*/;
            len += HCI_ISODATA_HDR_SIZE; /* the very first five bytes till 'length' feild in the packt */
            break;
          }
        }
        
        default:
        {
          LogInfo(" BleSerial : IsCompleteHciPckt !! ");
          BleErrHandler(); 
          break;
        }
      }
      /*compare the len with the size of received data */
      if ((size /* actual size */ < len /* calculated size */)) isCompletePckt = 0 ;
    }
    return isCompletePckt ; 
}
