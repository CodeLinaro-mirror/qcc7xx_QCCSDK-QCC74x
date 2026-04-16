#ifndef __RCP_CNTRL_MSGS_H__
#define __RCP_CNTRL_MSGS_H__

#include "buffer_mgmt_ctrl.h"
#include "rcp_interface.h"

// Capabilities

// Flag bit posistions
#define RCP_MCU_READY_FLAG         0x01
#define RCP_WIFI_CAPABILITY_FLAG   0x02
#define RCP_BT_CAPABILITY_FLAG     0x04
#define RCP_THREAD_CAPABILITY_FLAG 0x08


// Loopback

// Payload Byte posistions
#define RCP_MCU_LOOP_SIZE	0x02

#define RCP_MCU_LOOP_ACTION		0x00
#define RCP_MCU_LOOP_STREAM     0x01

// LOOP STATE Values
#define RCP_MCU_LOOP_ACTIVE     0x01	// Loopback stream/Indicates stream looped back
#define RCP_MCU_LOOP_INACTIVE	0x00	// Stop stream loop back/Indicate stream now normal operation

// Definition of control message handler
typedef void (*cntrl_msg_handler_t)(uint8_t msg_type, uint8_t subtype,
				    buffer_mgmt_elem_t *payload_ptr);

void init_cntrl_status();

void handle_cntrl_msgs(rcp_msg_ptr_t cntrl_msg);

void register_cntrl_msg_hdlr(cntrl_msg_handler_t handler);

void init_mcu_capability(uint8_t capbilities);

bool send_capability_request();

bool send_loopback_request(uint8_t stream);

bool send_loopback_cancel();

#endif
