#include "rcp_tx_scheduler.h"
#include "rcp_stat.h"
#include "rcp_log.h"
#include "rcp_os.h"

#include <string.h>

void init_tx_queue(rcp_tx_sch_t *sch, enum rcp_bus bus, uint16_t depth, uint8_t signal_flag)
{
	if (sch == NULL || depth == 0U) {
		return;
	}

	if (!rcp_os_queue_init(&sch->queue, sizeof(rcp_schedule_evt_t), depth)) {
		RCP_LOG_ERR("Failed to initialize TX queue");
		return;
	}

	sch->depth       = depth;
	sch->signal_flag = signal_flag;
	sch->bus         = bus;
}

bool schedule_tx(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event, uint32_t timeout_ms)
{
	rcp_schedule_evt_t queued;

	if (sch == NULL || event == NULL || event->msg_buffer == NULL) {
		return false;
	}

	queued = *event;
	queued.enqueue_ticks = RCP_TX_PROF_QUEUE_STAMP();

	if (rcp_os_queue_send(&sch->queue, &queued, timeout_ms) != 0) {
		RCP_LOG_ERR("No available TX event slots, depth=%lu",
			(unsigned long)sch->depth);
		return false;
	}

	__rcp_bus_set_flags(sch->bus, sch->signal_flag, 0);
	return true;
}

bool get_tx_event(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event, uint32_t timeout_ms)
{
	if (sch == NULL || event == NULL) {
		return false;
	}

	return (rcp_os_queue_receive(&sch->queue, event, timeout_ms) == 0);
}

bool requeue_tx_event_front(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event)
{
	if (sch == NULL || event == NULL) {
		return false;
	}

	return (rcp_os_queue_send_front(&sch->queue, event, 0) == 0);
}

bool peek_tx_event(rcp_tx_sch_t *sch, rcp_schedule_evt_t *event, uint32_t timeout_ms)
{
	if (sch == NULL || event == NULL) {
		return false;
	}

	return (rcp_os_queue_peek(&sch->queue, event, timeout_ms) == 0);
}

bool tx_event_is_empty(rcp_tx_sch_t *sch)
{
	if (sch == NULL) {
		return true;
	}

	return (rcp_os_queue_messages_waiting(&sch->queue) == 0);
}

int tx_event_pending_count(rcp_tx_sch_t *sch)
{
	if (sch == NULL) {
		return 0;
	}

	return rcp_os_queue_messages_waiting(&sch->queue);
}

int tx_event_capacity(rcp_tx_sch_t *sch)
{
	if (sch == NULL) {
		return 0;
	}

	return (int)sch->depth;
}
