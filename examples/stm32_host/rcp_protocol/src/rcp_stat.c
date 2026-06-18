#include "rcp_stat.h"
#include "rcp_interface.h"
#include "rcp_interface_impl.h"
#include "rcp_bus_sdio_master.h"
#include "rcp_protocol.h"
#include "rcp_tx_scheduler.h"
#include "rcp_os.h"
#include "buffer_mgmt_ctrl.h"
#include "nh_credit_flow.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#if RCP_STAT_ENABLE

rcp_stat_recover_timing_t g_rcp_stat_recover;

/* Hardware-counter-backed microsecond clock supplied by the platform
 * port (rcp_perf.h). Counter typically wraps every (2^32 / cycle_hz)
 * seconds (~7.16 s at 600 MHz); rcp_stat only stores deltas relative
 * to t_card_detected_us so wrap is fine as long as a single recover
 * finishes within that window. */
#include "rcp_perf.h"

uint32_t rcp_stat_now_us(void)
{
	return rcp_perf_now_us();
}

static const char *stream_name(uint8_t stream)
{
	switch (stream) {
	case RCP_MSG_TYPE_NONE:    return "NONE";
	case RCP_MSG_TYPE_TEST_1:  return "TEST_1";
	case RCP_MSG_TYPE_TEST_2:  return "TEST_2";
	case RCP_MSG_TYPE_TEST_3:  return "TEST_3";
	case RCP_MSG_TYPE_SYS:     return "SYS";
	case RCP_MSG_TYPE_WIFI_STA:    return "WIFI";
	case RCP_MSG_TYPE_ETH:     return "ETH";
	case RCP_MSG_TYPE_TTY:     return "TTY";
	case RCP_MSG_TYPE_NETLINK: return "NETLINK";
	case RCP_MSG_TYPE_BT_HCI:  return "BT_HCI";
	default:                   return "?";
	}
}

static const char *link_state_name(link_state_t state)
{
	switch (state) {
	case LINK_STATE_IDLE:              return "IDLE";
	case LINK_STATE_SYNCING:           return "SYNCING";
	case LINK_STATE_SYNC_PENDING:      return "SYNC_PENDING";
	case LINK_STATE_ACTIVE:            return "ACTIVE";
	case LINK_STATE_FAULT:             return "FAULT";
	case LINK_STATE_HSTA_RESET:        return "HSTA_RESET";
	case LINK_STATE_HSTA_HOST_READY:   return "HSTA_HOST_READY";
	case LINK_STATE_HSTA_DEVICE_RUN:   return "HSTA_DEVICE_RUN";
	case LINK_STATE_HSTA_ERROR:        return "HSTA_ERROR";
	default:                           return "UNKNOWN";
	}
}

static const char *flag_name(uint8_t flag)
{
	switch (flag) {
	case RCP_FLAG_NONE:          return "NONE";
	case RCP_FLAG_DNLD_DATA:     return "DNLD_DATA";
	case RCP_FLAG_UPLD_DATA:     return "UPLD_DATA";
	case RCP_FLAG_HOST_RESET:    return "HOST_RESET";
	case RCP_FLAG_HOST_READY:    return "HOST_READY";
	case RCP_FLAG_HOST_STOP:     return "HOST_STOP";
	case RCP_FLAG_DEVICE_RESET:  return "DEVICE_RESET";
	case RCP_FLAG_DEVICE_START:  return "DEVICE_START";
	case RCP_FLAG_DEVICE_STOP:   return "DEVICE_STOP";
	case RCP_FLAG_CREDIT_UPDATE: return "CREDIT_UPDATE";
	default:                     return "?";
	}
}

static unsigned long queue_free_slots(const rcp_queue_t *queue)
{
	return (unsigned long)rcp_os_queue_spaces_available((rcp_queue_t *)queue);
}

static void dump_credit_state(const char *name, const nh_credit_t *credit,
			      bool enabled, bool is_tx)
{
	uint8_t delta;

	if (credit == NULL) {
		printf("    %-9s: NULL\n", name);
		return;
	}

	delta = is_tx ?
		(uint8_t)(credit->credit_limit_cnt - credit->credit_consumed_cnt) :
		(uint8_t)(credit->credit_consumed_cnt - credit->credit_limit_cnt);

	printf("    %-9s: %s limit=%u used=%u %s=%u sem=%lu th=%u\n",
	       name,
	       enabled ? "on " : "off",
	       credit->credit_limit_cnt,
	       credit->credit_consumed_cnt,
	       is_tx ? "left" : "pending",
	       delta,
	       rcp_os_sem_count(credit->wait_sem),
	       credit->threshold);
}

