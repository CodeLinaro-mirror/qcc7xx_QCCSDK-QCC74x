#ifndef _RCP_DEFINITIONS_H_
#define _RCP_DEFINITIONS_H_

#include <stdint.h>

// Data sizes
#define RCP_HDR_SIZE            4
#define RCP_PAYLOAD_LENGTH_SIZE 2
#define RCP_ACK_MSG_SIZE        1
#define RCP_NACK_MSG_SIZE       3

// Max Sequence Number value/roll over
#define RPC_SEQ_NUM_MAX 0x07

// Byte positions from LSB
#define RPC_POS_MSG_INFO     0x00
#define RCP_POS_SEQ_NUMS     0x01
#define RCP_PAD_BYTE         0x01
#define RPC_POS_PAYLOAD_SIZE 0x02
#define RCP_PAYLOAD_POS      0x04

// Common Header Mask
#define RCP_MASK_STREAM   0XF0
#define RCP_MASK_MSG_TYPE 0x0F
#define RCP_MASK_FORMAT   0X0C
#define RCP_MASK_MSG_ID   0x03

// Bit shift
#define RCP_BIT_SHFT_STREAM 0x04
#define RCP_BIT_SHFT_FORMAT 0x02
#define RCP_BIT_SHFT_MSG_ID 0x00

// Common Header Mask
#define RCP_MASK_TSN 0XE0
#define RCP_MASK_RSN 0X1C
#define RCP_MASK_ACK 0x02

// Bit shifts
#define RCP_BIT_SHFT_TSN 0x05
#define RCP_BIT_SHFT_RSN 0x02
#define RCP_BIT_SHFT_ACK 0x01

// Msg Ids
// Data Messages
#define RPC_MSG_ID_INIT_DATA 0x00
#define RPC_MSG_ID_DATA_XFR  0x01
#define RPC_MSG_ID_ACK       0x02
#define RPC_MSG_ID_NACK      0x03

// Control
#define RPC_MSG_ID_CONTROL    0x00
#define RPC_MSG_ID_STATUS_IND 0x01
#define RPC_MSG_ID_DEBUG_REQ  0x02
#define RPC_MSG_ID_DEBUG_RESP 0x03

#define RPC_TYPE(FMT, MSG_ID)                                                                      \
	(((FMT) << RCP_BIT_SHFT_FORMAT) | (((MSG_ID) << (RCP_BIT_SHFT_MSG_ID)) & RCP_MASK_MSG_TYPE))

// Message Type - combines format with Id

// Data
//#define RCP_MSG_TYPE_INIT_DATA RPC_TYPE(RPC_MSG_FORMAT_DATA, RPC_MSG_ID_INIT_DATA)
//#define RCP_MSG_TYPE_DATA_XFER RPC_TYPE(RPC_MSG_FORMAT_DATA, RPC_MSG_ID_DATA_XFR)
//#define RCP_MSG_TYPE_ACK       RPC_TYPE(RPC_MSG_FORMAT_DATA, RPC_MSG_ID_ACK)
//#define RCP_MSG_TYPE_NACK      RPC_TYPE(RPC_MSG_FORMAT_DATA, RPC_MSG_ID_NACK)
//
//// Control
//#define RCP_MSG_TYPE_CONTROL    RPC_TYPE(RPC_MSG_FORMAT_CONTROL, RPC_MSG_ID_CONTROL)
//#define RCP_MSG_TYPE_STATUS_IND RPC_TYPE(RPC_MSG_FORMAT_CONTROL, RPC_MSG_ID_STATUS_IND)

// Control Subtypes
#define RCP_CNTRL_CAP_REQ 		0x00 // Request MCU Capabilities
#define RCP_CNTRL_LOOPBACK_REQ	0x01 // Request RCP stream loopback

// Status Indication Subtypes
#define RCP_CNTRL_CAP_IND 		0x01 // Indicates device capabilities
#define RCP_CNTRL_LOOPBACK_IND	0x01 // Indicates stream loopback state change

// Debug
#define RCP_MSG_TYPE_DEBUG_REQ  RPC_TYPE(RPC_MSG_FORMAT_CONTROL, RPC_MSG_ID_DEBUG_REQ)
#define RCP_MSG_TYPE_DEBUG_RESP RPC_TYPE(RPC_MSG_FORMAT_CONTROL, RPC_MSG_ID_DEBUG_RESP)

// Debug Sub type
#define RCP_DBG_MSG_STREAM_LOOPBACK 0x00

#endif
