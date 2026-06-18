#include "rcp_bus.h"
#include "rcp_definitions.h"
#include "rcp_interface_impl.h"
#include "rcp_cntrl_msgs.h"
#include "rcp_protocol.h"
#include "rcp_mux_dispatcher.h"
#include "rcp_stat.h"
#include "rcp_tx_scheduler.h"
#include "rcp_config.h"
#include "rcp_os.h"
#include "rcp_log.h"
#include "wifi/rcp_wifi_mgmt.h"

#include <stddef.h>
#include <limits.h>
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
#define STREAM_TASK_PRIORITY (29)
#define RCP_LINK_DETECT_TIMEOUT_MS 500U
#define RCP_HANDSHAKE_TIMEOUT_MS   1000U
#define RCP_HANDSHAKE_POLL_MS      100U
#ifndef RCP_CMD_TX_NUM_BUFFERS
#define RCP_CMD_TX_NUM_BUFFERS     RCP_MSG_TYPE_MAX
#endif
#define RCP_CMD_TX_FRAME_SIZE      (FRAME_BUFF_DMA_ALIGN * 2U)
#define RCP_CMD_TX_DATA_OFFSET     (FRAME_BUFF_DMA_ALIGN + sizeof(rcp_pro_packt_t))
_Static_assert(sizeof(rcp_pro_packt_t) <= FRAME_BUFF_DMA_ALIGN,
	       "RCP CMD header must fit before the aligned DMA address");

#define RCP_PROTO_META_SIZE ((uint16_t)(sizeof(rcp_pro_packt_t) - sizeof(rcp_msg_packt_t)))
#define RCP_PAYLOAD_SIZE(msg) \
	((uint16_t)(((msg)->msg_packt.len >= RCP_PROTO_META_SIZE) ? ((msg)->msg_packt.len - RCP_PROTO_META_SIZE) : 0U))
#define RCP_FLAG_IS_DATA(flag) (((flag) == RCP_FLAG_DNLD_DATA) || ((flag) == RCP_FLAG_UPLD_DATA))
#define RCP_FLAG_IS_HANDSHAKE(flag) (((flag) >= RCP_FLAG_HOST_RESET) && ((flag) <= RCP_FLAG_DEVICE_STOP))

#define get_stream_status(stream) (((stream) >= RCP_MSG_TYPE_MAX)?NULL:&g_stream_status[(stream)])

static uint8_t g_rcp_rx_buffer_mem[RCP_RX_NUM_BUFFERS][FRAME_BUFF_TOTALSIZE]
	FRAME_BUFF_DMA_MEM_ATTR;
static uint8_t g_rcp_cmd_buffer_mem[RCP_CMD_TX_NUM_BUFFERS][RCP_CMD_TX_FRAME_SIZE]
	FRAME_BUFF_DMA_MEM_ATTR;

static lnk_sts_t g_stream_status[RCP_MSG_TYPE_MAX];
static buffer_mgmt_ctrl_t *g_rx_buffer_ctrl;
static buffer_mgmt_ctrl_t *g_cmd_buffer_ctrl;
static enum rcp_bus g_rcp_bus = RCP_BUS_MAX;
static rcp_thread_t g_event_loop_thread;
static bool g_recover_deferred[RCP_BUS_MAX];

/* Control frames use a dedicated queue so recover handshakes can drain before data. */
static rcp_tx_sch_t g_tx_sch_ctrl;

/* Recover timing state and dump live in rcp_stat.c. */

static int rcp_process_msg(lnk_sts_t *hsm, rcp_msg_t *msg);
static void rcp_rx_buffer_credit_put(buffer_mgmt_elem_t *buffer_elem, void *arg);
static bool bus_flag_is_set(enum rcp_bus bus, uint8_t flag);
static void mark_bus_peer_down(enum rcp_bus bus);
static bool link_active_detect(enum rcp_bus bus);
static void rcp_reset_stream_counters(lnk_sts_t *lnk);
static bool process_rx_once(enum rcp_bus bus);
static bool process_pending_tx_sched(enum rcp_bus bus, rcp_tx_sch_t *sch);
static bool process_pending_tx_stream(enum rcp_bus bus, lnk_sts_t *lnk);
static bool data_tx_pending(enum rcp_bus bus);
static void process_handshake(enum rcp_bus bus);
static bool data_tx_pending(enum rcp_bus bus);
static link_result_t link_request_msg(rcp_message_t *pmessage,
					rcp_rx_event_handler_t response_cb,
					uint32_t timeout_ms);