void rcp_dump_status(void)
{
	struct rcp_dump_ctx ctx;
	uint8_t bus_flags = 0;
	struct rcp_bus_caps caps = {0};
	rcp_schedule_evt_t head_evt = {0};
	rcp_schedule_evt_t ctrl_head_evt = {0};
	bool head_valid = false;
	bool ctrl_head_valid;
	unsigned long rx_pool_free = 0;
	unsigned long rx_pool_inflight = 0;
	unsigned long rx_pool_total = 0;
	int tx_pending = 0;
	int tx_total = 0;
	int ctrl_pending;
	int ctrl_total;

	rcp_internal_get_dump_ctx(&ctx);

	for (int i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &ctx.streams[i];

		if (!lnk->rx_queue_enabled) {
			continue;
		}

		tx_pending += tx_event_pending_count(&lnk->tx_sch);
		tx_total += tx_event_capacity(&lnk->tx_sch);
	}

	if (ctx.bus < RCP_BUS_MAX) {
		(void)rcp_bus_get_flags(ctx.bus, &bus_flags);
		(void)rcp_bus_get_caps(ctx.bus, &caps);
	}

	printf("=== Host RCP Status ===\n");
	printf("[bus]\n");
	printf("  id=%d flags=0x%02X rx=%u tx=%u handshake=%u recover=%u rx_len=%u mtu=%u\n",
	       ctx.bus,
	       bus_flags,
	       (bus_flags & RCP_BUS_FLAG_RX_DATA) ? 1U : 0U,
	       (bus_flags & RCP_BUS_FLAG_TX_PENDING) ? 1U : 0U,
	       (bus_flags & RCP_BUS_FLAG_HANDSHAKE) ? 1U : 0U,
	       (bus_flags & RCP_BUS_FLAG_RECOVER) ? 1U : 0U,
	       (unsigned int)((ctx.bus < RCP_BUS_MAX) ? rcp_bus_get_rx_len(ctx.bus) : 0U),
	       (unsigned int)caps.mtu);

	printf("[tx]\n");
	printf("  data_queue: used=%d free=%d total=%d\n",
	       tx_pending,
	       (tx_total >= tx_pending) ? (tx_total - tx_pending) : 0,
	       tx_total);
	for (int i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &ctx.streams[i];

		if (!lnk->rx_queue_enabled) {
			continue;
		}

		head_valid = peek_tx_event(&lnk->tx_sch, &head_evt, 0);
		if (head_valid) {
			break;
		}
	}
	if (head_valid && head_evt.msg_buffer != NULL) {
		printf("    head: stream=%u(%s) raw_len=%u\n",
		       head_evt.stream,
		       stream_name(head_evt.stream),
		       head_evt.msg_buffer->data_size);
	} else {
		printf("    head: none\n");
	}

	ctrl_pending = tx_event_pending_count(ctx.ctrl_sch);
	ctrl_total = tx_event_capacity(ctx.ctrl_sch);
	printf("  ctrl_queue: used=%d free=%d total=%d\n",
	       ctrl_pending,
	       (ctrl_total >= ctrl_pending) ? (ctrl_total - ctrl_pending) : 0,
	       ctrl_total);

	ctrl_head_valid = peek_tx_event(ctx.ctrl_sch, &ctrl_head_evt, 0);
	if (ctrl_head_valid && ctrl_head_evt.msg_buffer != NULL) {
		rcp_pro_packt_t *ctrl_msg =
			(rcp_pro_packt_t *)buffer_mgmt_data_ptr(ctrl_head_evt.msg_buffer);

		printf("    head: stream=%u(%s) flag=%u(%s)\n",
		       ctrl_head_evt.stream,
		       stream_name(ctrl_head_evt.stream),
		       ctrl_msg->flag,
		       flag_name(ctrl_msg->flag));
	} else {
		printf("    head: none\n");
	}

	printf("[pools]\n");
	if (ctx.cmd_buffer_ctrl != NULL) {
		unsigned long cmd_pool_free =
			(unsigned long)buffer_mgmt_pool_get_available(ctx.cmd_buffer_ctrl);
		unsigned long cmd_pool_total =
			(unsigned long)ctx.cmd_buffer_ctrl->frame_elem_cnt;
		unsigned long cmd_pool_inflight =
			(cmd_pool_total >= cmd_pool_free) ? (cmd_pool_total - cmd_pool_free) : 0;

		printf("  cmd_tx(%s): free=%lu/%lu inflight=%lu\n",
		       ctx.cmd_buffer_ctrl->name ? ctx.cmd_buffer_ctrl->name : "?",
		       cmd_pool_free,
		       cmd_pool_total,
		       cmd_pool_inflight);
	} else {
		printf("  cmd_tx: NULL\n");
	}

	if (ctx.rx_buffer_ctrl != NULL) {
		rx_pool_free = (unsigned long)buffer_mgmt_pool_get_available(ctx.rx_buffer_ctrl);
		rx_pool_inflight = queue_free_slots(&ctx.rx_buffer_ctrl->pool_queue);
		rx_pool_total = rx_pool_free + rx_pool_inflight;
		printf("  rx(%s): free=%lu/%lu inflight=%lu\n",
		       ctx.rx_buffer_ctrl->name ? ctx.rx_buffer_ctrl->name : "rcp_rx",
		       rx_pool_free,
		       rx_pool_total,
		       rx_pool_inflight);
	} else {
		printf("  rx: NULL\n");
	}

	printf("[streams]\n");
	for (int i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		lnk_sts_t *lnk = &ctx.streams[i];
		unsigned long tx_queue_used = 0;
		unsigned long tx_queue_free = 0;
		unsigned long rx_queue_used = 0;
		unsigned long rx_queue_free = 0;

		if (!lnk->rx_queue_enabled && lnk->state == LINK_STATE_IDLE && lnk->stream == 0U) {
			continue;
		}

		printf("  %d %-7s state=%s(%d) err=%d bus=%d rx_wnd=%u\n",
		       i,
		       stream_name(i),
		       link_state_name(lnk->state),
		       lnk->state,
		       lnk->error,
		       lnk->bus,
		       lnk->rx_wnd);

		if (lnk->rx_queue_enabled) {
			tx_queue_used = (unsigned long)rcp_os_queue_messages_waiting(&lnk->tx_sch.queue);
			tx_queue_free = queue_free_slots(&lnk->tx_sch.queue);
			rx_queue_used = (unsigned long)rcp_os_queue_messages_waiting(&lnk->rx_queue);
			rx_queue_free = queue_free_slots(&lnk->rx_queue);
		}

		printf("    queue    : tx=%lu/%u free=%lu, rx=%s",
		       tx_queue_used,
		       lnk->tx_sch.depth,
		       tx_queue_free,
		       lnk->rx_queue_enabled ? "" : "disabled\n");
		if (lnk->rx_queue_enabled) {
			printf("%lu/%u free=%lu\n",
			       rx_queue_used,
			       RCP_MAX_RX_EVENTS,
			       rx_queue_free);
		}

		printf("    data     : pkt tx/rx=%lu/%lu bytes tx/rx=%lu/%lu\n",
		       (unsigned long)lnk->tx_data_packets,
		       (unsigned long)lnk->rx_data_packets,
		       (unsigned long)lnk->tx_data_bytes,
		       (unsigned long)lnk->rx_data_bytes);
		printf("    ctrl     : pkt tx/rx=%lu/%lu bytes tx/rx=%lu/%lu\n",
		       (unsigned long)lnk->tx_ctrl_packets,
		       (unsigned long)lnk->rx_ctrl_packets,
		       (unsigned long)lnk->tx_ctrl_bytes,
		       (unsigned long)lnk->rx_ctrl_bytes);
		printf("    credit   : tag=%s mask=0x%02X\n",
		       lnk->flow_ctrl.tag ? lnk->flow_ctrl.tag : "?",
		       lnk->flow_ctrl.enable);
		dump_credit_state("tx", &lnk->flow_ctrl.credit[NH_CREDIT_DIR_TX],
				  (lnk->flow_ctrl.enable & NH_CREDIT_EN_TX) != 0U, true);
		dump_credit_state("rx", &lnk->flow_ctrl.credit[NH_CREDIT_DIR_RX],
				  (lnk->flow_ctrl.enable & NH_CREDIT_EN_RX) != 0U, false);
	}

	printf("=== End ===\n");
}

