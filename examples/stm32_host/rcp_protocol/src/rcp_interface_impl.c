#include "rcp_bus.h"
#include "rcp_definitions.h"
#include "rcp_interface_impl.h"
#include "rcp_cntrl_msgs.h"
#include "rcp_protocol.h"
#include "rcp_mux_dispatcher.h"
#include "rcp_tx_scheduler.h"
#include "rcp_config.h"
#include "rcp_os.h"
#include "rcp_log.h"
#include "wifi/rcp_wifi_mgmt.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdlib.h>

#define BUS_TX_RETRIES 3
#define BUS_TX_RETRY_DELAY_MS 1
#define STREAM_TASK_STACK_SIZE 4096
#define STREAM_TASK_PRIORITY (30)
#define RCP_PROTO_META_SIZE ((uint16_t)(sizeof(rcp_pro_packt_t) - sizeof(rcp_msg_packt_t)))
#define RCP_PAYLOAD_SIZE(msg) \
	((uint16_t)(((msg)->msg_packt.len >= RCP_PROTO_META_SIZE) ? ((msg)->msg_packt.len - RCP_PROTO_META_SIZE) : 0U))
#define RCP_FLAG_IS_DATA(flag) (((flag) == RCP_FLAG_DNLD_DATA) || ((flag) == RCP_FLAG_UPLD_DATA))
#define RCP_FLAG_IS_HANDSHAKE(flag) (((flag) >= RCP_FLAG_HOST_RESET) && ((flag) <= RCP_FLAG_DEVICE_STOP))

#define get_stream_status(stream) (((stream) >= RCP_MSG_TYPE_MAX)?NULL:&g_stream_status[(stream)])

static uint8_t g_rcp_rx_buffer_mem[RCP_RX_NUM_BUFFERS][FRAME_BUFF_TOTALSIZE];
static lnk_sts_t g_stream_status[RCP_MSG_TYPE_MAX];
static buffer_mgmt_ctrl_t *g_rx_buffer_ctrl;
static enum rcp_bus g_rcp_bus = RCP_BUS_MAX;
static rcp_thread_t g_event_loop_thread;

static int rcp_process_msg(lnk_sts_t *hsm, rcp_msg_t *msg);
static void rcp_rx_buffer_credit_put(buffer_mgmt_elem_t *buffer_elem, void *arg);
static bool bus_flag_is_set(enum rcp_bus bus, uint8_t flag);
static void rcp_reset_stream_counters(lnk_sts_t *lnk);
static bool process_rx_once(enum rcp_bus bus);
static bool process_pending_tx(enum rcp_bus bus);
static bool process_pending_rx_credit_updates(enum rcp_bus bus);
static void rcp_event_loop_entry(void *arg);

static const char *rcp_stream_name(uint8_t stream)
{
	switch (stream) {
	case RCP_MSG_TYPE_NONE:
		return "NONE";
	case RCP_MSG_TYPE_TEST_1:
		return "TEST_1";
	case RCP_MSG_TYPE_TEST_2:
		return "TEST_2";
	case RCP_MSG_TYPE_TEST_3:
		return "TEST_3";
	case RCP_MSG_TYPE_SYS:
		return "SYS";
	case RCP_MSG_TYPE_WIFI:
		return "WIFI";
	case RCP_MSG_TYPE_ETH:
		return "ETH";
	case RCP_MSG_TYPE_TTY:
		return "TTY";
	case RCP_MSG_TYPE_NETLINK:
		return "NETLINK";
	case RCP_MSG_TYPE_BT_HCI:
		return "BT_HCI";
	default:
		return "?";
	}
}

static const char *rcp_link_state_name(link_state_t state)
{
	switch (state) {
	case LINK_STATE_IDLE:
		return "IDLE";
	case LINK_STATE_SYNCING:
		return "SYNCING";
	case LINK_STATE_SYNC_PENDING:
		return "SYNC_PENDING";
	case LINK_STATE_ACTIVE:
		return "ACTIVE";
	case LINK_STATE_FAULT:
		return "FAULT";
	case LINK_STATE_HSTA_RESET:
		return "HSTA_RESET";
	case LINK_STATE_HSTA_HOST_READY:
		return "HSTA_HOST_READY";
	case LINK_STATE_HSTA_DEVICE_READY:
		return "HSTA_DEVICE_READY";
	case LINK_STATE_HSTA_DEVICE_RUN:
		return "HSTA_DEVICE_RUN";
	case LINK_STATE_HSTA_ERROR:
		return "HSTA_ERROR";
	default:
		return "UNKNOWN";
	}
}

