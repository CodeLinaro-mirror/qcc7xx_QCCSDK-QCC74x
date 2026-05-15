/*****************************************************************************
 * @file    blestack.h
 *
 * @brief   Header file for STM32 BLE stack in RCP configuration
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

#ifndef BLESTACK_H__
#define BLESTACK_H__


#include "auto/ble_types.h"
#include "ble_bufsize.h"


/*
 * Definitions for return value of BleStack_Process( )
 */
enum
{
  BLE_SLEEPMODE_RUNNING   = 0,
  BLE_SLEEPMODE_CPU_HALT  = 1,
};

/*
 * Definitions for 'debug' parameter
 */
enum
{
  BLE_DEBUG_RAND_ADDR_INIT       = 0x00000010UL,
  BLE_DEBUG_NO_HOST_FLOW_CONTROL = 0x00000100UL,
};

/*
 * This structure contains the BLE stack initialization parameters
 */
typedef struct
{
  /* Start address of the RAM buffer allocated for BLE stack library.
   * It must be a 32bit aligned RAM area.
   */
  uint8_t* bleStartRamAddress;

  /* Size of the RAM buffer allocated for BLE stack library.
   * (could be filled with BLE_TOTAL_BUFFER_SIZE return value)
   */
  uint32_t total_buffer_size;

  /* Start address of the RAM buffer allocated for BLE NVM cache.
   */
  uint64_t* nvm_cache_buffer;

  /* Size of actual data in BLE NVM cache (in 64-bit words).
   * Range: 0 .. (nvm_cache_max_size - 1)
   */
  uint16_t  nvm_cache_size;

  /* Maximum size of BLE NVM cache (in 64-bit words).
   * Range: 1 .. 1024
   */
  uint16_t  nvm_cache_max_size;

  /* Start address of the RAM buffer allocated for GATT database.
   * It must be a 32bit aligned RAM area.
   */
  uint8_t* bleStartRamAddress_GATT;

  /* Size of the RAM buffer allocated for GATT database.
   * (could be filled with BLE_TOTAL_BUFFER_SIZE_GATT return value)
   */
  uint32_t total_buffer_size_GATT;

  /* Address of the RAM buffer allocated for GATT long write procedures.
   * If ACI_GATT_WRITE_LONG_CHAR_VALUE/ACI_GATT_WRITE_CHAR_RELIABLE commands
   * are used, this parameter must point to a 256-byte static buffer. If these
   * commands are never used, this parameter can be set to NULL.
   */
  uint8_t* gatt_long_write_buffer;

  /* Address of the RAM buffer allocated for the extension of Host commands.
   * This buffer is referred as the "extra data" buffer in the BLE Wireless
   * Interface document. If the commands that need this extension are never
   * used, this parameter can be set to NULL.
   */
  uint8_t* extra_data_buffer;

  /* Size of the RAM buffer allocated for the extension of Host commands.
   */
  uint32_t extra_data_buffer_size;

  /* Address of the Host events FIFO buffer.
   */
  uint16_t* host_event_fifo_buffer;

  /* Size of the Host events FIFO buffer (in 16-bit words).
   */
  uint16_t host_event_fifo_buffer_size;

  /* Address of the Controller events FIFO buffer.
   */
  uint16_t* ctrl_event_fifo_buffer;

  /* Size of the Controller events FIFO buffer (in 16-bit words).
   */
  uint16_t ctrl_event_fifo_buffer_size;

  /* Maximum number of Attributes (i.e. the number of characteristic + the
   * number of characteristic values + the number of descriptors, excluding the
   * services) that can be stored in the GATT database.
   * Note that certain characteristics and relative descriptors are added
   * automatically during device initialization so this parameters should be 9
   * (or 6 w.r.t. options) plus the number of user Attributes (NUM_GATT_
   * ATTRIBUTES used in the calculation of BLE TOTAL_BUFFER_SIZE_GATT)
   */
  uint16_t numAttrRecord;

  /* Maximum number of Services that can be stored in the GATT database.
   * Note that the GAP and GATT services are automatically added so this
   * parameter should be 2 plus the number of user services (NUM_GATT_SERVICES
   * used in the calculation of BLE_TOTAL_BUFFER_SIZE_GATT)
   */
  uint16_t numAttrServ;

  /* Size of the storage area for Attribute values (ATT_VALUE_ARRAY_SIZE used
   * in the calculation of BLE_TOTAL_BUFFER_SIZE_GATT)
   * This value depends on the number of attributes used by application. In
   * particular the sum of the following quantities (in octets) should be made
   * for each attribute:
   * - attribute value length
   * - 5, if UUID is 16 bit; 19, if UUID is 128 bit
   * - 2, if server configuration descriptor is used
   * - 2*numOfLinks, if client configuration descriptor is used
   * - 2, if extended properties is used
   * The total amount of memory needed is the sum of the above quantities for
   * each attribute.
   */
  uint16_t attrValueArrSize;

  /* Maximum number of simultaneous connections that the device will support.
   * Valid values are from 1 to 8 (NUM_LINKS used in the calculation of
   * BLE_TOTAL_BUFFER_SIZE).
   */
  uint8_t numOfLinks;

  /* Prepare Write List size in terms of number of packet with ATT_MTU=23 bytes
   */
  uint8_t prWriteListSize;

  /* Number of allocated memory blocks (ACL data pool size in 32-byte blocks)
   */
  uint16_t mblockCount;

  /* Maximum number of bearers that can be created for Enhanced ATT
   * in addition to the number of links
   * Range: 0 .. 64
   */
  uint8_t max_add_eatt_bearers;

  /* Maximum supported ATT_MTU size
   */
  uint16_t attMtu;

  /* Maximum value of the connection-oriented channel Maximum Payload Size
   * Range: 23 .. (BLE_EVT_MAX_PARAM_LEN - 7)
   */
  uint16_t max_coc_mps;

  /* Maximum number of connection-oriented channels.
   * Range: 0 .. 64
   */
  uint8_t max_coc_nbr;

  /* Maximum number of connection-oriented channels in initiator mode.
   * Range: 0 .. max_coc_nbr
   */
  uint8_t max_coc_initiator_nbr;

  /* Options flags
   * Bitmap of the "BLE_OPTIONS_..." definitions (see ble_defs.h).
   * - bit 0:   1: LL only                   0: LL + host
   * - bit 1:   1: no service change desc.   0: with service change desc.
   * - bit 2:   1: device name Read-Only     0: device name R/W
   * - bit 3:   1: extended adv supported    0: extended adv not supported
   * - bit 5:   1: Reduced GATT db in NVM    0: Full GATT db in NVM
   * - bit 6:   1: GATT caching is used      0: GATT caching is not used
   * - bit 8:   1: appearance Writable       0: appearance Read-Only
   * - bit 9:   1: Enhanced ATT supported    0: Enhanced ATT not supported
   * - other bits: reserved
   */
  uint16_t options;

  /* Debug flags
   * Bitmap of the "BLE_DEBUG_..." definitions (see above).
   */
  uint32_t debug;

} BleStack_init_t;