void rcp_stat_recover_dump(void)
{
	const rcp_stat_recover_timing_t *t = &g_rcp_stat_recover;
	int i;

	printf("=== RCP Recover Timing (T0 = card back on bus, units: us) ===\n");
	if (t->t_card_detected_us == 0U) {
		printf("  (no recover happened yet)\n");
		printf("=== End ===\n");
		return;
	}

	printf("  active_streams        : %u\n", (unsigned)t->active_stream_count);
	printf("  run_streams           : %u\n", (unsigned)t->run_stream_count);

	if (t->t_card_identified_us != 0U) {
		/* Sub-breakdown of "reinit + identify": HAL_SDIO_Init covers
		 * MspInit + 74-cycle delay + the IdentifyCard callback, while
		 * sdio_hw_init runs after that with bus-width / freq / func /
		 * block-size / scratch reads. */
		uint32_t hal_init  = t->t_hal_init_done_us
		                     ? (t->t_hal_init_done_us - t->t_card_detected_us) : 0U;
		uint32_t identify  = t->t_hal_init_done_us
		                     ? (t->t_card_identified_us - t->t_hal_init_done_us) : 0U;
		uint32_t hw_init   = t->t_reinit_done_us
		                     ? (t->t_reinit_done_us - t->t_card_identified_us) : 0U;

		(void)identify;
		printf("  HAL_SDIO_Init total   : %lu us  (incl. 74-cycle wait)\n",
		       (unsigned long)hal_init);
		printf("  sdio_hw_init          : %lu us  (bus_width/freq/func/etc)\n",
		       (unsigned long)hw_init);
	}

	if (t->t_reinit_done_us != 0U) {
		printf("  reinit + identify     : %lu us\n",
		       (unsigned long)(t->t_reinit_done_us - t->t_card_detected_us));
	} else {
		printf("  reinit + identify     : <pending>\n");
	}

	if (t->t_handshake_kicked_us != 0U && t->t_reinit_done_us != 0U) {
		printf("  evt loop wakeup       : %lu us\n",
		       (unsigned long)(t->t_handshake_kicked_us - t->t_reinit_done_us));
	}

	if (t->t_ctrl_drained_us != 0U && t->t_handshake_kicked_us != 0U) {
		printf("  ctrl tx drain (host)  : %lu us\n",
		       (unsigned long)(t->t_ctrl_drained_us - t->t_handshake_kicked_us));
	}

	if (t->t_first_dev_ready_us != 0U && t->t_ctrl_drained_us != 0U) {
		printf("  -> 1st DEVICE_READY   : %lu us (after ctrl drained)\n",
		       (unsigned long)(t->t_first_dev_ready_us - t->t_ctrl_drained_us));
	}

	if (t->t_first_dev_start_us != 0U && t->t_first_dev_ready_us != 0U) {
		printf("  -> 1st DEVICE_START   : %lu us (after DEVICE_READY)\n",
		       (unsigned long)(t->t_first_dev_start_us - t->t_first_dev_ready_us));
	}

	for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
		if (t->t_stream_run_us[i] != 0U && t->t_handshake_kicked_us != 0U) {
			printf("  stream %d (%-7s) hs : %lu us\n",
			       i, stream_name((uint8_t)i),
			       (unsigned long)(t->t_stream_run_us[i] - t->t_handshake_kicked_us));
		}
	}

	if (t->t_all_run_us != 0U) {
		printf("  TOTAL recover (T3-T0) : %lu us  (all streams RUN)\n",
		       (unsigned long)(t->t_all_run_us - t->t_card_detected_us));
	} else {
		/* Not every active stream finished handshake. Show the time
		 * up to the last stream that did reach RUN (so the user still
		 * sees a useful partial total). */
		uint32_t last_run_us = 0U;
		for (i = 0; i < RCP_MSG_TYPE_MAX; i++) {
			if (t->t_stream_run_us[i] > last_run_us) {
				last_run_us = t->t_stream_run_us[i];
			}
		}
		if (last_run_us != 0U) {
			printf("  PARTIAL recover       : %lu us  (only %u/%u streams RUN)\n",
			       (unsigned long)(last_run_us - t->t_card_detected_us),
			       (unsigned)t->run_stream_count,
			       (unsigned)t->active_stream_count);
		} else {
			printf("  TOTAL recover         : <no stream reached RUN>\n");
		}
	}

	printf("=== End ===\n");
}