static const char *rcp_flag_name(uint8_t flag)
{
	switch (flag) {
	case RCP_FLAG_NONE:
		return "NONE";
	case RCP_FLAG_DNLD_DATA:
		return "DNLD_DATA";
	case RCP_FLAG_UPLD_DATA:
		return "UPLD_DATA";
	case RCP_FLAG_HOST_RESET:
		return "HOST_RESET";
	case RCP_FLAG_HOST_READY:
		return "HOST_READY";
	case RCP_FLAG_HOST_STOP:
		return "HOST_STOP";
	case RCP_FLAG_DEVICE_RESET:
		return "DEVICE_RESET";
	case RCP_FLAG_DEVICE_READY:
		return "DEVICE_READY";
	case RCP_FLAG_DEVICE_START:
		return "DEVICE_START";
	case RCP_FLAG_DEVICE_STOP:
		return "DEVICE_STOP";
	case RCP_FLAG_CREDIT_UPDATE:
		return "CREDIT_UPDATE";
	default:
		return "?";
	}
}

static unsigned long rcp_debug_queue_free_slots(const rcp_queue_t *queue)
{
#if defined(RCP_OS_FREERTOS)
	if (queue == NULL || queue->_queue == NULL) {
		return 0;
	}

	return (unsigned long)uxQueueSpacesAvailable(queue->_queue);
#else
	(void)queue;
	return 0;
#endif
}

static unsigned long rcp_debug_sem_count(const rcp_sem_t *sem)
{
#if defined(RCP_OS_FREERTOS)
	if (sem == NULL || sem->_sem == NULL) {
		return 0;
	}

	return (unsigned long)uxQueueMessagesWaiting((QueueHandle_t)sem->_sem);
#else
	(void)sem;
	return 0;
#endif
}

static void rcp_dump_credit_state(const char *name, const nh_credit_t *credit, bool enabled, bool is_tx)
{
	uint8_t delta;

	if (credit == NULL) {
		printf("    %s: NULL\n", name);
		return;
	}

	delta = is_tx ?
		(uint8_t)(credit->credit_limit_cnt - credit->credit_consumed_cnt) :
		(uint8_t)(credit->credit_consumed_cnt - credit->credit_limit_cnt);

	printf("    %s: %s\n", name, enabled ? "enabled" : "disabled");
	printf("      init_credit: %u, threshold: %u, credit_limit_cnt: %u, credit_consumed_cnt: %u\n",
	       credit->init_credit,
	       credit->threshold,
	       credit->credit_limit_cnt,
	       credit->credit_consumed_cnt);
	printf("      wait_sem: %p, wait_sem_cnt: %lu, %s: %u\n",
	       (void *)credit->wait_sem,
	       rcp_debug_sem_count(credit->wait_sem),
	       is_tx ? "credit_left" : "pending_update",
	       delta);
}

void init_rx_queue()
{
	buffer_mgmt_ctrl_init_cfg_t cfg = {
		.frame_buff = g_rcp_rx_buffer_mem,
		.frame_elem_cnt = RCP_RX_NUM_BUFFERS,
		.frame_elem_size = sizeof(g_rcp_rx_buffer_mem[0]),
		.name = "rcp_rx",
	};
	g_rx_buffer_ctrl = buffer_mgmt_create(&cfg);
}

lnk_sts_t *link_get_status(uint8_t stream)
{
	return get_stream_status(stream);
}

static void rcp_rx_buffer_credit_put(buffer_mgmt_elem_t *buffer_elem, void *arg)
{
	lnk_sts_t *lnk;
	uint8_t stream;

	(void)buffer_elem;

	stream = (uint8_t)(uintptr_t)arg;

	lnk = get_stream_status(stream);
	if (lnk == NULL) {
		return;
	}

	nh_rx_credit_reclaim(&lnk->flow_ctrl);
	if ((lnk->state == LINK_STATE_HSTA_DEVICE_RUN) &&
	    nh_rx_credit_need_update(&lnk->flow_ctrl)) {
		rcp_bus_signal_tx_pending(lnk->bus);
	}
}

