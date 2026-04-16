#include <stdio.h>
#include <string.h>

#include "wifi/rcp_wifi_mgmt.h"
#include "buffer_mgmt_ctrl.h"
#include "rcp_interface.h"
#include "rcp_log.h"
#include "rcp_os.h"
#include "rcp_lwip_netif.h"

enum rcp_tty_flag {
	RCP_TTY_FLAG_OPEN = RCP_FLAG_MAX,
	RCP_TTY_FLAG_CLOSE,
};

#define RCP_WIFI_CTRL_STREAM      RCP_MSG_TYPE_TTY
#define RCP_WIFI_CTRL_NUM_BUFFERS 3
#define RCP_WIFI_CTRL_PAYLOAD_SIZE 256
#define RCP_WIFI_CTRL_BUFFER_SIZE (RCP_WIFI_CTRL_PAYLOAD_SIZE + FRAME_BUFF_PAYLOAD_OFFSET)
#define RCP_WIFI_CMD_TIMEOUT_MS   1000

#define RCP_WIFI_DHCP_TIMEOUT_MS  (15U * 1000U)
#define RCP_WIFI_AT_CMD_MAX_LEN   128

typedef struct {
	buffer_mgmt_ctrl_t *buffer_ctrl;
	lnk_sts_t *lnk_sts;
	uint8_t mac[6];
	volatile bool mac_valid;
} rcp_wifi_hd_t;

static uint8_t g_rcp_wifi_buffer_mem[RCP_WIFI_CTRL_NUM_BUFFERS][RCP_WIFI_CTRL_BUFFER_SIZE];
static rcp_wifi_hd_t g_rcp_wifi;

static int rcp_wifi_escape_at_arg(char *dst, size_t dst_size, const char *src)
{
	size_t in_idx;
	size_t out_idx = 0U;

	if (dst == NULL || dst_size == 0U || src == NULL) {
		return -1;
	}

	for (in_idx = 0U; src[in_idx] != '\0'; ++in_idx) {
		char ch = src[in_idx];

		if (ch == '\\' || ch == '"' || ch == ',') {
			if ((out_idx + 2U) >= dst_size) {
				return -1;
			}
			dst[out_idx++] = '\\';
		} else if ((out_idx + 1U) >= dst_size) {
			return -1;
		}

		dst[out_idx++] = ch;
	}

	dst[out_idx] = '\0';
	return (int)out_idx;
}

static int rcp_wifi_send_bytes(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
	buffer_mgmt_elem_t *msg_buffer = NULL;
	link_result_t send_result;

	if (data == NULL || len == 0U) {
		return RCP_BUS_ERR_INVALID;
	}

	if (g_rcp_wifi.buffer_ctrl == NULL || g_rcp_wifi.lnk_sts == NULL) {
		return RCP_BUS_ERR_NO_INST;
	}

	if (len > RCP_WIFI_CTRL_PAYLOAD_SIZE) {
		return RCP_BUS_ERR_INVALID;
	}

	if (buffer_mgmt_alloc_elem(g_rcp_wifi.buffer_ctrl, &msg_buffer, timeout_ms) != 0 || msg_buffer == NULL) {
		return RCP_BUS_ERR_MEMORY;
	}

	if (!buffer_mgmt_write(msg_buffer, data, len)) {
		buffer_mgmt_free_elem(msg_buffer);
		return RCP_BUS_ERR_INVALID;
	}

	send_result = link_send(RCP_WIFI_CTRL_STREAM, msg_buffer, timeout_ms);
	if (send_result == RCP_LNK_SUCCESS) {
		return RCP_BUS_OK;
	}

	buffer_mgmt_free_elem(msg_buffer);

	if (send_result == RCP_LNK_TIMEOUT) {
		return RCP_BUS_ERR_TIMEOUT;
	}

	if (send_result == RCP_LNK_ERR_TX_NO_RESOURCES || send_result == RCP_LNK_TX_BUSY) {
		return RCP_BUS_ERR_MEMORY;
	}

	return RCP_BUS_ERR_IO;
}