/* Drain all queued events from a TX scheduler and free their buffers.
 * Used after a successful bus recover to discard stale ctrl/handshake
 * frames that were queued before the link went down. Data frames in the
 * per-stream schedulers are kept — only the caller-chosen scheduler is
 * purged. */
static void purge_tx_queue(rcp_tx_sch_t *sch)
{
	rcp_schedule_evt_t evt;

	while (get_tx_event(sch, &evt, 0)) {
		if (evt.msg_buffer != NULL) {
			buffer_mgmt_free_elem(evt.msg_buffer);
		}
	}
}
static bool process_pending_rx_credit_updates(enum rcp_bus bus);
static void rcp_event_loop_entry(void *arg);

/* TX profile state (g_rcp_tx_prof) and rcp_tx_prof_dump_stats() live in
 * rcp_stat.c so all stats are in one place. */

void rcp_internal_get_dump_ctx(struct rcp_dump_ctx *ctx)
{
	if (ctx == NULL) {
		return;
	}
	ctx->bus             = g_rcp_bus;
	ctx->streams         = g_stream_status;
	ctx->ctrl_sch        = &g_tx_sch_ctrl;
	ctx->cmd_buffer_ctrl = g_cmd_buffer_ctrl;
	ctx->rx_buffer_ctrl  = g_rx_buffer_ctrl;
}

