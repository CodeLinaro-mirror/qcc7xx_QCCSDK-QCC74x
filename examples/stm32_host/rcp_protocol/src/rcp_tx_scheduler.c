#include "rcp_tx_scheduler.h"
#include "rcp_log.h"
#include "rcp_os.h"

#include <string.h>

#define MAX_TX_EVENTS RCP_MAX_TX_EVENTS

static rcp_queue_t tx_queue;
static bool tx_queue_initialized;

void init_tx_queue()
{
	if (tx_queue_initialized) {
		return;
	}

	if (!rcp_os_queue_init(&tx_queue, sizeof(rcp_schedule_evt_t), MAX_TX_EVENTS)) {
		RCP_LOG_ERR("Failed to initialize TX queue");
		return;
	}

	tx_queue_initialized = true;
}

bool schedule_tx(enum rcp_bus bus, enum rcp_msg_type stream, buffer_mgmt_elem_t *msg_buffer, uint32_t timeout_ms)
{
	rcp_schedule_evt_t event = {
		.stream = stream,
		.data_xfer = true,
		.msg_buffer = msg_buffer,
	};

	if (!tx_queue_initialized) {
		RCP_LOG_ERR("TX queue not initialized");
		return false;
	}
	if (rcp_os_queue_send(&tx_queue, &event, timeout_ms) != 0) {
		RCP_LOG_ERR("No available TX event slots, Used=%d", MAX_TX_EVENTS);
		return false;
	}

	rcp_bus_signal_tx_pending(bus);
	return true;
}

bool get_tx_event(rcp_schedule_evt_t *event, uint32_t timeout_ms)
{
	if (!tx_queue_initialized || !event) {
		return false;
	}

	return (rcp_os_queue_receive(&tx_queue, event, timeout_ms) == 0);
}

bool peek_tx_event(rcp_schedule_evt_t *event, uint32_t timeout_ms)
{
	if (!tx_queue_initialized || !event) {
		return false;
	}

	return (rcp_os_queue_peek(&tx_queue, event, timeout_ms) == 0);
}

bool tx_event_is_empty(void)
{
	if (!tx_queue_initialized) {
		return true;
	}

	return (rcp_os_queue_messages_waiting(&tx_queue) == 0);
}

int tx_event_pending_count(void)
{
	if (!tx_queue_initialized) {
		return 0;
	}

	return rcp_os_queue_messages_waiting(&tx_queue);
}

int tx_event_capacity(void)
{
	return MAX_TX_EVENTS;
}