static bool get_rx_data(rcp_msg_ptr_t msg_ptr, enum rcp_bus bus)
{
	buffer_mgmt_elem_t *rx_buffer = NULL;

	if (!g_rx_buffer_ctrl) {
		return false;
	}

	if (buffer_mgmt_alloc_elem(g_rx_buffer_ctrl, &rx_buffer, 0) != 0) {
		RCP_LOG_ERR("RX buffer alloc err\r\n");
		return false;
	}

	int ret = rcp_bus_read(bus, (rcp_msg_packt_t *)BUFFER_MGMT_DATA_PTR(rx_buffer));

	if (ret <= 0) {
		if (ret != RCP_BUS_ERR_EMPTY) {
			RCP_LOG_ERR("RX bus failure err=%d", ret);
		}
	} else {
		rx_buffer->data_size = ret;
		if (decode_rcp_msg(msg_ptr, rx_buffer)) {
			return true;
		}
		RCP_LOG_ERR("Rx Decode failed");
	}

	if (rx_buffer) {
		buffer_mgmt_free_elem(rx_buffer);
		rx_buffer = NULL;
	}
	return false;
}

static int rcp_credit_get(lnk_sts_t *hsm, uint32_t timeout_ms)
{
	if (hsm->state != LINK_STATE_HSTA_DEVICE_RUN) {
		return 0;
	}

	return nh_tx_credit_get(&hsm->flow_ctrl, timeout_ms);
}

static int rcp_credit_put(lnk_sts_t *hsm)
{
	if (hsm->state != LINK_STATE_HSTA_DEVICE_RUN) {
		return 0;
	}

	nh_tx_credit_put(&hsm->flow_ctrl);
	return 0;
}

static int rcp_credit_limit_update(lnk_sts_t *hsm, uint8_t credit_new)
{
	RCP_LOG_DBG("Channel %d: credit limit update now_credit:%d new_credit:%d",
			hsm->stream, hsm->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt, credit_new);
	(void)nh_tx_credit_update(&hsm->flow_ctrl, credit_new);
	return 0;
}

link_result_t send_rcp_cmd(lnk_sts_t *lnk, enum rcp_flag flag)
{
	rcp_msg_t send_msg;
    buffer_mgmt_elem_t *tx_buffer = NULL;

	if (!lnk || !lnk->buffer_ctrl) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	if (buffer_mgmt_alloc_elem(lnk->buffer_ctrl, &tx_buffer, 1000) != 0) {
		RCP_LOG_ERR("Failed to allocate buffer for CMD");
		return -1;
	}

	build_rcp_msg(&send_msg, lnk->stream, flag, tx_buffer);

	if (!send_rcp_msg(&send_msg, 1000)) {
		buffer_mgmt_free_elem(tx_buffer);
		return RCP_LNK_ERR_TX_NO_RESOURCES;
	}
	return RCP_LNK_SUCCESS;
}

link_result_t link_request(uint8_t stream, buffer_mgmt_elem_t *tx_buffer,
			   rcp_rx_event_handler_t response_cb, uint32_t timeout_ms)
{
	lnk_sts_t *lnk_stat_ptr = get_stream_status(stream);

	rcp_msg_t send_msg;

	if (!lnk_stat_ptr) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	if (!lnk_stat_ptr->buffer_ctrl) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	if (!tx_buffer) {
		return RCP_LINK_ERR_INVALID_BUFFER;
	}

    //RCP_LOG_INF("stream:%d pending_tx %d %d\r\n", stream,
    //		lnk_stat_ptr->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt,
	//		lnk_stat_ptr->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_consumed_cnt);
	if (rcp_credit_get(lnk_stat_ptr, timeout_ms) < 0) {
		RCP_LOG_DBG("RCP TX busy %d %d\r\n",
				lnk_stat_ptr->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt,
				lnk_stat_ptr->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_consumed_cnt);
		return RCP_LNK_TIMEOUT;
	}

	build_rcp_msg(&send_msg, lnk_stat_ptr->stream, RCP_FLAG_DNLD_DATA, tx_buffer);

	if (!send_rcp_msg(&send_msg, timeout_ms)) {
		return RCP_LNK_ERR_TX_NO_RESOURCES;
	}
	if (response_cb) {
		register_data_msg_handler(stream, response_cb);
	}
	return RCP_LNK_SUCCESS;
}

link_result_t link_send(uint8_t stream, buffer_mgmt_elem_t *tx_buffer, uint32_t timeout_ms)
{
	return link_request(stream, tx_buffer, NULL, timeout_ms);
}

link_result_t link_register_rx_handler(uint8_t stream, rcp_rx_event_handler_t response_cb)
{
	if (!register_data_msg_handler(stream, response_cb)) {
		return RCP_LNK_ERROR;
	}
	return RCP_LNK_SUCCESS;
}