void init_rx_queue()
{
	buffer_mgmt_ctrl_init_cfg_t cfg = {
		.frame_buff = g_rcp_rx_buffer_mem,
		.frame_elem_cnt = RCP_RX_NUM_BUFFERS,
		.frame_elem_size = sizeof(g_rcp_rx_buffer_mem[0]),
		.frame_data_offset = FRAME_BUFF_MSGSTRUCT_OFFSET,
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

	int ret = rcp_bus_read(bus,
			      (rcp_msg_packt_t *)BUFFER_MGMT_DATA_PTR(rx_buffer),
			      (uint16_t)BUFFER_MGMT_MAX_SIZE(rx_buffer));

	if (ret <= 0) {
		if (ret != RCP_BUS_ERR_EMPTY) {
			RCP_LOG_DBG("RX bus failure err=%d", ret);
			mark_bus_peer_down(bus);
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

static void mark_bus_peer_down(enum rcp_bus bus)
{
	int i;

	RCP_LOG_DBG("RCP bus %d peer lost", bus);

	/* Reset recover timing snapshot. T0 is stamped later when the SDIO
	 * card has been re-identified, so device-sleep window is excluded. */
	RCP_STAT_RECOVER_RESET();

	/* Force every active stream back to RESET so any in-flight TX /
	 * credit accounting halts immediately. The event loop will re-arm
	 * the handshake once the bus has recovered. */
	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];
		if (lnk->rx_queue_enabled && lnk->bus == bus) {
			lnk->state = LINK_STATE_HSTA_RESET;
			nh_credit_reset(&lnk->flow_ctrl);
			RCP_STAT_RECOVER_INC(active_stream_count);
		}
	}

	/* Wake the event loop with a RECOVER signal, just like RX/TX/HANDSHAKE.
	 * The event loop's process_recover() branch will run rcp_bus_runtime_recover()
	 * and signal HANDSHAKE on success. */
	__rcp_bus_set_flags(bus, RCP_BUS_FLAG_RECOVER, 0);
}

static int rcp_credit_get(lnk_sts_t *hsm, uint32_t timeout_ms)
{
	int ret;

	if (hsm->state != LINK_STATE_HSTA_DEVICE_RUN) {
		return 0;
	}

	RCP_TX_PROF_BEGIN(credit_wait_start_cycles);
	ret = nh_tx_credit_get(&hsm->flow_ctrl, timeout_ms);
	RCP_TX_PROF_ACC(RCP_TX_PROF_CREDIT_WAIT, credit_wait_start_cycles);
	return ret;
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

	if (!lnk || !g_cmd_buffer_ctrl) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	if (buffer_mgmt_alloc_elem(g_cmd_buffer_ctrl, &tx_buffer, 1000) != 0) {
		RCP_LOG_ERR("Failed to allocate buffer for CMD");
		return -1;
	}

	build_rcp_msg(&send_msg, lnk->stream, flag, tx_buffer);

	if (!send_rcp_msg(&g_tx_sch_ctrl, &send_msg, 1000)) {
		buffer_mgmt_free_elem(tx_buffer);
		return RCP_LNK_ERR_TX_NO_RESOURCES;
	}
	return RCP_LNK_SUCCESS;
}

link_result_t link_request(uint8_t stream, buffer_mgmt_elem_t *tx_buffer,
			   rcp_rx_event_handler_t response_cb, uint32_t timeout_ms)
{
	rcp_message_t message = {
		.stream = stream,
		.msg_subtype = 0,
		.msg_flags = RCP_FLAG_DNLD_DATA,
		.msg_buffer = tx_buffer,
	};

	return link_request_msg(&message, response_cb, timeout_ms);
}

static link_result_t link_request_msg(rcp_message_t *pmessage,
					rcp_rx_event_handler_t response_cb,
					uint32_t timeout_ms)
{
	lnk_sts_t *lnk_stat_ptr;
	rcp_schedule_evt_t tx_event;

	if (!pmessage) {
		return RCP_LINK_ERR_INVALID_BUFFER;
	}

	lnk_stat_ptr = get_stream_status(pmessage->stream);
	if (!lnk_stat_ptr) {
		return RCP_LNK_ERR_INVALID_STREAM;
	}

	if (!pmessage->msg_buffer) {
		return RCP_LINK_ERR_INVALID_BUFFER;
	}

	pmessage->stream = lnk_stat_ptr->stream;
	tx_event.stream = pmessage->stream;
	tx_event.msg_subtype = pmessage->msg_subtype;
	tx_event.msg_flags = pmessage->msg_flags;
	tx_event.msg_buffer = pmessage->msg_buffer;
	tx_event.enqueue_ticks = 0;
	if (!schedule_tx(&lnk_stat_ptr->tx_sch, &tx_event, timeout_ms)) {
		return RCP_LNK_ERR_TX_NO_RESOURCES;
	}
	if (response_cb) {
		register_data_msg_handler(pmessage->stream, response_cb);
	}
	return RCP_LNK_SUCCESS;
}

link_result_t link_send(uint8_t stream, buffer_mgmt_elem_t *tx_buffer, uint32_t timeout_ms)
{
	return link_request(stream, tx_buffer, NULL, timeout_ms);
}

link_result_t link_send_msg(rcp_message_t *pmessage, uint32_t timeout_ms)
{
	return link_request_msg(pmessage, NULL, timeout_ms);
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
	__rcp_bus_set_flags(lnk_stat_ptr->bus, RCP_BUS_FLAG_HANDSHAKE, 0);
	return link_request(stream, tx_buffer, NULL, RCP_OS_WAIT_FOREVER);
}

static bool link_active_detect(enum rcp_bus bus)
{
	struct rcp_bus_caps caps;

	if (bus_flag_is_set(bus, RCP_BUS_FLAG_RECOVER)) {
		return false;
	}

	if (rcp_bus_get_caps(bus, &caps) != RCP_BUS_OK) {
		return true;
	}
	if (!caps.active) {
		mark_bus_peer_down(bus);
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
			RCP_LOG_DBG("Send message over bus %d %p. Retrying.", status, msg);
			rcp_os_delay_ms(BUS_TX_RETRY_DELAY_MS);
		}
	} while(--retries && (status == RCP_BUS_ERR_EMPTY));

	if (status != RCP_BUS_OK && status != RCP_BUS_ERR_EMPTY) {
		mark_bus_peer_down(bus);
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

static void process_recover(enum rcp_bus bus)
{
	if (!rcp_bus_runtime_recover(bus)) {
		/* Back off before re-arming RECOVER, otherwise the event loop
		 * spins on a failing recover at full CPU. */
		rcp_os_delay_ms(100);
		__rcp_bus_set_flags(bus, RCP_BUS_FLAG_RECOVER, 0);
		return;
	}

	RCP_STAT_RECOVER_STAMP(t_reinit_done_us);

	/* Discard stale ctrl/handshake frames queued before the link went
	 * down. Only data frames in the per-stream schedulers are kept. */
	purge_tx_queue(&g_tx_sch_ctrl);

	process_handshake(bus);
}

static void process_handshake(enum rcp_bus bus)
{
	int i;

	RCP_STAT_RECOVER_STAMP_ONCE(t_handshake_kicked_us);

	/* Step 1: advance the handshake state machine for streams sitting
	 * in HSTA_RESET (pushes HOST_RESET / HOST_READY into the ctrl
	 * scheduler). Reuses rcp_process_msg() with a NULL message so
	 * the FSM advances RESET -> HOST_READY in the same code path the
	 * RX side uses for HOST_READY -> DEVICE_START -> DEVICE_RUN. */
	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];
		if (lnk->rx_queue_enabled && lnk->bus == bus
		    && lnk->state == LINK_STATE_HSTA_RESET) {
			rcp_process_msg(lnk, NULL);
		}
	}

	/* Step 2: drain the ctrl scheduler so the device sees those frames
	 * before any data frames the data scheduler may already hold from
	 * before the recover. */
	while (process_pending_tx_sched(bus, &g_tx_sch_ctrl)) {
	}
	RCP_STAT_RECOVER_STAMP_ONCE(t_ctrl_drained_us);
}

static bool data_tx_pending(enum rcp_bus bus)
{
	int i;

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];

		if (lnk->rx_queue_enabled && lnk->bus == bus &&
		    !tx_event_is_empty(&lnk->tx_sch)) {
			return true;
		}
	}

	return false;
}

