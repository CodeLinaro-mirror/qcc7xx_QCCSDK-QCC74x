#ifndef __RCP_TX_SCHEDULER_H__
#define __RCP_TX_SCHEDULER_H__

#include "buffer_mgmt_ctrl.h"
#include "rcp_bus.h"

typedef struct rcp_schedule_evt_t evt_handle_t;

typedef struct rcp_schedule_evt_t {
	uint8_t stream;
	bool data_xfer;
	buffer_mgmt_elem_t *msg_buffer;
} rcp_schedule_evt_t;

void init_tx_queue();

bool schedule_tx(enum rcp_bus bus, enum rcp_msg_type stream, buffer_mgmt_elem_t *msg_buffer, uint32_t timeout_ms);

bool get_tx_event(rcp_schedule_evt_t *event, uint32_t timeout_ms);
bool peek_tx_event(rcp_schedule_evt_t *event, uint32_t timeout_ms);
bool tx_event_is_empty(void);
int tx_event_pending_count(void);
int tx_event_capacity(void);

#endif