#else  /* RCP_STAT_ENABLE == 0 */

void rcp_dump_status(void) { }

#endif /* RCP_STAT_ENABLE */

/*****************************************************************************
 * TX profile (DWT cycle counters per stage).
 * Storage + dump live here so all stats have one module owner.
 * RCP_TX_PROFILE_ENABLE is the compile switch, independent of RCP_STAT_ENABLE.
 *****************************************************************************/
#if defined(RCP_TX_PROFILE_ENABLE) && (RCP_TX_PROFILE_ENABLE != 0)
rcp_tx_prof_stats_t g_rcp_tx_prof;

static const char *const g_tx_prof_stage_name[RCP_TX_PROF_MAX] = {
	[RCP_TX_PROF_PROD_ALLOC]         = "prod_alloc",
	[RCP_TX_PROF_PROD_LINK_SEND]     = "prod_link_send",
	[RCP_TX_PROF_NETIF_ALLOC]        = "netif_alloc",
	[RCP_TX_PROF_NETIF_COPY]         = "netif_copy",
	[RCP_TX_PROF_NETIF_LINK_SEND]    = "netif_link_send",
	[RCP_TX_PROF_CREDIT_WAIT]        = "credit_wait",
	[RCP_TX_PROF_QUEUE_LATENCY]      = "queue_latency",
	[RCP_TX_PROF_PROC_TOTAL]         = "proc_total",
	[RCP_TX_PROF_SDIO_DMA_WAIT]      = "sdio_dma_wait",
	[RCP_TX_PROF_SDIO_WAIT_TO_IRQ]   = "sdio_wait_to_irq",
	[RCP_TX_PROF_SDIO_IRQ_TO_RESUME] = "sdio_irq_to_resume",
};