link_result_t link_receive(uint8_t stream, buffer_mgmt_elem_t **rx_buffer, uint32_t timeout_ms)
{
	lnk_sts_t *lnk_sts_ptr = get_stream_status(stream);
	rcp_msg_t rx_msg = {0};

	if (!lnk_sts_ptr || !rx_buffer) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	*rx_buffer = NULL;

	if (rcp_os_queue_receive(&lnk_sts_ptr->rx_queue, &rx_msg, timeout_ms) != 0) {
		return (timeout_ms == 0U) ? RCP_LNK_NO_RX_DATA : RCP_LNK_TIMEOUT;
	}

	*rx_buffer = rx_msg.payload_ptr;

	return RCP_LNK_SUCCESS;
}

link_result_t link_reset(uint8_t stream, buffer_mgmt_elem_t *tx_buffer)
{
	lnk_sts_t *lnk_stat_ptr = get_stream_status(stream);

	if (lnk_stat_ptr == NULL) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	lnk_stat_ptr->state = LINK_STATE_HSTA_RESET;
	return link_request(stream, tx_buffer, NULL, RCP_OS_WAIT_FOREVER);
}

static bool link_active_detect(enum rcp_bus bus)
{
	struct rcp_bus_caps caps;

	if (rcp_bus_get_caps(bus, &caps) != RCP_BUS_OK) {
		return true;
	}
	if (!caps.active) {
		//rcp_bus_sdio_master_init();
        printf("link not active\r\n");
		return false;
	}
	return true;
}

/* Send message over bus with retries in case of failure. */
static bool tx_msg_with_retries(enum rcp_bus bus, buffer_mgmt_elem_t *msg)
{
	enum rcp_bus_status status = 0;
	uint8_t retries = BUS_TX_RETRIES;

	do {
		status = tx_rcp_msg(bus, msg);
		if (status != RCP_BUS_OK) {
			RCP_LOG_INF("Send message over bus. Retrying.");
			rcp_os_delay_ms(BUS_TX_RETRY_DELAY_MS);
		}
	} while(retries-- && (status != RCP_BUS_OK));

	if (status != RCP_BUS_OK && status != RCP_BUS_ERR_EMPTY) {
		link_active_detect(bus);
	}
	return (status == RCP_BUS_OK);
}

static bool bus_flag_is_set(enum rcp_bus bus, uint8_t flag)
{
	uint8_t flags = 0;

	if (RCP_BUS_OK != rcp_bus_get_flags(bus, &flags)) {
		return false;
	}

	return ((flags & flag) != 0U);
}

void run_event_loop(enum rcp_bus bus)
{
	while (true) {
		uint8_t ready_flags = 0;

		if (RCP_BUS_OK != rcp_bus_wait_for_any_flags(bus,
							      RCP_BUS_FLAG_RX_DATA |
								  RCP_BUS_FLAG_TX_PENDING,
							      RCP_OS_WAIT_FOREVER, &ready_flags)) {
			continue;
		}

		if ((ready_flags & RCP_BUS_FLAG_RX_DATA)) {
			/* RX flag transitions are reported through the bus core; the event loop only drains. */
			while (process_rx_once(bus)) {
			}
		}

		if ((ready_flags & RCP_BUS_FLAG_TX_PENDING) || (!tx_event_is_empty())) {
			while (process_pending_tx(bus)) {
			}
		}
	}
}

static void rcp_event_loop_entry(void *arg)
{
	run_event_loop((enum rcp_bus)(uintptr_t)arg);
}

static bool process_pending_tx(enum rcp_bus bus)
{
	rcp_schedule_evt_t tx_event = {0};
	lnk_sts_t *lnk;
	rcp_pro_packt_t *tx_msg;
	uint8_t flag;
	uint16_t tx_size;

	if (!peek_tx_event(&tx_event, 0)) {
		return process_pending_rx_credit_updates(bus);
	}

	lnk = get_stream_status(tx_event.stream);
	tx_msg = (rcp_pro_packt_t *)buffer_mgmt_data_ptr(tx_event.msg_buffer);
	flag = tx_msg->flag;
	tx_size = RCP_PAYLOAD_SIZE(tx_msg);

	if (!tx_msg_with_retries(bus, tx_event.msg_buffer)) {
		if (flag == RCP_FLAG_DNLD_DATA) {
			rcp_credit_put(lnk);
		}
		RCP_LOG_WRN("Failed to send message over bus, dropping.");
		return false;
	}

	if (lnk != NULL) {
		if (!RCP_FLAG_IS_HANDSHAKE(flag)) {
			if (RCP_FLAG_IS_DATA(flag)) {
				lnk->tx_data_packets++;
				lnk->tx_data_bytes += tx_size;
			} else {
				lnk->tx_ctrl_packets++;
				lnk->tx_ctrl_bytes += tx_size;
			}
		}
	}

	get_tx_event(&tx_event, 0);

	if (tx_event.msg_buffer) {
		buffer_mgmt_free_elem(tx_event.msg_buffer);
		tx_event.msg_buffer = NULL;
	}

	return true;
}