static enum rcp_wifi_status rcp_wifi_status_from_send_result(int send_result)
{
	if (send_result == RCP_BUS_OK) {
		return RCP_WIFI_OK;
	}

	if (send_result == RCP_BUS_ERR_MEMORY) {
		return RCP_WIFI_ERR_MEM;
	}

	if (send_result == RCP_BUS_ERR_INVALID) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	return RCP_WIFI_ERR;
}

static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr)
{
	const uint8_t *payload;
	uint16_t payload_len;

	if (msg_flags == RCP_FLAG_DEVICE_START) {
		printf("[TTY] Open tty\r\n");
		send_rcp_cmd(g_rcp_wifi.lnk_sts, RCP_TTY_FLAG_OPEN);
	}
	if (msg_flags != RCP_FLAG_UPLD_DATA || payload_ptr == NULL) {
		return RCP_RX_NO_ACTION;
	}

	payload = buffer_mgmt_data_ptr_const(payload_ptr);
	payload_len = buffer_mgmt_data_size(payload_ptr);
	if (payload == NULL || payload_len == 0U) {
		return RCP_RX_NO_ACTION;
	}

	printf("%.*s", (int)payload_len, (const char *)payload);

	if (strncmp((const char *)payload, "+CW:CONNECTED\r\n", payload_len) == 0) {
		rcp_lwip_dhcp_start(RCP_WIFI_DHCP_TIMEOUT_MS);
	} else if (strncmp((const char *)payload, "+CW:DISCONNECTED\r\n", payload_len) == 0) {
		rcp_lwip_dhcp_done();
	}

	return RCP_RX_NO_ACTION;
}

