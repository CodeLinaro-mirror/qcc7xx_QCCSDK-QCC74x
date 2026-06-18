#ifndef __RCP_TX_SCHEDULER_H__
#define __RCP_TX_SCHEDULER_H__

#include "buffer_mgmt_ctrl.h"
#include "rcp_bus.h"
#include "rcp_os.h"

typedef struct rcp_schedule_evt_t evt_handle_t;

typedef struct rcp_schedule_evt_t {
	uint8_t stream;
	uint8_t msg_subtype;
	uint8_t msg_flags;
	buffer_mgmt_elem_t *msg_buffer;
	uint32_t enqueue_ticks;
} rcp_schedule_evt_t;

/* Reusable TX scheduler instance. Each instance owns its own queue and
 * signals a caller-chosen bus event flag whenever a new event is pushed. */
typedef struct rcp_tx_sch_t {
	rcp_queue_t queue;
	uint16_t    depth;
	uint8_t     signal_flag;
    uint8_t     bus;
} rcp_tx_sch_t;

void init_tx_queue(rcp_tx_sch_t *sch, enum rcp_bus bus, uint16_t depth, uint8_t signal_flag);

bool schedule_tx(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event, uint32_t timeout_ms);

bool get_tx_event(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event, uint32_t timeout_ms);
bool requeue_tx_event_front(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event);
bool peek_tx_event(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event, uint32_t timeout_ms);
bool tx_event_is_empty(rcp_tx_sch_t *sch);
int tx_event_pending_count(rcp_tx_sch_t *sch);
int tx_event_capacity(rcp_tx_sch_t *sch);

#endif