static bool process_pending_rx_credit_updates(enum rcp_bus bus)
{
	int i;

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];

		if (lnk->stream != i || lnk->bus != bus ||
		    lnk->state != LINK_STATE_HSTA_DEVICE_RUN) {
			continue;
		}

		if (!nh_rx_credit_need_update(&lnk->flow_ctrl)) {
			continue;
		}

		return (send_rcp_cmd(lnk, RCP_FLAG_CREDIT_UPDATE) == RCP_LNK_SUCCESS);
	}

	return false;
}

static bool process_rx_once(enum rcp_bus bus)
{
	rcp_msg_t rx_msg = {0};

	if (!get_rx_data(&rx_msg, bus)) {
		return false;
	}

	lnk_sts_t *lnk_sts_ptr = get_stream_status(rx_msg.msg_type);
	uint16_t rx_size = (rx_msg.payload_ptr != NULL) ? rx_msg.payload_ptr->data_size : 0;

	if (!lnk_sts_ptr) {
		RCP_LOG_WRN("Discarding message on invalid stream=%x", rx_msg.msg_type);
		if (rx_msg.payload_ptr) {
			buffer_mgmt_free_elem(rx_msg.payload_ptr);
			rx_msg.payload_ptr = NULL;
		}
		return true;
	}

	if ((rx_msg.flag == RCP_FLAG_UPLD_DATA) && (rx_msg.payload_ptr != NULL)) {
		buffer_mgmt_elem_set_free_cb(rx_msg.payload_ptr, rcp_rx_buffer_credit_put,
					 (void *)(uintptr_t)rx_msg.msg_type);
	}

	if (!RCP_FLAG_IS_HANDSHAKE(rx_msg.flag)) {
		if (RCP_FLAG_IS_DATA(rx_msg.flag)) {
			lnk_sts_ptr->rx_data_packets++;
			lnk_sts_ptr->rx_data_bytes += rx_size;
		} else {
			lnk_sts_ptr->rx_ctrl_packets++;
			lnk_sts_ptr->rx_ctrl_bytes += rx_size;
		}
	}

	rcp_process_msg(lnk_sts_ptr, &rx_msg);

	return true;
}

link_state_t link_get_state(uint8_t stream)
{
	lnk_sts_t *lnk_stat_ptr = get_stream_status(stream);
	if (lnk_stat_ptr) {
		return lnk_stat_ptr->state;
	} else {
		return LINK_STATE_FAULT;
	}
}

static void rcp_reset_stream_counters(lnk_sts_t *lnk)
{
	if (lnk == NULL) {
		return;
	}

	lnk->tx_data_packets = 0;
	lnk->tx_data_bytes = 0;
	lnk->tx_ctrl_packets = 0;
	lnk->tx_ctrl_bytes = 0;
	lnk->rx_data_packets = 0;
	lnk->rx_data_bytes = 0;
	lnk->rx_ctrl_packets = 0;
	lnk->rx_ctrl_bytes = 0;
}

void rcp_reset_status_counters(void)
{
	for (int i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		rcp_reset_stream_counters(&g_stream_status[i]);
	}
}

