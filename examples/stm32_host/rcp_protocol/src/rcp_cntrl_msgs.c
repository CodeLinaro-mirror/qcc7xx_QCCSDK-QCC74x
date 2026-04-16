#include "rcp_cntrl_msgs.h"
#include "rcp_interface.h"
#include "rcp_mux_dispatcher.h"
#include "rcp_log.h"

#include <stdlib.h>

static cntrl_msg_handler_t registered_cntrl_msg_hdlr; // Custom control msg handler
static uint8_t mcu_capabilities;                      // Flags indicating MCU capabilities

static struct {
	uint8_t stream;
	bool active;
	rcp_rx_event_handler_t saved_handler;
} loopback;

static uint8_t handle_loopback_data_msg(uint8_t event_type, buffer_mgmt_elem_t *buffer_ptr)
{
	// Just send response with received data
	return RCP_RX_NO_ACTION;
}

static void reset_loopback_state() {
	loopback.stream = RCP_MSG_TYPE_NONE;
	loopback.active = false;
	loopback.saved_handler = NULL;
}

bool send_looback_ind(uint8_t stream, uint8_t state)
{
	return false;
}

void init_cntrl_status()
{
	registered_cntrl_msg_hdlr = NULL;
	reset_loopback_state();
}

void init_mcu_capability(uint8_t capabilities)
{
	mcu_capabilities = capabilities | RCP_MCU_READY_FLAG;
}

void register_cntrl_msg_hdlr(cntrl_msg_handler_t handler)
{
	registered_cntrl_msg_hdlr = handler;
}

bool send_capability_request()
{
	return false;
}