/* RX: drain every pending RX message. */
static inline void process_rx(enum rcp_bus bus)
{
	while (process_rx_once(bus)) {
	}
}

/* TX: drain the data scheduler, then opportunistic RX-credit updates.
 * Inlined-style direct loop to skip the process_pending_tx wrapper on
 * the hot path. */
static inline void process_tx(enum rcp_bus bus)
{
	uint32_t tx_packets = 0;
	int i;

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];

		if (!lnk->rx_queue_enabled || lnk->bus != bus) {
			continue;
		}

		while (process_pending_tx_stream(bus, lnk)) {
			tx_packets++;
		}
	}
	RCP_TX_PROF_NOTE_WAKEUP(tx_packets);

	/* When data TX is drained, push standalone CREDIT_UPDATE if needed. */
	(void)process_pending_rx_credit_updates(bus);
}

void run_event_loop(enum rcp_bus bus)
{
	while (true) {
		uint8_t ready_flags = 0;
		enum rcp_bus_status wait_status;

		wait_status = rcp_bus_wait_for_any_flags(bus,
				RCP_BUS_FLAG_RECOVER   |
				RCP_BUS_FLAG_HANDSHAKE |
				RCP_BUS_FLAG_RX_DATA   |
				RCP_BUS_FLAG_TX_PENDING,
				RCP_LINK_DETECT_TIMEOUT_MS, &ready_flags);

		if (!g_recover_deferred[bus] && wait_status != RCP_BUS_OK) {
			link_active_detect(bus);
			continue;
		}

		if (ready_flags & RCP_BUS_FLAG_RECOVER) {
			if (!data_tx_pending(bus) && !(ready_flags & RCP_BUS_FLAG_RX_DATA)) {
				g_recover_deferred[bus] = true;
				continue;
			}
			process_recover(bus);
			continue;
		}

		if (g_recover_deferred[bus] &&
		    (data_tx_pending(bus) || (ready_flags & RCP_BUS_FLAG_RX_DATA))) {
			g_recover_deferred[bus] = false;
			process_recover(bus);
			continue;
		}

		if (ready_flags & RCP_BUS_FLAG_RX_DATA) {
			process_rx(bus);
		}

		if (bus_flag_is_set(bus, RCP_BUS_FLAG_TX_PAUSED)) {
			continue;
		}

		if (ready_flags & RCP_BUS_FLAG_HANDSHAKE) {
			process_handshake(bus);
		}

		if ((ready_flags & RCP_BUS_FLAG_TX_PENDING) || data_tx_pending(bus)) {
			process_tx(bus);
		}
	}
}

