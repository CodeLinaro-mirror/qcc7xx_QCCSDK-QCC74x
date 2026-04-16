/* bt_hci_rcp.c
 *
 * Implementation of bt_hci.h API over RCP protocol.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "rcp_interface.h"
#include "rcp_mux_dispatcher.h"
#include "rcp_os.h"
#include "bt_hci.h"

#define BT_HCI_MAX_MESSAGE_SIZE    256
#define BT_HCI_CMD_RESP_TIMEOUT_MS 5000

#define HCI_H4_CMD 0x01
#define HCI_H4_EVT 0x04

#define HCI_EVT_CMD_COMPLETE 0x0E
#define HCI_EVT_CMD_STATUS   0x0F
#define HCI_EVT_LE_META      0x3E

bt_evt_cb user_cb;

static rcp_sem_t cmd_cplt_sem;
static bool cmd_cplt_sem_ready;

static bool initialized;

static struct {
	bool active;
	uint16_t opcode;
	struct bt_hci_resp resp;
} active_cmd;

static int hci_command_to_buf(uint8_t *buf, uint16_t buf_len, struct bt_hci_cmd *cmd);
static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr);

bool bt_hci_init(bt_evt_cb evt_cb)
{
	if (!evt_cb) {
		return false;
	}
	user_cb = evt_cb;
	register_data_msg_handler(RCP_MSG_TYPE_BT_HCI, handle_rcp_msg);

	if (!cmd_cplt_sem_ready && !rcp_os_sem_init(&cmd_cplt_sem, 0, 1)) {
		printf("Failed to create sem\n");
		return false;
	}
	cmd_cplt_sem_ready = true;
	initialized = true;

	return true;
}

bool bt_hci_cmd_send(struct bt_hci_cmd *cmd, struct bt_hci_resp **resp)
{
	bool ret = true;
	*resp = NULL;
	buffer_mgmt_elem_t *msg_buffer_elem = NULL;
	buffer_mgmt_elem_t* msg_buffer;
	link_result_t res;

	if (!initialized) {
		printf("BT HCI not initialized\n");
	}

	if (active_cmd.active) {
		return false;
	}

	lnk_sts_t *lnk = link_get_status(RCP_MSG_TYPE_BT_HCI);
	if (!lnk || !lnk->buffer_ctrl) {
		return false;
	}

	if (buffer_mgmt_alloc_elem(lnk->buffer_ctrl, &msg_buffer_elem, 0) != 0) {
		return false;
	}
	msg_buffer = msg_buffer_elem;

	int tx_len = hci_command_to_buf(buffer_mgmt_data_ptr(msg_buffer),
				      buffer_mgmt_max_size(msg_buffer), cmd);
	if (tx_len == 0) {
		buffer_mgmt_free_elem(msg_buffer);
		return false;
	}
	msg_buffer->data_size = tx_len;
	LOG_HEXDUMP_DBG(buffer_mgmt_data_ptr_const(msg_buffer), msg_buffer->data_size, "HCI TX");
	/* Set command context */
	active_cmd.active = true;
	/* Set active CMD opcode */
	active_cmd.opcode = cmd->opcode;
	(void)rcp_os_sem_take(&cmd_cplt_sem, 0);

	res = link_send(RCP_MSG_TYPE_BT_HCI, msg_buffer, 1000);
	if (res != RCP_LNK_SUCCESS) {
		buffer_mgmt_free_elem(msg_buffer);
		ret = false;
		goto end;
	}

	/* Wait for command complete */
	if (rcp_os_sem_take(&cmd_cplt_sem, BT_HCI_CMD_RESP_TIMEOUT_MS)) {
		/* Return active command response */
		*resp = &active_cmd.resp;
	} else { /* Timeout on response. */
		ret = false;
		goto end;
	}

end:
	/* Invalidate current command */
	active_cmd.active = false;
	return ret;
}

static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr)
{
	uint8_t tot_len = 0;
	uint8_t *rx;

	if (msg_flags != RCP_FLAG_UPLD_DATA || payload_ptr == NULL) {
		return RCP_RX_NO_ACTION;
	}

	rx = (uint8_t *)buffer_mgmt_data_ptr_const(payload_ptr);
	LOG_HEXDUMP_DBG(buffer_mgmt_data_ptr_const(payload_ptr), payload_ptr->data_size, "HCI RX");

	switch (rx[0]) {
	case HCI_H4_EVT:
		tot_len = rx[2];
		switch (rx[1]) {
		case HCI_EVT_CMD_COMPLETE: {
			uint8_t payload_len;

			/* TODO: Check if active command. */
			/* Length indicated - num HCI packets - opcode */
			payload_len = tot_len - 3;
			memset(&active_cmd.resp, 0, sizeof(active_cmd.resp));
			active_cmd.resp.response_len = payload_len;
			memcpy(active_cmd.resp.response, &rx[6], payload_len);
			(void)rcp_os_sem_give(&cmd_cplt_sem);
			break;
		}
		case HCI_EVT_CMD_STATUS:
			active_cmd.resp.status = rx[3];
			(void)rcp_os_sem_give(&cmd_cplt_sem);
			break;
		default:
			/* Call user callback for all other events. */
			user_cb(rx[1], &rx[3], tot_len);
		}
		break;
	default:
		printf("Unknown HCI H:4 event\n");
	}

	return RCP_RX_NO_ACTION;
}

static int hci_command_to_buf(uint8_t *buf, uint16_t buf_len, struct bt_hci_cmd *cmd)
{
	if (buf_len < cmd->payload_len + 4) {
		return 0;
	}

	buf[0] = HCI_H4_CMD;
	buf[1] = cmd->opcode & 0xFF;
	buf[2] = (cmd->opcode >> 8) & 0xFF;
	buf[3] = cmd->payload_len;
	if (cmd->payload_len) {
		if (!cmd->payload) {
			return 0;
		}
		memcpy(&buf[4], cmd->payload, cmd->payload_len);
	}
	return cmd->payload_len + 4;
}
