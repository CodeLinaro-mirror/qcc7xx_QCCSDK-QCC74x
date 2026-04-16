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

static uint8_t g_rcp_rx_buffer_mem[RCP_RX_NUM_BUFFERS][FRAME_BUFF_TOTALSIZE];
static lnk_sts_t g_stream_status[RCP_MSG_TYPE_MAX];
static buffer_mgmt_ctrl_t *g_rx_buffer_ctrl;
static enum rcp_bus g_rcp_bus = RCP_BUS_MAX;
static rcp_thread_t g_event_loop_thread;

static int rcp_process_msg(lnk_sts_t *hsm, rcp_msg_t *msg);
static bool bus_flag_is_set(enum rcp_bus bus, uint8_t flag);
static void process_all_pending_tx(enum rcp_bus bus);
static bool process_rx_once(enum rcp_bus bus);
static bool process_pending_tx(enum rcp_bus bus);
static void rcp_event_loop_entry(void *arg);

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

static lnk_sts_t *get_stream_status(uint8_t stream)
{
    if (stream >= RCP_MSG_TYPE_MAX) {
        return NULL;
    }
    return &g_stream_status[stream];
}

lnk_sts_t *link_get_status(uint8_t stream)
{
	return get_stream_status(stream);
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

static int rcp_credit_get(lnk_sts_t *hsm)
{
	if (hsm->state != LINK_STATE_HSTA_DEVICE_RUN) {
		return 0;
	}
	rcp_os_disable_irq();
	if ((int8_t)(hsm->credit_limit_cnt - hsm->credit_consumed_cnt) <= 0) {
		rcp_os_enable_irq();
		return -1;
	}
	hsm->credit_consumed_cnt++;
	if ((int8_t)(hsm->credit_limit_cnt - hsm->credit_consumed_cnt) <= 0) {
		/* TODO:disable transfer ? */
	}
	rcp_os_enable_irq();
	return 0;
}

static int rcp_credit_put(lnk_sts_t *hsm)
{
	uint8_t old;
	if (hsm->state != LINK_STATE_HSTA_DEVICE_RUN) {
		return 0;
	}

	rcp_os_disable_irq();
	old = hsm->credit_consumed_cnt;
	hsm->credit_consumed_cnt -=1;
	if (((int8_t)(old - hsm->credit_consumed_cnt) <= 0) &&
		((int8_t)(hsm->credit_limit_cnt - hsm->credit_consumed_cnt) > 0)) {
		/* TODO:enable transfer ? */
	}
	rcp_os_enable_irq();
	return 0;
}

static int rcp_credit_limit_update(lnk_sts_t *hsm, uint8_t credit_new)
{
	RCP_LOG_DBG("Channel %d: credit limit update now_credit:%d new_credit:%d",
			hsm->stream, hsm->credit_limit_cnt, credit_new);
	rcp_os_disable_irq();
	if ((int8_t)(credit_new - hsm->credit_limit_cnt) > 0) {
		hsm->credit_limit_cnt = credit_new;
	} else {
		RCP_LOG_ERR("credit limit update error:%d %d\r\n", credit_new, hsm->credit_limit_cnt);
	}
	rcp_os_enable_irq();
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

/* Send message over bus with retries in case of failure. */
static bool tx_msg_with_retries(enum rcp_bus bus, buffer_mgmt_elem_t *msg)
{
	bool status = false;
	uint8_t retries = BUS_TX_RETRIES;

	do {
		status = tx_rcp_msg(bus, msg);
		if (!status) {
			RCP_LOG_WRN("Failed to send message over bus. Retrying.");
			rcp_os_delay_ms(BUS_TX_RETRY_DELAY_MS);
		}
	} while(retries-- && !status);

	return status;
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

static void process_all_pending_tx(enum rcp_bus bus)
{
	//rcp_bus_clear_flags(bus, RCP_BUS_FLAG_TX_PENDING);

	while (process_pending_tx(bus)) {
	}
}

static bool process_pending_tx(enum rcp_bus bus)
{
	rcp_schedule_evt_t tx_event = {0};
    lnk_sts_t *lnk;

	if (!peek_tx_event(&tx_event, 0)) {
		return false;
	}

    lnk = get_stream_status(tx_event.stream);
	if (rcp_credit_get(lnk) < 0) {
		RCP_LOG_DBG("RCP TX busy %d %d\r\n", lnk->credit_limit_cnt, lnk->credit_consumed_cnt);
		return false;
	}

	if (!tx_msg_with_retries(bus, tx_event.msg_buffer)) {
		rcp_credit_put(lnk);
		RCP_LOG_WRN("Failed to send message over bus, dropping.");
	}
	get_tx_event(&tx_event, 0);

	if (tx_event.msg_buffer) {
		buffer_mgmt_free_elem(tx_event.msg_buffer);
		tx_event.msg_buffer = NULL;
	}

	return true;
}

static bool process_rx_once(enum rcp_bus bus)
{
	rcp_msg_t rx_msg = {0};

	if (!get_rx_data(&rx_msg, bus)) {
		return false;
	}

	lnk_sts_t *lnk_sts_ptr = get_stream_status(rx_msg.msg_type);

	if (!lnk_sts_ptr) {
		RCP_LOG_WRN("Discarding message on invalid stream=%x", rx_msg.msg_type);
		if (rx_msg.payload_ptr) {
			buffer_mgmt_free_elem(rx_msg.payload_ptr);
			rx_msg.payload_ptr = NULL;
		}
		return true;
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
			hsm->credit_limit_cnt = 0;
			hsm->credit_consumed_cnt = 0;
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
			RCP_LOG_INF("Channel %d: HOST_READY (credits=%d)", stream, hsm->credit_limit_cnt);
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
			hsm->credit_limit_cnt = msg->credit_limit_cnt;
			hsm->credit_consumed_cnt = 0;
			hsm->state = LINK_STATE_HSTA_DEVICE_RUN;
			RCP_LOG_INF("Channel %d: DEVICE_RUN (credits=%d)", stream, hsm->credit_limit_cnt);
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
				hsm->credit_limit_cnt = 0;
				hsm->credit_consumed_cnt = 0;
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
lnk_sts_t *rcp_stream_create(buffer_mgmt_ctrl_t *buffer_ctrl, uint8_t stream_id)
{
	lnk_sts_t *lnk_sts_ptr = NULL;

	if (stream_id >= RCP_MSG_TYPE_MAX || g_rcp_bus >= RCP_BUS_MAX) {
		return NULL;
	}
	lnk_sts_ptr = &g_stream_status[stream_id];

	memset(lnk_sts_ptr, 0, sizeof(lnk_sts_t));

	/* Initialize lnk_sts_t */
	lnk_sts_ptr->stream = stream_id;
	lnk_sts_ptr->state = LINK_STATE_IDLE;
	lnk_sts_ptr->error = RCP_LNK_SUCCESS;
	lnk_sts_ptr->bus = g_rcp_bus;
	lnk_sts_ptr->credit_limit_cnt = 0;
	lnk_sts_ptr->credit_consumed_cnt = 0;
	lnk_sts_ptr->buffer_ctrl = buffer_ctrl;

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