void rcp_dump_status(void)
{
	uint8_t bus_flags = 0;
	struct rcp_bus_caps caps = {0};
	rcp_schedule_evt_t head_evt = {0};
	bool head_valid = false;
	unsigned long rx_pool_free = 0;
	unsigned long rx_pool_inflight = 0;
	unsigned long rx_pool_total = 0;
	int tx_pending = tx_event_pending_count();
	int tx_total = tx_event_capacity();

	printf("=== Host RCP Status Dump ===\n");
	printf("Basic Info:\n");
	printf("  bus: %d\n", g_rcp_bus);
	printf("  event_loop_thread: %p\n", g_event_loop_thread._thread);
	printf("  g_rx_buffer_ctrl: %p\n", g_rx_buffer_ctrl);

	if (g_rcp_bus < RCP_BUS_MAX) {
		(void)rcp_bus_get_flags(g_rcp_bus, &bus_flags);
		(void)rcp_bus_get_caps(g_rcp_bus, &caps);
	}

	printf("Bus State:\n");
	printf("  bus_flags: 0x%02X, rx_ready: %s, tx_pending: %s\n",
	       bus_flags,
	       (bus_flags & RCP_BUS_FLAG_RX_DATA) ? "true" : "false",
	       (bus_flags & RCP_BUS_FLAG_TX_PENDING) ? "true" : "false");
	printf("  bus_rx_len: %u, bus_mtu: %u\n",
	       (unsigned int)((g_rcp_bus < RCP_BUS_MAX) ? rcp_bus_get_rx_len(g_rcp_bus) : 0U),
	       (unsigned int)caps.mtu);

	printf("TX Scheduler:\n");
	printf("  tx_queue: used: %d/%d, free: %d\n",
	       tx_pending,
	       tx_total,
	       (tx_total >= tx_pending) ? (tx_total - tx_pending) : 0);
	head_valid = peek_tx_event(&head_evt, 0);
	if (head_valid && head_evt.msg_buffer != NULL) {
		rcp_pro_packt_t *head_msg = (rcp_pro_packt_t *)buffer_mgmt_data_ptr(head_evt.msg_buffer);
		printf("  tx_head: stream=%u(%s), flag=%u(%s), msg_buffer=%p\n",
		       head_evt.stream,
		       rcp_stream_name(head_evt.stream),
		       head_msg->flag,
		       rcp_flag_name(head_msg->flag),
		       head_evt.msg_buffer);
	} else {
		printf("  tx_head: none\n");
	}

	printf("Global RX Pool:\n");
	if (g_rx_buffer_ctrl != NULL) {
		rx_pool_free = (unsigned long)buffer_mgmt_pool_get_available(g_rx_buffer_ctrl);
		rx_pool_inflight = rcp_debug_queue_free_slots(&g_rx_buffer_ctrl->pool_queue);
		rx_pool_total = rx_pool_free + rx_pool_inflight;
		printf("  %s: %p (free: %lu/%lu, inflight: %lu)\n",
		       g_rx_buffer_ctrl->name ? g_rx_buffer_ctrl->name : "rcp_rx",
		       g_rx_buffer_ctrl,
		       rx_pool_free,
		       rx_pool_total,
		       rx_pool_inflight);
	} else {
		printf("  g_rx_buffer_ctrl: NULL\n");
	}

	printf("Streams:\n");
	for (int i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];
		unsigned long tx_pool_free = 0;
		unsigned long tx_pool_inflight = 0;
		unsigned long tx_pool_total = 0;
		unsigned long rx_queue_used = 0;
		unsigned long rx_queue_free = 0;

		if (lnk->buffer_ctrl == NULL && !lnk->rx_queue_enabled &&
		    lnk->state == LINK_STATE_IDLE && lnk->stream == 0U) {
			continue;
		}

		printf("  stream %d (%s):\n", i, rcp_stream_name(i));
        printf("    lnk: %p, state: %s(%d), error: %d, bus: %d, rx_wnd: %u\n",
		       lnk,
		       rcp_link_state_name(lnk->state),
		       lnk->state,
		       lnk->error,
		       lnk->bus,
		       lnk->rx_wnd);
		printf("    tag %d (%s):\n",
		       i,
		       rcp_stream_name(i));
		printf("           data pkt tx=%lu rx=%lu, bytes tx=%lu rx=%lu\n",
		       (unsigned long)lnk->tx_data_packets,
		       (unsigned long)lnk->rx_data_packets,
		       (unsigned long)lnk->tx_data_bytes,
		       (unsigned long)lnk->rx_data_bytes);
		printf("           ctrl pkt tx=%lu rx=%lu, bytes tx=%lu rx=%lu\n",
		       (unsigned long)lnk->tx_ctrl_packets,
		       (unsigned long)lnk->rx_ctrl_packets,
		       (unsigned long)lnk->tx_ctrl_bytes,
		       (unsigned long)lnk->rx_ctrl_bytes);

		if (lnk->buffer_ctrl != NULL) {
			tx_pool_free = (unsigned long)buffer_mgmt_pool_get_available(lnk->buffer_ctrl);
			tx_pool_inflight = rcp_debug_queue_free_slots(&lnk->buffer_ctrl->pool_queue);
			tx_pool_total = tx_pool_free + tx_pool_inflight;
			printf("    tx_buffer_ctrl: %p (%s) free: %lu/%lu, inflight: %lu\n",
			       lnk->buffer_ctrl,
			       lnk->buffer_ctrl->name ? lnk->buffer_ctrl->name : "?",
			       tx_pool_free,
			       tx_pool_total,
			       tx_pool_inflight);
		} else {
			printf("    tx_buffer_ctrl: NULL\n");
		}

		if (lnk->rx_queue_enabled) {
			rx_queue_used = (unsigned long)rcp_os_queue_messages_waiting(&lnk->rx_queue);
			rx_queue_free = rcp_debug_queue_free_slots(&lnk->rx_queue);
			printf("    rx_queue: used: %lu/%u, free: %lu\n",
			       rx_queue_used,
			       RCP_MAX_RX_EVENTS,
			       rx_queue_free);
		} else {
			printf("    rx_queue: disabled\n");
		}

		printf("    flow_ctrl: tag=%s, enable_mask=0x%02X\n",
		       lnk->flow_ctrl.tag ? lnk->flow_ctrl.tag : "?",
		       lnk->flow_ctrl.enable);
		rcp_dump_credit_state("tx_credit", &lnk->flow_ctrl.credit[NH_CREDIT_DIR_TX],
				      (lnk->flow_ctrl.enable & NH_CREDIT_EN_TX) != 0U, true);
		rcp_dump_credit_state("rx_credit", &lnk->flow_ctrl.credit[NH_CREDIT_DIR_RX],
				      (lnk->flow_ctrl.enable & NH_CREDIT_EN_RX) != 0U, false);
	}

	printf("=== End of Host RCP Status Dump ===\n");
}

