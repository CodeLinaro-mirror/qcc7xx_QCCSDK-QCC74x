/**
 * @file rcp_bt_hci.c
 * @brief HCI transport layer over RCP protocol - pure pipe implementation
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "buffer_mgmt_ctrl.h"
#include "rcp_config.h"
#include "rcp_interface.h"
#include "rcp_mux_dispatcher.h"
#include "bt_hci.h"

#define BT_HCI_MAX_MESSAGE_SIZE    256
#define BT_HCI_NUM_BUFFERS          3
#define BT_HCI_BUFFER_SIZE          FRAME_BUFF_TOTALSIZE

/* Debug configuration */
#ifndef BT_HCI_DEBUG_ENABLE
#define BT_HCI_DEBUG_ENABLE   0
#endif

#if BT_HCI_DEBUG_ENABLE
#define BT_HCI_LOG(fmt, ...)    printf("[BT_HCI] " fmt "\n", ##__VA_ARGS__)
#else
#define BT_HCI_LOG(fmt, ...)    do {} while(0)
#endif

/* Global variables */
static bt_evt_cb user_cb;
static bool initialized;
static bool stream_created;
static uint8_t g_bt_hci_buffer_mem[BT_HCI_NUM_BUFFERS][BT_HCI_BUFFER_SIZE];
static buffer_mgmt_ctrl_t *g_bt_hci_buffer_ctrl;
static lnk_sts_t *g_bt_hci_link;

/* Forward declarations */
static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr);

bool bt_hci_init(bt_evt_cb evt_cb)
{
	buffer_mgmt_ctrl_init_cfg_t buffer_cfg;

	if (!evt_cb) {
		return false;
	}

	/* Create buffer controller if not already created */
	if (!stream_created && !g_bt_hci_buffer_ctrl) {
		BT_HCI_LOG("Creating buffer controller...");

		buffer_cfg.frame_buff = g_bt_hci_buffer_mem;
		buffer_cfg.frame_elem_cnt = BT_HCI_NUM_BUFFERS;
		buffer_cfg.frame_elem_size = BT_HCI_BUFFER_SIZE;
		buffer_cfg.name = "bt_hci";

		g_bt_hci_buffer_ctrl = buffer_mgmt_create(&buffer_cfg);
		if (!g_bt_hci_buffer_ctrl) {
			printf("[BT_HCI] Failed to create buffer controller\n");
			return false;
		}

		/* Create RCP stream for BT_HCI */
		BT_HCI_LOG("Creating RCP stream for BT_HCI (type=%d)...", RCP_MSG_TYPE_BT_HCI);
		g_bt_hci_link = rcp_stream_create(RCP_MSG_TYPE_BT_HCI, g_bt_hci_buffer_ctrl,
						       RCP_BT_HCI_RX_WND);
		if (!g_bt_hci_link) {
			printf("[BT_HCI] Failed to create RCP stream\n");
			return false;
		}

		stream_created = true;
		BT_HCI_LOG("Stream created successfully");
	}

	user_cb = evt_cb;
	register_data_msg_handler(RCP_MSG_TYPE_BT_HCI, handle_rcp_msg);
	initialized = true;

	return true;
}

bool bt_hci_send(const uint8_t *data, uint16_t len)
{
	buffer_mgmt_elem_t *msg_buffer_elem = NULL;
	buffer_mgmt_elem_t* msg_buffer;
	link_result_t res;

	if (!initialized) {
		printf("[BT_HCI] bt_hci_send failed: not initialized\n");
		return false;
	}

	if (!data || len == 0 || len > BT_HCI_MAX_MESSAGE_SIZE) {
		printf("[BT_HCI] bt_hci_send failed: invalid parameters\n");
		return false;
	}

	lnk_sts_t *lnk = link_get_status(RCP_MSG_TYPE_BT_HCI);
	if (!lnk || !lnk->buffer_ctrl) {
		printf("[BT_HCI] bt_hci_send failed: link not ready\n");
		return false;
	}

	if (buffer_mgmt_alloc_elem(lnk->buffer_ctrl, &msg_buffer_elem, 0) != 0) {
		printf("[BT_HCI] bt_hci_send failed: allocation failed\n");
		return false;
	}
	msg_buffer = msg_buffer_elem;

	/* Copy HCI data directly */
	uint8_t *buf_ptr = buffer_mgmt_data_ptr(msg_buffer);
	uint16_t max_size = buffer_mgmt_max_size(msg_buffer);

	if (len > max_size) {
		printf("[BT_HCI] bt_hci_send failed: data too large (len=%u, max=%u)\n",
		       len, max_size);
		buffer_mgmt_free_elem(msg_buffer);
		return false;
	}

	memcpy(buf_ptr, data, len);
	msg_buffer->data_size = len;

	BT_HCI_LOG("HCI send: type=0x%02X, len=%u", data[0], len);
	LOG_HEXDUMP_DBG(data, len, "HCI TX");

	/* Send data asynchronously */
	res = link_send(RCP_MSG_TYPE_BT_HCI, msg_buffer, 1000);
	if (res != RCP_LNK_SUCCESS) {
		printf("[BT_HCI] bt_hci_send failed: link_send error=%d\n", res);
		buffer_mgmt_free_elem(msg_buffer);
		return false;
	}

	BT_HCI_LOG("HCI send succeeded");
	return true;
}

static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr)
{
	const uint8_t *rx;

	if (msg_flags != RCP_FLAG_UPLD_DATA || payload_ptr == NULL) {
		return RCP_RX_NO_ACTION;
	}

	if (payload_ptr->data_size == 0) {
		return RCP_RX_NO_ACTION;
	}

	rx = (const uint8_t *)buffer_mgmt_data_ptr_const(payload_ptr);

	BT_HCI_LOG("HCI RX: %u bytes", payload_ptr->data_size);
	LOG_HEXDUMP_DBG(rx, payload_ptr->data_size, "HCI RX");

	/* Directly forward received data to user callback - pure pipe */
	if (user_cb) {
		user_cb(rx, payload_ptr->data_size);
	}

	return RCP_RX_NO_ACTION;
}