static void rcp_event_loop_entry(void *arg)
{
	run_event_loop((enum rcp_bus)(uintptr_t)arg);
}

static bool process_pending_tx_sched(enum rcp_bus bus, rcp_tx_sch_t *sch)
{
	rcp_schedule_evt_t tx_event;
	lnk_sts_t *lnk = NULL;
	rcp_pro_packt_t *tx_msg;
	uint8_t flag;
	RCP_TX_PROF_BEGIN(proc_start_cycles);

	if (!peek_tx_event(sch, &tx_event, 0)) {
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return false;
	}

	RCP_TX_PROF_ACC(RCP_TX_PROF_QUEUE_LATENCY, tx_event.enqueue_ticks);

	tx_msg = (rcp_pro_packt_t *)buffer_mgmt_data_ptr(tx_event.msg_buffer);
	flag = tx_msg->flag;

	if (!tx_msg_with_retries(bus, tx_event.msg_buffer)) {
		__rcp_bus_set_flags(bus, sch->signal_flag, 0);
        RCP_LOG_INF("Send message %d stream:%d retry later.", flag, tx_event.stream);
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return false;
	}

	get_tx_event(sch, &tx_event, 0);

	if (tx_msg->credit_update_flag) {
		lnk = get_stream_status(tx_event.stream);
		if (lnk != NULL) {
			nh_rx_credit_update(&lnk->flow_ctrl, tx_msg->credit_limit_cnt);
		}
	}

	if (!RCP_FLAG_IS_HANDSHAKE(flag)) {
		if (lnk == NULL) {
			lnk = get_stream_status(tx_event.stream);
		}
		if (lnk != NULL) {
			uint16_t tx_size = RCP_PAYLOAD_SIZE(tx_msg);

			if (RCP_FLAG_IS_DATA(flag)) {
				lnk->tx_data_packets++;
				lnk->tx_data_bytes += tx_size;
			} else {
				lnk->tx_ctrl_packets++;
				lnk->tx_ctrl_bytes += tx_size;
			}
		}
	}

	buffer_mgmt_free_elem(tx_event.msg_buffer);
	RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);

	return true;
}

static bool process_pending_tx_stream(enum rcp_bus bus, lnk_sts_t *lnk)
{
	rcp_schedule_evt_t tx_event;
	rcp_msg_t send_msg;
	rcp_pro_packt_t *tx_msg;
	int rx_credit;
	uint16_t tx_size;
	RCP_TX_PROF_BEGIN(proc_start_cycles);

	if (lnk->state != LINK_STATE_HSTA_DEVICE_RUN) {
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return false;
	}

	if (!peek_tx_event(&lnk->tx_sch, &tx_event, 0)) {
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return false;
	}

	if (rcp_credit_get(lnk, 0) < 0) {
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return false;
	}

	RCP_TX_PROF_ACC(RCP_TX_PROF_QUEUE_LATENCY, tx_event.enqueue_ticks);

	rx_credit = nh_rx_credit_prepare(&lnk->flow_ctrl);
	send_msg.flag = tx_event.msg_flags ? tx_event.msg_flags : RCP_FLAG_DNLD_DATA;
	send_msg.msg_type = lnk->stream;
	send_msg.msg_subtype = tx_event.msg_subtype;
	send_msg.credit_update_flag = (rx_credit >= 0);
	send_msg.credit_limit_cnt = (rx_credit >= 0) ? (uint8_t)rx_credit : 0;
	send_msg.payload_ptr = tx_event.msg_buffer;

	if (marshal_rcp_msg(&send_msg) == 0) {
		rcp_credit_put(lnk);
		if (get_tx_event(&lnk->tx_sch, &tx_event, 0)) {
			buffer_mgmt_free_elem(tx_event.msg_buffer);
		}
		RCP_LOG_ERR("Failed to marshal data message");
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return true;
	}

	tx_msg = (rcp_pro_packt_t *)buffer_mgmt_data_ptr(tx_event.msg_buffer);
	tx_size = RCP_PAYLOAD_SIZE(tx_msg);

	if (!tx_msg_with_retries(bus, tx_event.msg_buffer)) {
		(void)buffer_mgmt_consume_data(tx_event.msg_buffer, sizeof(rcp_pro_packt_t));
		rcp_credit_put(lnk);
		__rcp_bus_set_flags(bus, RCP_BUS_FLAG_TX_PENDING, 0);
        RCP_LOG_INF("Send message %d stream:%d retry later.", tx_event.msg_flags, tx_event.stream);
		RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);
		return false;
	}

	get_tx_event(&lnk->tx_sch, &tx_event, 0);

	if (rx_credit >= 0) {
		nh_rx_credit_update(&lnk->flow_ctrl, (uint8_t)rx_credit);
	}
	lnk->tx_data_packets++;
	lnk->tx_data_bytes += tx_size;
	buffer_mgmt_free_elem(tx_event.msg_buffer);
	RCP_TX_PROF_ACC(RCP_TX_PROF_PROC_TOTAL, proc_start_cycles);

	return true;
}