static unsigned long prof_u64_to_ul(uint64_t value)
{
	return (value > (uint64_t)ULONG_MAX) ? ULONG_MAX : (unsigned long)value;
}

void rcp_tx_prof_dump_stats(void)
{
	unsigned int i;

	printf("RCP TX profile (cycle_hz=%lu)\n", (unsigned long)rcp_perf_cycle_hz());
	printf("  %-18s %10s %12s %12s %12s\n",
	       "stage", "count", "avg_us", "max_us", "total_ms");
	for (i = 0; i < RCP_TX_PROF_MAX; i++) {
		const uint64_t total_cycles = g_rcp_tx_prof.stage[i].total_cycles;
		const uint32_t count        = g_rcp_tx_prof.stage[i].count;
		unsigned long avg_us;
		unsigned long max_us;
		unsigned long total_ms;

		if (count == 0U) {
			continue;
		}

		avg_us  = prof_u64_to_ul(rcp_perf_cycles_to_us(total_cycles) / count);
		max_us  = prof_u64_to_ul(rcp_perf_cycles_to_us(g_rcp_tx_prof.stage[i].max_cycles));
		total_ms = prof_u64_to_ul(rcp_perf_cycles_to_ms(total_cycles));
		printf("  %-18s %10lu %12lu %12lu %12lu\n",
		       g_tx_prof_stage_name[i],
		       (unsigned long)count, avg_us, max_us, total_ms);
	}
	if (g_rcp_tx_prof.wakeups != 0U) {
		printf("  %-18s %10lu %12lu %12lu %12s\n",
		       "tx_wakeup",
		       (unsigned long)g_rcp_tx_prof.wakeups,
		       (unsigned long)(g_rcp_tx_prof.packets_per_wakeup_sum / g_rcp_tx_prof.wakeups),
		       (unsigned long)g_rcp_tx_prof.max_packets_per_wakeup,
		       "-");
	}
}
#endif /* RCP_TX_PROFILE_ENABLE */

void rcp_stat_dump_all(void)
{
	RCP_STAT_DUMP_ALL_BODY();
}