static int rcp_process_msg(lnk_sts_t *hsm, rcp_msg_t *msg)
{
	uint8_t stream;
	bool upload = false;

	if (!hsm || !msg) {
		return -1;
	}

	stream = hsm->stream;
	uint8_t flag = msg->flag;

	/* Return to the initial state when DEVICE_RESET is received in any state. */
			if (flag == RCP_FLAG_DEVICE_RESET) {
			RCP_LOG_WRN("Channel %d: Received DEVICE_RESET, returning to RESET", stream);
			if (hsm->state != LINK_STATE_HSTA_RESET) {
				hsm->state = LINK_STATE_HSTA_RESET;
				nh_credit_reset(&hsm->flow_ctrl);
			}
		}

	/* State machine */
	switch (hsm->state) {
		//case LINK_STATE_HSTA_RESET:
		//    ret = send_rcp_cmd(stream, RCP_FLAG_HOST_READY);
		//    if (ret != RCP_LNK_SUCCESS) {
		//        hsm->state = LINK_STATE_HSTA_ERROR;
		//    } else {
		//        hsm->state = LINK_STATE_HSTA_HOST_READY;
		//    }
		//	break;

        case LINK_STATE_HSTA_HOST_READY:
            if (flag != RCP_FLAG_DEVICE_READY) {
                break;
            }
        //memcpy(hsm->device_mac, pro_packt->data, 6);
        //hsm->mac_valid = true;
            //RCP_LOG_INF("Channel %d: MAC %02X:%02X:%02X:%02X:%02X:%02X", stream,
            //	   hsm->device_mac[0], hsm->device_mac[1], hsm->device_mac[2],
            //	   hsm->device_mac[3], hsm->device_mac[4], hsm->device_mac[5]);
            hsm->state = LINK_STATE_HSTA_DEVICE_READY;
            break;

		case LINK_STATE_HSTA_DEVICE_READY:
			if (flag != RCP_FLAG_DEVICE_START) {
				break;
			}
            if (!msg->credit_update_flag || msg->credit_limit_cnt == 0) {
                RCP_LOG_ERR("Channel %d: DEVICE_START without credit", stream);
                hsm->state = LINK_STATE_HSTA_ERROR;
                return -1;
            }
            if (nh_credit_init(&hsm->flow_ctrl, NULL,
                       msg->credit_limit_cnt,
                       RCP_UPLD_CREDIT_UPDATE_THRESHOLD,
                       hsm->rx_wnd,
                       RCP_UPLD_CREDIT_UPDATE_THRESHOLD) != 0) {
                RCP_LOG_ERR("Channel %d: flow ctrl reinit failed", stream);
                hsm->state = LINK_STATE_HSTA_ERROR;
                return -1;
            }
            nh_credit_enable(&hsm->flow_ctrl, NH_CREDIT_EN_TX | NH_CREDIT_EN_RX);

            rcp_reset_stream_counters(hsm);
            hsm->state = LINK_STATE_HSTA_DEVICE_RUN;
            RCP_LOG_INF("Channel %d: DEVICE_RUN (credits=%d)", stream,
                hsm->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt);
            break;

		case LINK_STATE_HSTA_DEVICE_RUN:

			if (flag == RCP_FLAG_CREDIT_UPDATE && msg->credit_update_flag) {
				rcp_credit_limit_update(hsm, msg->credit_limit_cnt);
			}

			if (flag == RCP_FLAG_UPLD_DATA) {
				upload = true;
				if (msg->credit_update_flag) {
					rcp_credit_limit_update(hsm, msg->credit_limit_cnt);
				}
			} else if (flag == RCP_FLAG_DEVICE_STOP) {
                RCP_LOG_INF("Channel %d: Received DEVICE_STOP", stream);
                hsm->state = LINK_STATE_HSTA_RESET;
                nh_credit_reset(&hsm->flow_ctrl);
            }
            break;

		default:
			break;
	}

	if (dispatch_msg(msg, hsm, upload)) {
		msg->payload_ptr = NULL;
	}

	if (msg->payload_ptr) {
		buffer_mgmt_free_elem(msg->payload_ptr);
		msg->payload_ptr = NULL;
	}

	return 0;
}

