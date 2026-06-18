#include "buffer_mgmt_ctrl.h"
#include "rcp_error_codes.h"
#include "rcp_log.h"
#include "rcp_mux_dispatcher.h"
#include "rcp_cntrl_msgs.h"

#include <stddef.h>

#define RCP_DISPATCH_QUEUE_DEPTH      48U
#define RCP_DISPATCH_TASK_STACK_SIZE  2048U
#define RCP_DISPATCH_TASK_PRIORITY    29U

static rcp_rx_event_handler_t data_dispatch_handlers[RCP_MSG_TYPE_MAX];
static rcp_queue_t dispatch_queue;
static rcp_thread_t dispatch_thread;

static bool is_rx_seq_num_valid(lnk_sts_t *lnk_sts_ptr, rcp_msg_ptr_t data_msg)
{
	return true;
}

static bool enqueue_stream_rx_event(const rcp_msg_t *rx_msg, lnk_sts_t *lnk_sts_ptr)
{
	if (rx_msg == NULL || lnk_sts_ptr == NULL) {
		return false;
	}

	if (rcp_os_queue_send(&lnk_sts_ptr->rx_queue, rx_msg, 0) != 0) {
		RCP_LOG_WRN("RX queue full, dropping queued packet for stream=%d flag=%d",
			    rx_msg->msg_type, rx_msg->flag);
		return false;
	}
	return true;
}

static bool enqueue_rx_event(const rcp_msg_t *rx_msg, lnk_sts_t *lnk_sts_ptr)
{
	if (rx_msg == NULL || lnk_sts_ptr == NULL) {
		return false;
	}

	if (rcp_os_queue_send(&dispatch_queue, rx_msg, 0) != 0) {
		RCP_LOG_WRN("Dispatch queue full, dropping packet for stream=%d flag=%d",
			    rx_msg->msg_type, rx_msg->flag);
		return false;
	}

	return true;
}

static void dispatch_thread_entry(void *arg)
{
	(void)arg;
    rcp_msg_t msg;
    rcp_rx_event_handler_t dispatch_fun;
    lnk_sts_t *lnk_sts_ptr;
    enum RCP_DSPTCH_RESULT result = RCP_RX_NO_ACTION;
    bool owned;
    bool upload;

	while (true) {
        owned = false;
        upload = false;

		if (rcp_os_queue_receive(&dispatch_queue, &msg, RCP_OS_WAIT_FOREVER) != 0) {
			continue;
		}

		if (msg.msg_type >= RCP_MSG_TYPE_MAX) {
			RCP_LOG_ERR("unknow msg_type %d\r\n", msg.msg_type);
			if (msg.payload_ptr != NULL) {
				buffer_mgmt_free_elem(msg.payload_ptr);
				msg.payload_ptr = NULL;
			}
			continue;
		}

		dispatch_fun = data_dispatch_handlers[msg.msg_type];
		lnk_sts_ptr = link_get_status(msg.msg_type);
		upload = (msg.flag == RCP_FLAG_UPLD_DATA);

		if (dispatch_fun != NULL) {
			rcp_message_t message = {
				.stream = msg.msg_type,
				.msg_subtype = msg.msg_subtype,
				.msg_flags = msg.flag,
				.msg_buffer = msg.payload_ptr,
			};
			result = dispatch_fun(&message, lnk_sts_ptr);
		}

		if ((result == RCP_RX_UPLOAD) && upload) {
			owned = enqueue_stream_rx_event(&msg, lnk_sts_ptr);
		} else if (result == RCP_RX_TAKEN) {
			owned = true;
		}

		if (!owned && msg.payload_ptr != NULL) {
			buffer_mgmt_free_elem(msg.payload_ptr);
			msg.payload_ptr = NULL;
		}
	}
}

static void dispatch_cntrl_msg(rcp_msg_ptr_t cntrl_msg)
{
	RCP_LOG_DBG("Rx Cntrl Msgs Type %d  Subtype %d", cntrl_msg->msg_type,
		    cntrl_msg->msg_subtype);
	//handle_cntrl_msgs(cntrl_msg);
	if (cntrl_msg->payload_ptr) {
		buffer_mgmt_free_elem(cntrl_msg->payload_ptr);
		cntrl_msg->payload_ptr = NULL;
	}
}

void init_dispatcher()
{
	for (int i = 0; i < RCP_MSG_TYPE_MAX; ++i) {
		data_dispatch_handlers[i] = NULL;
	}

	if (!rcp_os_queue_init(&dispatch_queue, sizeof(rcp_msg_t), RCP_DISPATCH_QUEUE_DEPTH)) {
		RCP_LOG_ERR("Failed to initialize dispatch queue");
		return;
	}

	if (!rcp_os_thread_create(&dispatch_thread, dispatch_thread_entry, NULL, "rcp_dispatch",
				 RCP_DISPATCH_TASK_STACK_SIZE, RCP_DISPATCH_TASK_PRIORITY)) {
		RCP_LOG_ERR("Failed to create dispatch task");
		rcp_os_queue_deinit(&dispatch_queue);
		return;
	}
}

bool register_data_msg_handler(const uint8_t stream, rcp_rx_event_handler_t handler)
{
	if ((stream != RCP_MSG_TYPE_SYS) && stream < RCP_MSG_TYPE_MAX) {
		data_dispatch_handlers[stream] = handler;
		return true;
	}
	RCP_LOG_ERR("Stream Id: %d Not supported", stream);
	return false;
}

bool dispatch_msg(rcp_msg_ptr_t message, lnk_sts_t *link_sts_ptr, bool upload)
{
	(void)upload;

	if (message == NULL || link_sts_ptr == NULL) {
		return false;
	}

	if ((message->msg_type >= RCP_MSG_TYPE_NONE) && (message->msg_type < RCP_MSG_TYPE_MAX)) {
		return enqueue_rx_event(message, link_sts_ptr);
	} else {
		RCP_LOG_ERR("Message Format: %d NOT supported", message->msg_type);
	}

	return false;
}

rcp_rx_event_handler_t get_rx_handler(uint8_t stream)
{
	if ((stream == RCP_MSG_TYPE_SYS) || (stream >= RCP_MSG_TYPE_MAX)) {
		return NULL;
	}
	return data_dispatch_handlers[stream];
}