static bool process_pending_rx_credit_updates(enum rcp_bus bus)
{
	int i;
	bool sent = false;

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &g_stream_status[i];

		if (lnk->stream != i || lnk->bus != bus ||
		    lnk->state != LINK_STATE_HSTA_DEVICE_RUN) {
			continue;
		}

		if (!nh_rx_credit_need_update(&lnk->flow_ctrl)) {
			continue;
		}

		if (!tx_event_is_empty(&lnk->tx_sch)) {
			continue;
		}

		if (send_rcp_cmd(lnk, RCP_FLAG_CREDIT_UPDATE) != RCP_LNK_SUCCESS) {
			break;
		}
		sent = true;
	}

	return sent;
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

/* rcp_dump_status() lives in rcp_stat.c. */

/* Drive the per-stream handshake state machine.
 *
 * msg may be NULL: the caller is poking the FSM without an incoming
 * message (e.g. the event loop draining RCP_BUS_FLAG_HANDSHAKE after a
 * stream was created or after the bus recovered). In that case only
 * state-driven outbound transitions (HSTA_RESET -> HSTA_HOST_READY) run.
 *
 * msg != NULL: a message just arrived from the device; we may both
 * react to its flag and dispatch its payload to the upper layer. */
static int rcp_process_msg(lnk_sts_t *hsm, rcp_msg_t *msg)
{
	uint8_t stream;
	bool upload = false;
	uint8_t flag;

	if (!hsm) {
		return -1;
	}

	stream = hsm->stream;
	flag = (msg != NULL) ? msg->flag : RCP_FLAG_NONE;

	/* Return to the initial state when DEVICE_RESET is received in any state.
	 * Falling through into the switch below also kicks the RESET->HOST_READY
	 * outbound transition in the same call, so the device sees a fresh
	 * HOST_READY immediately. */
	if (flag == RCP_FLAG_DEVICE_RESET) {
		RCP_LOG_WRN("Channel %d: Received DEVICE_RESET, returning to RESET", stream);
		if (hsm->state != LINK_STATE_HSTA_RESET) {
			hsm->state = LINK_STATE_HSTA_RESET;
			nh_credit_reset(&hsm->flow_ctrl);
		}
		/* Make sure the event loop revisits this stream even if no further
		 * RX comes (e.g. DEVICE_RESET followed by silence). */
		__rcp_bus_set_flags(hsm->bus, RCP_BUS_FLAG_HANDSHAKE, 0);
	}

	/* State machine */
	switch (hsm->state) {
		case LINK_STATE_HSTA_RESET: {
			int ret;

			/* Skip HOST_RESET in the RCP layer: sdio_hal_init_card()
			 * already issued a CCCR Abort Reset (CMD52 to 0x06 with
			 * RES bit), which fires the device-side
			 * sdio2_reset_irq_callback / nh_msg_hw_reset_cb chain.
			 * That chain has already forced every nh_tty / nh_eth /
			 * nh_bt_hci stream back to DSTA_IDLE and reset their
			 * credit, so an extra RCP-level HOST_RESET frame is
			 * redundant and just costs ~450 us per stream.
			 *
			 * Sending HOST_READY directly is enough: device sees it
			 * in DSTA_IDLE and replies DEVICE_START. */
			ret = send_rcp_cmd(hsm, RCP_FLAG_HOST_READY);
			if (ret != RCP_LNK_SUCCESS) {
				RCP_LOG_ERR("Channel %d: HOST_READY send failed, retry next round", stream);
				break;
			}

			hsm->state = LINK_STATE_HSTA_HOST_READY;
			RCP_LOG_DBG("Channel %d: HOST_READY sent, waiting DEVICE_START", stream);
			break;
		}

		case LINK_STATE_HSTA_HOST_READY:
			if (flag != RCP_FLAG_DEVICE_START) {
				break;
			}
            RCP_STAT_RECOVER_STAMP_ONCE(t_first_dev_start_us);
            if (!msg->credit_update_flag || msg->credit_limit_cnt == 0) {
                RCP_LOG_ERR("Channel %d: DEVICE_START without credit", stream);
                hsm->state = LINK_STATE_HSTA_ERROR;
                return -1;
            }
            if (nh_credit_init(&hsm->flow_ctrl, NULL,
                       msg->credit_limit_cnt,
                       hsm->tx_threshold,
                       hsm->rx_wnd,
                       hsm->rx_threshold) != 0) {
                RCP_LOG_ERR("Channel %d: flow ctrl reinit failed", stream);
                hsm->state = LINK_STATE_HSTA_ERROR;
                return -1;
            }
            nh_credit_enable(&hsm->flow_ctrl, NH_CREDIT_EN_TX | NH_CREDIT_EN_RX);

            rcp_reset_stream_counters(hsm);
            hsm->state = LINK_STATE_HSTA_DEVICE_RUN;
            RCP_LOG_INF("Channel %d: DEVICE_RUN (credits=%d)", stream,
                hsm->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt);

            RCP_STAT_RECOVER_STAMP_STREAM_RUN(stream);
            if (!tx_event_is_empty(&hsm->tx_sch)) {
                __rcp_bus_set_flags(hsm->bus, RCP_BUS_FLAG_TX_PENDING, 0);
            }
            break;

		case LINK_STATE_HSTA_DEVICE_RUN:

			if (flag == RCP_FLAG_CREDIT_UPDATE && msg->credit_update_flag) {
				rcp_credit_limit_update(hsm, msg->credit_limit_cnt);
				if (!tx_event_is_empty(&hsm->tx_sch)) {
					__rcp_bus_set_flags(hsm->bus, RCP_BUS_FLAG_TX_PENDING, 0);
				}
			}

			if (flag == RCP_FLAG_UPLD_DATA) {
				upload = true;
				if (msg->credit_update_flag) {
					rcp_credit_limit_update(hsm, msg->credit_limit_cnt);
					if (!tx_event_is_empty(&hsm->tx_sch)) {
						__rcp_bus_set_flags(hsm->bus, RCP_BUS_FLAG_TX_PENDING, 0);
					}
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

	/* No incoming message means this was a self-driven FSM kick;
	 * nothing to dispatch / free. */
	if (msg == NULL) {
		return 0;
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
lnk_sts_t *rcp_stream_create(uint8_t stream_id, const rcp_stream_config_t *conf)
{
	lnk_sts_t *lnk_sts_ptr = NULL;
	uint32_t waited_ms = 0;

	if (stream_id >= RCP_MSG_TYPE_MAX || g_rcp_bus >= RCP_BUS_MAX ||
	    conf == NULL || conf->tx_cnt == 0U || conf->rx_wnd == 0U) {
		return NULL;
	}
	lnk_sts_ptr = &g_stream_status[stream_id];

	nh_credit_deinit(&lnk_sts_ptr->flow_ctrl);
	memset(lnk_sts_ptr, 0, sizeof(lnk_sts_t));

	/* Initialize lnk_sts_t */
	lnk_sts_ptr->stream = stream_id;
	lnk_sts_ptr->error = RCP_LNK_SUCCESS;
	lnk_sts_ptr->bus = g_rcp_bus;
	lnk_sts_ptr->rx_wnd = conf->rx_wnd;
	lnk_sts_ptr->rx_threshold = conf->rx_threshold;
	lnk_sts_ptr->tx_threshold = conf->tx_threshold;
	lnk_sts_ptr->rx_buffer_ctrl = g_rx_buffer_ctrl;
	init_tx_queue(&lnk_sts_ptr->tx_sch, g_rcp_bus,
		      conf->tx_cnt, RCP_BUS_FLAG_TX_PENDING);

	nh_credit_init(&lnk_sts_ptr->flow_ctrl, NULL,
				   0,
				   lnk_sts_ptr->tx_threshold,
				   lnk_sts_ptr->rx_wnd,
				   lnk_sts_ptr->rx_threshold);
    nh_credit_enable(&lnk_sts_ptr->flow_ctrl, NH_CREDIT_EN_TX | NH_CREDIT_EN_RX);

	if (!rcp_os_queue_init(&lnk_sts_ptr->rx_queue, sizeof(rcp_msg_t), RCP_MAX_RX_EVENTS)) {
		RCP_LOG_ERR("Failed to initialize RX queue for stream=%d", lnk_sts_ptr->stream);
		return NULL;
	}

	lnk_sts_ptr->rx_queue_enabled = true;

	/* Mark the stream as needing handshake; the event loop's
	 * RCP_BUS_FLAG_HANDSHAKE handler will drive HOST_RESET -> HOST_READY,
	 * and rcp_process_msg() will advance us through DEVICE_START ->
	 * DEVICE_RUN as device replies arrive. Block here until the stream
	 * reaches DEVICE_RUN (or times out / errors). */
	lnk_sts_ptr->state = LINK_STATE_HSTA_RESET;
	__rcp_bus_set_flags(g_rcp_bus, RCP_BUS_FLAG_HANDSHAKE, 0);

	while (lnk_sts_ptr->state != LINK_STATE_HSTA_DEVICE_RUN) {
		if (waited_ms >= RCP_HANDSHAKE_TIMEOUT_MS) {
			RCP_LOG_WRN("Stream %u: handshake timeout (state=%d)",
				stream_id, lnk_sts_ptr->state);
			lnk_sts_ptr->state = LINK_STATE_HSTA_ERROR;
            break;
		}
		rcp_os_delay_ms(RCP_HANDSHAKE_POLL_MS);
		waited_ms += RCP_HANDSHAKE_POLL_MS;
	}

	return lnk_sts_ptr;
}


link_result_t init_rcp(enum rcp_bus bus)
{
	int i;
	buffer_mgmt_ctrl_init_cfg_t cmd_cfg = {
		.frame_buff = g_rcp_cmd_buffer_mem,
		.frame_elem_cnt = RCP_CMD_TX_NUM_BUFFERS,
		.frame_elem_size = sizeof(g_rcp_cmd_buffer_mem[0]),
		.frame_data_offset = RCP_CMD_TX_DATA_OFFSET,
		.name = "rcp_ctl_tx",
	};

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		nh_credit_deinit(&g_stream_status[i].flow_ctrl);
	}

	memset(g_stream_status, 0, sizeof(g_stream_status));
	srand(rcp_os_time_ms());

	init_tx_queue(&g_tx_sch_ctrl, bus, RCP_CMD_TX_NUM_BUFFERS, RCP_BUS_FLAG_HANDSHAKE);

	init_rx_queue();
	g_cmd_buffer_ctrl = buffer_mgmt_create(&cmd_cfg);
	if (g_cmd_buffer_ctrl == NULL) {
		RCP_LOG_ERR("Failed to initialize CMD TX buffer pool");
		return RCP_LNK_ERROR;
	}
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