/**
 * @brief Create a stream and start the associated processing.
 * @param stream_id stream ID (RCP_MSG_TYPE_ETH/TTY/WIFI/etc.)
 * @return lnk_sts_t* Pointer to the created stream state, or NULL on failure.
 */
lnk_sts_t *rcp_stream_create(uint8_t stream_id, buffer_mgmt_ctrl_t *tx_buf, uint8_t rx_wnd)
{
	lnk_sts_t *lnk_sts_ptr = NULL;

	if (stream_id >= RCP_MSG_TYPE_MAX || g_rcp_bus >= RCP_BUS_MAX) {
		return NULL;
	}
	lnk_sts_ptr = &g_stream_status[stream_id];

	nh_credit_deinit(&lnk_sts_ptr->flow_ctrl);
	memset(lnk_sts_ptr, 0, sizeof(lnk_sts_t));

	/* Initialize lnk_sts_t */
	lnk_sts_ptr->stream = stream_id;
	lnk_sts_ptr->state = LINK_STATE_IDLE;
	lnk_sts_ptr->error = RCP_LNK_SUCCESS;
	lnk_sts_ptr->bus = g_rcp_bus;
	lnk_sts_ptr->buffer_ctrl = tx_buf;
	lnk_sts_ptr->rx_wnd = rx_wnd;

	nh_credit_init(&lnk_sts_ptr->flow_ctrl, NULL,
				   0,
				   0,
				   lnk_sts_ptr->rx_wnd,
				   RCP_UPLD_CREDIT_UPDATE_THRESHOLD);
    nh_credit_enable(&lnk_sts_ptr->flow_ctrl, NH_CREDIT_EN_TX | NH_CREDIT_EN_RX);

	if (!rcp_os_queue_init(&lnk_sts_ptr->rx_queue, sizeof(rcp_msg_t), RCP_MAX_RX_EVENTS)) {
		RCP_LOG_ERR("Failed to initialize RX queue for stream=%d", lnk_sts_ptr->stream);
		return NULL;
	}

	lnk_sts_ptr->rx_queue_enabled = true;

	int ret = send_rcp_cmd(lnk_sts_ptr, RCP_FLAG_HOST_RESET);
	if (ret != RCP_LNK_SUCCESS) {
		lnk_sts_ptr->state = LINK_STATE_HSTA_ERROR;
		return NULL;
	}
	lnk_sts_ptr->state = LINK_STATE_HSTA_RESET;

	ret = send_rcp_cmd(lnk_sts_ptr, RCP_FLAG_HOST_READY);
	if (ret != RCP_LNK_SUCCESS) {
		lnk_sts_ptr->state = LINK_STATE_HSTA_ERROR;
	} else {
		lnk_sts_ptr->state = LINK_STATE_HSTA_HOST_READY;
	}
	return lnk_sts_ptr;
}


link_result_t init_rcp(enum rcp_bus bus)
{
	int i;

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		nh_credit_deinit(&g_stream_status[i].flow_ctrl);
	}

	memset(g_stream_status, 0, sizeof(g_stream_status));
	srand(rcp_os_time_ms());

	init_tx_queue();
	init_rx_queue();
	init_dispatcher();
	init_cntrl_status();

    if (!rcp_os_thread_create(&g_event_loop_thread, rcp_event_loop_entry,
                 (void *)(uintptr_t)bus, "rcp_evt_loop",
                 STREAM_TASK_STACK_SIZE, STREAM_TASK_PRIORITY)) {
        RCP_LOG_ERR("Failed to create internal event loop");
        return RCP_LNK_ERROR;
    }

	/* Open the bus */
	if (rcp_bus_open(bus) != RCP_BUS_OK) {
		RCP_LOG_ERR("Unable to open bus %d", bus);
		return RCP_LNK_ERROR;
	}
	g_rcp_bus = bus;

	return RCP_LNK_SUCCESS;
}