/*
 * BleStack_Init
 *
 * @brief The BLE Stack initialization routine
 *
 * @param[in]  init               pointer to the const structure containing
 *                                memory data for the device
 *
 * @return Value indicating success or error code.
 */
extern tBleStatus BleStack_Init( const BleStack_init_t* init );

/*
 * BleStack_Process
 *
 * @brief This function executes the processing of all Host Stack layers.
 * It has to be called regularly to process incoming Controller packets and
 * to execute Host Layers procedures.
 * Specifically, this function must be called in the following cases:
 *  - An ACI/HCI command has been sent to the BLE stack (either by calling
 *    BleStack_Request() or by direct call to a specific command).
 *  - An HCI event or ACL data has been received from the Controller and the
 *    function BleStack_IndicationFromController() has been called.
 *  - A BLE timer has expired and the function BLEPLATCB_TimerExpiry() has
 *    been called.
 *  - The transmission of a packet to the controller has ended.
 *  - The BleStack_Process() function returns BLE_SLEEPMODE_RUNNING.
 * Note: The BLE stack functions are not reentrant. It is not allowed to
 * interrupt BleStack_Init(), BleStack_IndicationFromController(),
 * BleStack_Process(), BleStack_Request(), or any HCI/ACI command, and call
 * again one of these functions.
 * The only exception is when the BLE stack is blocked during a call to
 * BLECB_RequestToController() while waiting for a response. In this case, the
 * call to BleStack_IndicationFromController() is allowed.
 *
 * @return
 *  BLE_SLEEPMODE_RUNNING   (0) -> BLE Stack Process has to be executed,
 *  BLE_SLEEPMODE_CPU_HALT  (1) -> BLE Stack Process does not have to be
 *                                 executed,
 *  When this function returns BLE_SLEEPMODE_RUNNING, it shall be re-called.
 *  If it returns BLE_SLEEPMODE_CPU_HALT, there is no need to call this
 *  function again and the MCU could go to sleep mode.
 */
