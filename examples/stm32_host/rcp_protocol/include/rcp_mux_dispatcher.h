#ifndef __RCP_MUX_DISPATCHER_H__
#define __RCP_MUX_DISPATCHER_H__

#include "buffer_mgmt_ctrl.h"
#include "rcp_interface.h"

void init_dispatcher();

bool register_data_msg_handler(uint8_t stream, rcp_rx_event_handler_t handler);

bool dispatch_msg(rcp_msg_ptr_t message, lnk_sts_t *lnk_sts_ptr, bool upload);

rcp_rx_event_handler_t get_rx_handler(uint8_t stream);

#endif