static int rcp_wifi_send_at_cmd(const char *cmd)
{
	size_t cmd_len;

	if (cmd == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	cmd_len = strlen(cmd);
	if (cmd_len == 0U || cmd_len > RCP_WIFI_CTRL_PAYLOAD_SIZE) {
		return RCP_BUS_ERR_INVALID;
	}

	return rcp_wifi_send_bytes((const uint8_t *)cmd, (uint16_t)cmd_len, RCP_WIFI_CMD_TIMEOUT_MS);
}

static int rcp_wifi_connect_ap(const char *ssid, const char *psk)
{
	char escaped_ssid[(WIFI_MAX_SSID_SIZE * 2) + 1];
	char escaped_psk[(WIFI_MAX_PSK_SIZE * 2) + 1];
	char cmd[RCP_WIFI_AT_CMD_MAX_LEN];
	int ret;

	if (ssid == NULL || ssid[0] == '\0') {
		return RCP_BUS_ERR_INVALID;
	}

	ret = rcp_wifi_escape_at_arg(escaped_ssid, sizeof(escaped_ssid), ssid);
	if (ret < 0) {
		return RCP_BUS_ERR_INVALID;
	}

	if (psk == NULL) {
		psk = "";
	}

	ret = rcp_wifi_escape_at_arg(escaped_psk, sizeof(escaped_psk), psk);
	if (ret < 0) {
		return RCP_BUS_ERR_INVALID;
	}

	ret = snprintf(cmd, sizeof(cmd), "AT+CWMODE=1\r\n");
	if (ret <= 0 || ret >= (int)sizeof(cmd)) {
		return RCP_BUS_ERR_INVALID;
	}
	(void)rcp_wifi_send_at_cmd(cmd);

	ret = snprintf(cmd, sizeof(cmd), "AT+CWDHCP=0,1\r\n");
	if (ret <= 0 || ret >= (int)sizeof(cmd)) {
		return RCP_BUS_ERR_INVALID;
	}
	(void)rcp_wifi_send_at_cmd(cmd);

	ret = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", escaped_ssid, escaped_psk);
	if (ret <= 0 || ret >= (int)sizeof(cmd)) {
		return RCP_BUS_ERR_INVALID;
	}

	RCP_LOG_INF("Send WiFi connect command, ssid=%s", ssid);
	return rcp_wifi_send_at_cmd(cmd);
}

static int rcp_wifi_disconnect_ap(bool erase_credentials)
{
	if (erase_credentials) {
		return rcp_wifi_send_at_cmd("AT+CWQAP=1\r\n");
	}

	return rcp_wifi_send_at_cmd("AT+CWQAP\r\n");
}

void rcp_wifi_set_mac(const uint8_t mac[6])
{
	if (mac == NULL) {
		return;
	}

	memcpy(g_rcp_wifi.mac, mac, sizeof(g_rcp_wifi.mac));
	g_rcp_wifi.mac_valid = true;
	RCP_LOG_INF("MAC %02X:%02X:%02X:%02X:%02X:%02X",
		    g_rcp_wifi.mac[0], g_rcp_wifi.mac[1], g_rcp_wifi.mac[2],
		    g_rcp_wifi.mac[3], g_rcp_wifi.mac[4], g_rcp_wifi.mac[5]);
}

bool rcp_wifi_get_mac(uint8_t mac[6], uint32_t timeout_ms)
{
	uint32_t start_ms;

	if (mac == NULL) {
		return false;
	}

	start_ms = rcp_os_time_ms();
	while (!g_rcp_wifi.mac_valid) {
		if ((rcp_os_time_ms() - start_ms) >= timeout_ms) {
			return false;
		}
		rcp_os_delay_ms(10);
	}

	memcpy(mac, g_rcp_wifi.mac, 6);
	return true;
}

enum rcp_wifi_status rcp_wifi_connect(struct rcp_wifi_conn_req *req)
{
	char ssid[WIFI_MAX_SSID_SIZE + 1];
	char psk[WIFI_MAX_PSK_SIZE + 1];
	const char *psk_arg = NULL;

	if (req == NULL || req->ssid_len == 0U || req->ssid_len > WIFI_MAX_SSID_SIZE) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	memcpy(ssid, req->ssid, req->ssid_len);
	ssid[req->ssid_len] = '\0';

	if (req->sec_type != RCP_WIFI_SEC_TYPE_NONE && req->sec_type != RCP_WIFI_SEC_TYPE_PSK) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	if (req->sec_type == RCP_WIFI_SEC_TYPE_PSK) {
		if (req->psk_len == 0U || req->psk_len > WIFI_MAX_PSK_SIZE) {
			return RCP_WIFI_ERR_NOT_SUPP;
		}

		memcpy(psk, req->psk, req->psk_len);
		psk[req->psk_len] = '\0';
		psk_arg = psk;
	}

	return rcp_wifi_status_from_send_result(rcp_wifi_connect_ap(ssid, psk_arg));
}

enum rcp_wifi_status rcp_wifi_disconnect(void)
{
	return rcp_wifi_status_from_send_result(rcp_wifi_disconnect_ap(false));
}

enum rcp_wifi_status rcp_wifi_init(void)
{
	buffer_mgmt_ctrl_init_cfg_t cfg = {
		.frame_buff = g_rcp_wifi_buffer_mem,
		.frame_elem_cnt = RCP_WIFI_CTRL_NUM_BUFFERS,
		.frame_elem_size = sizeof(g_rcp_wifi_buffer_mem[0]),
		.name = "rcp_wifi",
	};

	if (g_rcp_wifi.lnk_sts != NULL) {
		return RCP_WIFI_OK;
	}

	if (link_register_rx_handler(RCP_MSG_TYPE_TTY, handle_rcp_msg) != RCP_LNK_SUCCESS) {
		LOG_ERR("Failed to register ETH RX handler");
		return RCP_WIFI_ERR;
	}

	g_rcp_wifi.buffer_ctrl = buffer_mgmt_create(&cfg);
	if (g_rcp_wifi.buffer_ctrl == NULL) {
		return RCP_WIFI_ERR_MEM;
	}

	g_rcp_wifi.lnk_sts = rcp_stream_create(g_rcp_wifi.buffer_ctrl, RCP_WIFI_CTRL_STREAM);
	if (g_rcp_wifi.lnk_sts == NULL) {
		return RCP_WIFI_ERR;
	}

	return RCP_WIFI_OK;
}