extern uint8_t BleStack_Process( void );

/*
 * BleStack_Request
 *
 * @brief This function gives a request from application to the BLE stack.
 * The input parameter is a buffer (in) of bytes in the BLE standard format:
 * HCI/ACI command packet or ACL data packet.
 * The response packet is returned in buffer_out and the size (in bytes)
 * of the response is given by the function return value.
 * Note: After a call to this function, it is mandatory to trigger a call to
 * BleStack_Process().
 */
extern uint16_t BleStack_Request( const uint8_t* buffer_in,
                                  uint8_t* buffer_out );

/*
 * BleStack_IndicationFromController
 *
 * @brief This function gives an indication from Controller to the BLE stack.
 * It must be called on every reception of a packet from the Controller,
 * unless the packet is an expected response.
 * The input parameter is a buffer of bytes in the BLE standard format:
 * HCI event packet or ACL data packet.
 * This function returns a non-zero value if the indication packet cannot be
 * handled by the BLE stack. This is an error case that should not occur if
 * the Controller events FIFO and ACL data pool have been properly sized.
 * Note: After a call to this function, it is mandatory to trigger a call to
 * BleStack_Process().
 */
extern uint8_t BleStack_IndicationFromController( const uint8_t* buffer );

/*
 * BleStack_IsResponse
 *
 * @brief Utility function that can be called at reception of a packet from
 * Controller when a command has been sent, to check if the received packet
 * is the expected response. The input parameter is a buffer of bytes
 * containing the received packet in HCI format.
 * The function returns 1 if the packet is the response, 0 otherwise.
 */
extern uint8_t BleStack_IsResponse( const uint8_t* buffer );

/*
 * BLECB_Indication
 *
 * @brief Callback function called by the BLE stack (from BleStack_Process()
 * context) to send an indication to the application. This indication is a
 * standard BLE packet, which can be an ACI or HCI event.
 * The packet data is obtained via the "data" pointer and "length" in bytes.
 * The parameters "ext_data" and "ext_length" are not used.
 */
extern uint8_t BLECB_Indication( const uint8_t* data,
                                 uint16_t length,
                                 const uint8_t* ext_data,
                                 uint16_t ext_length );

/*
 * BLECB_RequestToController
 *
 * @brief Callback function called by the BLE stack (from BleStack_Init(),
 * BleStack_Process(), BleStack_Request(), or any HCI/ACI command) to send a
 * request to the Controller. This request is a standard BLE packet, which can
 * be an HCI command or ACL data.
 * The packet data is obtained via the "data" pointer and "length" in bytes.
 * If a response is expected ("response_flag" != 0), the response data must be
 * returned via "data", which must be populated with a standard BLE response
 * event packet. In this case, the function is blocking, and the result must
 * always be a success (i.e. the return value shall be BLE_STATUS_SUCCESS).
 * If no response is expected ("response_flag" == 0), the return value shall be
 * BLE_STATUS_SUCCESS if the request was successfully registered for
 * transmission to the controller. Otherwise, if the request cannot be
 * processed, the function shall return a value other than BLE_STATUS_SUCCESS.
 */
extern uint8_t BLECB_RequestToController( uint8_t* data,
                                          uint16_t length,
                                          uint8_t response_flag );


#endif /* BLESTACK_H__ */
