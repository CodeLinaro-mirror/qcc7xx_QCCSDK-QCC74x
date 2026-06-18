#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "rcp_tty.h"

#include "buffer_mgmt_ctrl.h"
#include "rcp_config.h"
#include "rcp_interface.h"
#include "rcp_log.h"

enum rcp_tty_flag {
	RCP_TTY_FLAG_OPEN = RCP_FLAG_MAX,
	RCP_TTY_FLAG_CLOSE,
};

#define RCP_TTY_CTRL_STREAM       RCP_MSG_TYPE_TTY
#define RCP_TTY_CTRL_NUM_BUFFERS  RCP_TTY_TX_NUM
#define RCP_TTY_CTRL_PAYLOAD_SIZE 1024U
#define RCP_TTY_CTRL_BUFFER_SIZE  FRAME_BUFF_TOTALSIZE

typedef struct {
	buffer_mgmt_ctrl_t *buffer_ctrl;
	lnk_sts_t *lnk_sts;
} rcp_tty_hd_t;

static uint8_t g_rcp_tty_buffer_mem[RCP_TTY_CTRL_NUM_BUFFERS][RCP_TTY_CTRL_BUFFER_SIZE]
	FRAME_BUFF_DMA_MEM_ATTR;
static rcp_tty_hd_t g_rcp_tty;

static int rcp_tty_status_from_link_result(link_result_t result)
{
	if (result == RCP_LNK_SUCCESS) {
		return RCP_BUS_OK;
	}

	if (result == RCP_LNK_TIMEOUT) {
		return RCP_BUS_ERR_TIMEOUT;
	}

	if (result == RCP_LNK_ERR_TX_NO_RESOURCES || result == RCP_LNK_TX_BUSY) {
		return RCP_BUS_ERR_MEMORY;
	}

	return RCP_BUS_ERR_IO;
}

int rcp_tty_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
	buffer_mgmt_elem_t *msg_buffer = NULL;
	link_result_t send_result;

	if (data == NULL || len == 0U) {
		return RCP_BUS_ERR_INVALID;
	}

	if (g_rcp_tty.buffer_ctrl == NULL || g_rcp_tty.lnk_sts == NULL) {
		return RCP_BUS_ERR_NO_INST;
	}

	if (len > RCP_TTY_CTRL_PAYLOAD_SIZE) {
		return RCP_BUS_ERR_INVALID;
	}

	if (buffer_mgmt_alloc_elem(g_rcp_tty.buffer_ctrl, &msg_buffer, timeout_ms) != 0 ||
	    msg_buffer == NULL) {
		return RCP_BUS_ERR_MEMORY;
	}

	if (!buffer_mgmt_write(msg_buffer, data, len)) {
		buffer_mgmt_free_elem(msg_buffer);
		return RCP_BUS_ERR_INVALID;
	}

	send_result = link_send(RCP_TTY_CTRL_STREAM, msg_buffer, timeout_ms);
	if (send_result == RCP_LNK_SUCCESS) {
		return RCP_BUS_OK;
	}

	buffer_mgmt_free_elem(msg_buffer);
	return rcp_tty_status_from_link_result(send_result);
}

int rcp_tty_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
	buffer_mgmt_elem_t *rx_buffer = NULL;
	const uint8_t *data;
	uint16_t data_len;
	size_t copy_len;
	link_result_t ret;

	if (buf == NULL || len == 0U) {
		return -EINVAL;
	}

	ret = link_receive(RCP_TTY_CTRL_STREAM, &rx_buffer, timeout_ms);
	if (ret != RCP_LNK_SUCCESS || rx_buffer == NULL) {
		return (ret == RCP_LNK_TIMEOUT) ? -ETIMEDOUT : -EIO;
	}

	data = buffer_mgmt_data_ptr_const(rx_buffer);
	data_len = buffer_mgmt_data_size(rx_buffer);
	if (data == NULL || data_len == 0U) {
		buffer_mgmt_free_elem(rx_buffer);
		return -EIO;
	}

	copy_len = (data_len < len) ? data_len : len;
	memcpy(buf, data, copy_len);

	buffer_mgmt_free_elem(rx_buffer);
	return copy_len;
}

static uint8_t handle_rcp_msg(rcp_message_t *pmessage, lnk_sts_t *lnk)
{
	const uint8_t *payload;
	uint16_t payload_len;
	const buffer_mgmt_elem_t *payload_ptr = pmessage ? pmessage->msg_buffer : NULL;

	(void)lnk;

	if (pmessage != NULL && pmessage->msg_flags == RCP_FLAG_DEVICE_START) {
		send_rcp_cmd(g_rcp_tty.lnk_sts, RCP_TTY_FLAG_OPEN);
	}

	if (pmessage == NULL || pmessage->msg_flags != RCP_FLAG_UPLD_DATA || payload_ptr == NULL) {
		return RCP_RX_NO_ACTION;
	}

	payload = buffer_mgmt_data_ptr_const(payload_ptr);
	payload_len = buffer_mgmt_data_size(payload_ptr);
	if (payload == NULL || payload_len == 0U) {
		return RCP_RX_NO_ACTION;
	}

	return RCP_RX_UPLOAD;
}

int rcp_tty_init(void)
{
	buffer_mgmt_ctrl_init_cfg_t cfg = {
		.frame_buff = g_rcp_tty_buffer_mem,
		.frame_elem_cnt = RCP_TTY_CTRL_NUM_BUFFERS,
		.frame_elem_size = sizeof(g_rcp_tty_buffer_mem[0]),
		.frame_data_offset = FRAME_BUFF_PAYLOAD_OFFSET,
		.name = "rcp_tty",
	};
	const rcp_stream_config_t stream_cfg = {
		.tx_cnt = RCP_TTY_CTRL_NUM_BUFFERS,
		.rx_wnd = RCP_TTY_RX_WND,
		.rx_threshold = RCP_UPLD_CREDIT_UPDATE_THRESHOLD,
		.tx_threshold = RCP_UPLD_CREDIT_UPDATE_THRESHOLD,
	};

	if (g_rcp_tty.lnk_sts != NULL) {
		return RCP_BUS_OK;
	}

	if (link_register_rx_handler(RCP_TTY_CTRL_STREAM, handle_rcp_msg) != RCP_LNK_SUCCESS) {
		RCP_LOG_ERR("Failed to register TTY RX handler");
		return RCP_BUS_ERR_IO;
	}

	g_rcp_tty.buffer_ctrl = buffer_mgmt_create(&cfg);
	if (g_rcp_tty.buffer_ctrl == NULL) {
		return RCP_BUS_ERR_MEMORY;
	}

	g_rcp_tty.lnk_sts = rcp_stream_create(RCP_TTY_CTRL_STREAM, &stream_cfg);
	if (g_rcp_tty.lnk_sts == NULL) {
		return RCP_BUS_ERR_IO;
	}

	return RCP_BUS_OK;
}
