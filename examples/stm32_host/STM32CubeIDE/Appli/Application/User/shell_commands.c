/**
 * @file shell_commands.c
 * @brief UART-based shell commands for WiFi and BT RCP control
 */

#include "shell_commands.h"
//#include "bt_host.h"
#include "main.h"
#include "stm32h7rsxx_hal_uart_ex.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cmsis_os2.h>
#include <wifi/rcp_wifi_mgmt.h>
#include <rcp_interface.h>
#include <rcp_lwip_netif.h>
#include <rcp_user_config.h>
#include <bt_hci.h>
#include <rcp_mcu_capability.h>
#include <rcp_data_test.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <lwip/prot/dhcp.h>
#include <lwip/ip4_addr.h>
#include <lwip/tcpip.h>

void iperf_start(uint8_t is_client, uint8_t is_udp, const char *ip);
TaskHandle_t fhost_iperf_msg_handle_get(void);
int fhost_iperf_sigkill_handler(TaskHandle_t iperf_handle);

/* External UART handle */
extern UART_HandleTypeDef huart3;

#define TESTBUFF_SIZE 1600
/* Shell buffer */
#define SHELL_BUF_SIZE 256
#define SHELL_RX_STREAM_SIZE 512
#define SHELL_UART_RX_CHUNK_SIZE 32
static char shell_buffer[SHELL_BUF_SIZE];
static uint16_t shell_idx = 0;
static uint8_t shell_uart_rx_chunk[SHELL_UART_RX_CHUNK_SIZE];
static uint8_t shell_rx_stream_storage[SHELL_RX_STREAM_SIZE];
static StaticStreamBuffer_t shell_rx_stream_struct;
static StreamBufferHandle_t shell_rx_stream;
static volatile uint32_t shell_rx_dropped;
static bool shell_last_was_cr;

/* Shell task */
osThreadId_t shell_task_handle;
const osThreadAttr_t shell_task_attr = {
	.name = "shell_task",
	.stack_size = 1024 * 4,
	.priority = (osPriority_t)osPriorityLow,
};

/* Function prototypes */
static void shell_task(void *argument);
static void shell_process_command(char *cmd);
static void rcp_test_task(void *argument);
static void shell_handle_char(uint8_t ch);
static void shell_uart_rx_start(void);

/* WiFi commands */
static void cmd_wifi_connect(int argc, char **argv);
static void cmd_wifi_disconnect(int argc, char **argv);
static void cmd_wifi_status(int argc, char **argv);
static void cmd_wifi_scan(int argc, char **argv);

/* iperf commands */
static void cmd_iperf_tcp_tx(int argc, char **argv);
static void cmd_iperf_tcp_rx(int argc, char **argv);
static void cmd_iperf_udp_tx(int argc, char **argv);
static void cmd_iperf_udp_rx(int argc, char **argv);
static void cmd_iperf_stop(int argc, char **argv);

/* BT commands */
static void cmd_bt_init(int argc, char **argv);
static void cmd_bt_scan(int argc, char **argv);
static void cmd_bt_scan_stop(int argc, char **argv);
static void cmd_bt_adv_start(int argc, char **argv);
static void cmd_bt_adv_stop(int argc, char **argv);

/* System commands */
static void cmd_help(int argc, char **argv);
static void cmd_rcp_test(int argc, char **argv);

typedef struct {
	rcp_data_test_mode_t mode;
	uint32_t duration_ms;
} rcp_test_ctx_t;

static osThreadId_t rcp_test_task_handle;
static rcp_test_ctx_t g_rcp_test_ctx;
static const osThreadAttr_t rcp_test_task_attr = {
	.name = "rcp_test",
	.stack_size = 1024 * 4,
	.priority = (osPriority_t)osPriorityNormal1,
};

/* Command structure */
typedef struct {
	enum mcu_cap_type group;
	const char *name;
	void (*handler)(int argc, char **argv);
	const char *help;
} shell_cmd_t;

/* Command table */
static const shell_cmd_t commands[] = {
	{RCP_MCU_CAP_ANY, "help", cmd_help, "Show this help message"},
	{RCP_MCU_CAP_ANY, "rcp_test", cmd_rcp_test, "Run RCP throughput test: rcp_test tx [duration_s] | rcp_test rx | rcp_test stop"},
//
//	/* WiFi commands */
	{RCP_MCU_CAP_WIFI, "wifi_connect", cmd_wifi_connect, "Connect to WiFi: wifi_connect <SSID> [PSK]"},
	{RCP_MCU_CAP_WIFI, "wifi_disconnect", cmd_wifi_disconnect, "Disconnect from WiFi"},
//	{RCP_MCU_CAP_WIFI, "wifi_status", cmd_wifi_status, "Show WiFi and network status (queried from RCP over SDIO)"},
//	{RCP_MCU_CAP_WIFI, "wifi_scan", cmd_wifi_scan, "Run a default scan for APs"},
//
	{RCP_MCU_CAP_WIFI, "iperf_c", cmd_iperf_tcp_tx, "Start iperf TCP TX: iperf_tcp_tx <server_ip>"},
	{RCP_MCU_CAP_WIFI, "iperf_s", cmd_iperf_tcp_rx, "Start iperf TCP RX on default port 5001"},
	{RCP_MCU_CAP_WIFI, "iperf_u_c", cmd_iperf_udp_tx, "Start iperf UDP TX: iperf_udp_tx <server_ip>"},
	{RCP_MCU_CAP_WIFI, "iperf_u_s", cmd_iperf_udp_rx, "Start iperf UDP RX on default port 5001"},
	{RCP_MCU_CAP_WIFI, "iperf_stop", cmd_iperf_stop, "Stop the running iperf session"},

//	{RCP_MCU_CAP_BT, "bt_scan", cmd_bt_scan, "Start BLE scan"},
//	{RCP_MCU_CAP_BT, "bt_scan_stop", cmd_bt_scan_stop, "Stop BLE scan"},
//	{RCP_MCU_CAP_BT, "bt_adv_start", cmd_bt_adv_start, "Start BLE advertising"},
//	{RCP_MCU_CAP_BT, "bt_adv_stop", cmd_bt_adv_stop, "Stop BLE advertising"},
};

/* Display Utilities */
static const char *security2str(enum rcp_wifi_sec_type security_type)
{
	/* Security type */
	const char *sec_str = "Unknown";
	switch (security_type) {
	case RCP_WIFI_SEC_TYPE_NONE:
		sec_str = "Open";
		break;
	case RCP_WIFI_SEC_TYPE_WEP:
		sec_str = "WEP";
		break;
	case RCP_WIFI_SEC_TYPE_WPA_PSK:
		sec_str = "WPA PSK";
		break;
	case RCP_WIFI_SEC_TYPE_PSK:
		sec_str = "WPA2 PSK";
		break;
	case RCP_WIFI_SEC_TYPE_WPA_AUTO_PERSONAL:
		sec_str = "WPA/WPA2/WPA3 PSK";
		break;
	case RCP_WIFI_SEC_TYPE_SAE:
		sec_str = "WPA3 SAE";
		break;
	case RCP_WIFI_SEC_TYPE_EAP:
		sec_str = "WPA2 Enterprise";
		break;
	default:
		sec_str = "Unknown";
		break;
	}
	return sec_str;
}

static void bssid2str(char* bssi_str, uint8_t* bssid)
{
	sprintf(bssi_str, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0],
			bssid[1], bssid[2],
			bssid[3], bssid[4],
			bssid[5]);
}

static const char *band2str(enum rcp_wifi_band band)
{
	/* Band */
	const char *band_str = "Unknown";
	switch (band) {
	case RCP_WIFI_BAND_2_4_GHZ:
		band_str = "2.4GHz";
		break;
	case RCP_WIFI_BAND_5_GHZ:
		band_str = "5GHz";
		break;
	case RCP_WIFI_BAND_6_GHZ:
		band_str = "6GHz";
		break;
	case RCP_WIFI_BAND_ANY:
		band_str = "Any";
		break;
	default:
		band_str = "Unknown";
		break;
	}
	return band_str;
}



static const char *wifi_state_txt(enum rcp_wifi_iface_state state)
{
	switch (state) {
	case RCP_WIFI_STATE_DISCONNECTED:
		return "DISCONNECTED";
	case RCP_WIFI_STATE_INTERFACE_DISABLED:
		return "INTERFACE_DISABLED";
	case RCP_WIFI_STATE_INACTIVE:
		return "INACTIVE";
	case RCP_WIFI_STATE_SCANNING:
		return "SCANNING";
	case RCP_WIFI_STATE_AUTHENTICATING:
		return "AUTHENTICATING";
	case RCP_WIFI_STATE_ASSOCIATING:
		return "ASSOCIATING";
	case RCP_WIFI_STATE_ASSOCIATED:
		return "ASSOCIATED";
	case RCP_WIFI_STATE_4WAY_HANDSHAKE:
		return "4WAY_HANDSHAKE";
	case RCP_WIFI_STATE_GROUP_HANDSHAKE:
		return "GROUP_HANDSHAKE";
	case RCP_WIFI_STATE_COMPLETED:
		return "COMPLETED";
	default:
		return "UNKNOWN";
	}
}

static const char *wifi_mode_txt(enum rcp_wifi_iface_mode mode)
{
	switch (mode) {
	case RCP_WIFI_MODE_INFRA:
		return "STATION";
	case RCP_WIFI_MODE_IBSS:
		return "IBSS";
	case RCP_WIFI_MODE_AP:
		return "AP";
	case RCP_WIFI_MODE_P2P_GO:
		return "P2P_GO";
	case RCP_WIFI_MODE_P2P_GROUP_FORMATION:
		return "P2P_GROUP_FORMATION";
	case RCP_WIFI_MODE_MESH:
		return "MESH";
	default:
		return "UNKNOWN";
	}
}

static const char *wifi_link_mode_txt(enum rcp_wifi_link_mode link_mode)
{
	switch (link_mode) {
	case RCP_WIFI_LINK_MODE_WIFI_0:
		return "WIFI 0 (802.11)";
	case RCP_WIFI_LINK_MODE_WIFI_1:
		return "WIFI 1 (802.11b)";
	case RCP_WIFI_LINK_MODE_WIFI_2:
		return "WIFI 2 (802.11a)";
	case RCP_WIFI_LINK_MODE_WIFI_3:
		return "WIFI 3 (802.11g)";
	case RCP_WIFI_LINK_MODE_WIFI_4:
		return "WIFI 4 (802.11n/HT)";
	case RCP_WIFI_LINK_MODE_WIFI_5:
		return "WIFI 5 (802.11ac/VHT)";
	case RCP_WIFI_LINK_MODE_WIFI_6:
		return "WIFI 6 (802.11ax/HE)";
	case RCP_WIFI_LINK_MODE_WIFI_6E:
		return "WIFI 6E (802.11ax/HE 6GHz)";
	case RCP_WIFI_LINK_MODE_WIFI_7:
		return "WIFI 7 (802.11be/EHT)";
	default:
		return "UNKNOWN";
	}
}

static const char *wifi_wpa3_enterprise_txt(enum rcp_wifi_wpa_ent_type type)
{
	switch (type) {
	case RCP_WIFI_WPA3_TYPE_NA:
		return "NA";
	case RCP_WIFI_WPA3_SUITEB:
		return "SUITEB";
	case RCP_WIFI_WPA3_SUITEB_192:
		return "SUITEB_192";
	case RCP_WIFI_WPA3_PA3_ONLY:
		return "WPA3_ONLY";
	default:
		return "NA";
	}
}

static const char *wifi_mfp_txt(enum rcp_wifi_mfp_options mfp)
{
	switch (mfp) {
	case RCP_WIFI_MFP_DISABLE:
		return "Disabled";
	case RCP_WIFI_MFP_OPTIONAL:
		return "Optional";
	case RCP_WIFI_MFP_REQUIRED:
		return "Required";
	default:
		return "Unknown";
	}
}

static void display_ipv4_config(void)
{
	struct netif *netif = rcp_netif_get();
	if (netif && netif_is_up(netif)) {
		printf("\n=== IPv4 Configuration ===\n");
		printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
		printf("Netmask:    %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
		printf("Gateway:    %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));

#if LWIP_DHCP
		struct dhcp *dhcp = netif_dhcp_data(netif);
		if (dhcp && dhcp->state != DHCP_STATE_OFF) {
			if (dhcp_supplied_address(netif)) {
				printf("DHCP:       Enabled (Address obtained)\n");
			} else {
				printf("DHCP:       Enabled (State: %d, waiting for address...)\n",
				       dhcp->state);
			}
		} else {
			printf("DHCP:       Not started\n");
		}
#endif
	} else if (netif) {
		printf("\nNetwork interface: DOWN\n");
	} else {
		printf("\nNetwork interface: Not initialized\n");
	}
	printf("\n");
}

static void display_iface_status(const struct rcp_wifi_evt_iface_status *evt)
{
	printf("\nStatus: successful\n");
	printf("==================\n");
	printf("State: %s\n", wifi_state_txt(evt->status.state));

	if (evt->status.state >= RCP_WIFI_STATE_ASSOCIATED &&
	    evt->status.state <= RCP_WIFI_STATE_COMPLETED) {
		char bssid_str[20] = {};
		bssid2str(bssid_str, (uint8_t *)evt->status.bssid);

		printf("Interface Mode: %s\n", wifi_mode_txt(evt->status.iface_mode));
		printf("Link Mode: %s\n", wifi_link_mode_txt(evt->status.link_mode));
		printf("SSID: %.*s\n", evt->status.ssid_len, (const char *)evt->status.ssid);
		printf("BSSID: %s\n", bssid_str);
		printf("Band: %s\n", band2str(evt->status.band));
		printf("Channel: %lu\n", evt->status.channel);
		printf("Security: %s %s\n", wifi_wpa3_enterprise_txt(evt->status.wpa3_ent_type),
		       security2str(evt->status.security));
		printf("MFP: %s\n", wifi_mfp_txt(evt->status.mfp));
		if (evt->status.iface_mode == RCP_WIFI_MODE_INFRA) {
			printf("RSSI: %ld\n", evt->status.rssi);
		}
		printf("Beacon Interval: %lu\n", evt->status.beacon_interval);
		printf("DTIM: %lu\n", evt->status.dtim_period);
		printf("TWT: %s\n", evt->status.twt_capable ? "Supported" : "Not supported");
		printf("Current PHY TX rate (Mbps) : %lu\n", (evt->status.tx_bitrate_kbps / 1000U));
	}

	display_ipv4_config();
}

static void display_scan_result(struct rcp_wifi_evt_scan_result *result)
{
	char ssid_str[WIFI_MAX_SSID_SIZE] = {};
	char bssid_str[20];
	bssid2str(bssid_str, result->bssid);
	memcpy(ssid_str, result->ssid.bytes, result->ssid.size);
	printf("SSID: %-32s    rssi %3d   sec: %s\n",
		ssid_str, result->rssi, security2str(result->sec_type));
	printf("   BSSID: %s   Band: %-7s    Channel: %d\n" ,
		bssid_str, band2str(result->band), result->channel);
}

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* Static netif (must persist) */
static struct netif *rcp_netif;
static bool netif_initialized;

/* Store current WiFi connection details for status display */
static struct {
	bool connected;
	char ssid[WIFI_MAX_SSID_SIZE + 1];
	char psk[WIFI_MAX_PSK_SIZE + 1];
	uint8_t bssid[WIFI_BSSID_SIZE];
	enum rcp_wifi_sec_type sec_type;
	uint32_t channel;
	enum rcp_wifi_band band;
} wifi_conn_info = {0};

/* --- iface status (SDIO query) synchronization --- */
static osSemaphoreId_t iface_status_sem;
static volatile bool iface_status_waiting;
static volatile bool iface_status_valid;
static struct rcp_wifi_evt_iface_status iface_status_last;
static volatile int32_t iface_status_err;

/* BT scan callback */
static void bt_scan_callback(struct bt_scan_result *res)
{
	/* Simplified scan result printing */
	printf("[BT] Device found\n");
	printf("rcp> ");
	fflush(stdout);
}

/* Initialize shell */
bool shell_commands_init(void)
{
	/* Get the current interface (to be used by wifi handlers */
	if (!netif_initialized) {
		rcp_netif = rcp_netif_get();
		if (!rcp_netif) {
			printf("Failed to find netif\n");
			return false;
		}
		netif_initialized = true;
	}

	return true;
}

/* Start shell task */
void shell_task_start(void)
{
	if (shell_rx_stream == NULL) {
		shell_rx_stream = xStreamBufferCreateStatic(
			sizeof(shell_rx_stream_storage),
			1,
			shell_rx_stream_storage,
			&shell_rx_stream_struct);
		if (shell_rx_stream == NULL) {
			printf("[ERROR] Failed to create shell RX stream buffer\n");
			return;
		}
	}

	shell_idx = 0;
	shell_rx_dropped = 0;
	shell_last_was_cr = false;
	(void)xStreamBufferReset(shell_rx_stream);
	shell_uart_rx_start();

	shell_task_handle = osThreadNew(shell_task, NULL, &shell_task_attr);
	if (shell_task_handle == NULL) {
		printf("[ERROR] Failed to create shell task\n");
	}
}

static void shell_uart_rx_start(void)
{
	(void)HAL_UARTEx_ReceiveToIdle_IT(&huart3, shell_uart_rx_chunk,
					  sizeof(shell_uart_rx_chunk));
}

static void shell_handle_char(uint8_t ch)
{
	if (ch == '\n' && shell_last_was_cr) {
		shell_last_was_cr = false;
		return;
	}

	shell_last_was_cr = (ch == '\r');

	if (ch == '\b' || ch == 127U) {
		if (shell_idx > 0) {
			shell_idx--;
			printf(" \b");
			fflush(stdout);
		}
		return;
	}

	if (ch == '\r' || ch == '\n') {
		printf("\n");
		if (shell_idx > 0) {
			shell_buffer[shell_idx] = '\0';
			shell_process_command(shell_buffer);
			shell_idx = 0;
		}
		printf("rcp> ");
		fflush(stdout);
		return;
	}

	if (shell_idx < (SHELL_BUF_SIZE - 1U)) {
		shell_buffer[shell_idx++] = (char)ch;
	}
}

/* Shell task implementation */
static void shell_task(void *argument)
{
	uint8_t rx_buf[SHELL_UART_RX_CHUNK_SIZE];
	size_t rx_len;
	size_t i;

	(void)argument;

	printf("rcp> ");
	fflush(stdout);

	while (1) {
		if (shell_rx_stream == NULL) {
			osDelay(10);
			continue;
		}

		rx_len = xStreamBufferReceive(shell_rx_stream, rx_buf, sizeof(rx_buf),
					      portMAX_DELAY);
		for (i = 0; i < rx_len; ++i) {
			shell_handle_char(rx_buf[i]);
		}
	}
}

static void rcp_test_task(void *argument)
{
	rcp_test_ctx_t ctx = *(rcp_test_ctx_t *)argument;

	(void)start_data_test(ctx.mode, ctx.duration_ms);
	rcp_test_task_handle = NULL;
	osThreadExit();
}

/* Process shell command */
static void shell_process_command(char *cmd)
{
	char *argv[10];
	int argc = 0;
	char *token;

	/* Tokenize command */
	token = strtok(cmd, " ");
	while (token != NULL && argc < 10) {
		argv[argc++] = token;
		token = strtok(NULL, " ");
	}

	if (argc == 0) {
		return;
	}

	/* Find and execute command */
	for (size_t i = 0; i < NUM_COMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			commands[i].handler(argc, argv);
			return;
		}
	}

	printf("Unknown command: %s\n", argv[0]);
	printf("Type 'help' for available commands\n");
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
	BaseType_t higher_priority_task_woken = pdFALSE;
	size_t sent;

	if (huart == NULL || huart->Instance != USART3) {
		return;
	}

	if (size > 0U && shell_rx_stream != NULL) {
		sent = xStreamBufferSendFromISR(shell_rx_stream, shell_uart_rx_chunk,
						size, &higher_priority_task_woken);
		if (sent < size) {
			shell_rx_dropped += (uint32_t)(size - sent);
		}
	}

	shell_uart_rx_start();
	portYIELD_FROM_ISR(higher_priority_task_woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart == NULL || huart->Instance != USART3) {
		return;
	}

	__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF |
				      UART_CLEAR_PEF | UART_CLEAR_FEF | UART_CLEAR_IDLEF);
	shell_uart_rx_start();
}

/* Help command */
static void cmd_help(int argc, char **argv)
{
	printf("\nAvailable commands:\n");
	printf("-------------------\n");
	for (size_t i = 0; i < NUM_COMMANDS; i++) {
		printf("  %-20s - %s\n", commands[i].name, commands[i].help);
	}
	printf("\n");
}

/* ============================================================================
   WiFi Commands
   ============================================================================ */

static void cmd_wifi_status(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (!iface_status_sem) {
		iface_status_sem = osSemaphoreNew(1, 0, NULL);
		if (!iface_status_sem) {
			printf("Failed to create iface status semaphore\n");
			return;
		}
	}

	/* Drain any stale signals */
	while (osSemaphoreAcquire(iface_status_sem, 0) == osOK) {
		/* no-op */
	}

	iface_status_err = 0;
	iface_status_valid = false;
	iface_status_waiting = true;

	printf("\nRequesting WiFi interface status (over SDIO)...\n");

	enum rcp_wifi_status status = rcp_wifi_get_iface_status(1);
	if (status != RCP_WIFI_OK) {
		iface_status_waiting = false;
		printf("Status request failed to send: %d\n\n", status);
		return;
	}

	/* Wait for response */
	osStatus_t wait = osSemaphoreAcquire(iface_status_sem, 2000);
	if (wait != osOK) {
		iface_status_waiting = false;
		printf("Timed out waiting for iface status response\n\n");
		return;
	}

	if (iface_status_err != 0) {
		printf("Iface status request failed, error_code=%ld\n\n", (long)iface_status_err);
		return;
	}

	if (!iface_status_valid) {
		printf("Iface status response missing\n\n");
		return;
	}

	display_iface_status(&iface_status_last);
}

static void cmd_wifi_connect(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: wifi_connect <SSID> [PSK]\n");
		return;
	}

	struct rcp_wifi_conn_req req = {0};
	strncpy((char *)req.ssid, argv[1], sizeof(req.ssid) - 1);
	req.ssid_len = strlen(argv[1]);

	/* Store connection details for status display */
	memset(&wifi_conn_info, 0, sizeof(wifi_conn_info));
	strncpy(wifi_conn_info.ssid, argv[1], sizeof(wifi_conn_info.ssid) - 1);
	wifi_conn_info.band = RCP_WIFI_BAND_ANY;
	wifi_conn_info.sec_type = RCP_WIFI_SEC_TYPE_NONE;

	if (argc >= 3) {
		strncpy((char *)req.psk, argv[2], sizeof(req.psk) - 1);
		req.psk_len = strlen(argv[2]);
		req.sec_type = RCP_WIFI_SEC_TYPE_PSK;

		/* Store password (for masked display) */
		strncpy(wifi_conn_info.psk, argv[2], sizeof(wifi_conn_info.psk) - 1);
		wifi_conn_info.sec_type = RCP_WIFI_SEC_TYPE_PSK;
	} else {
		req.sec_type = RCP_WIFI_SEC_TYPE_NONE;
	}

	printf("Connecting to '%s'...\n", argv[1]);
	enum rcp_wifi_status status = rcp_wifi_connect(&req);
	if (status == RCP_WIFI_OK) {
		printf("Connection request sent\n");
	} else {
		printf("Connection failed: %d\n", status);
		memset(&wifi_conn_info, 0, sizeof(wifi_conn_info));
	}
}

static void cmd_wifi_disconnect(int argc, char **argv)
{
	printf("Disconnecting from WiFi...\n");
	enum rcp_wifi_status status = rcp_wifi_disconnect();
	if (status == RCP_WIFI_OK) {
		printf("Disconnect request sent\n");
	} else {
		printf("Disconnect failed: %d\n", status);
	}
}

static void cmd_wifi_scan(int argc, char **argv)
{
	struct rcp_wifi_scan_params params;
	params.scan_type = RCP_WIFI_SCAN_PASSIVE;
	params.ssids->size = 0;
	params.ssids_count = 1;
	params.max_bss_cnt = 0;
	params.dwell_time_active = 20;
	params.dwell_time_passive = 130;
	params.bands_bitmap = 0x7;

	params.band_chan_count = 0;
	enum rcp_wifi_status status = rcp_wifi_scan(1, &params);
	if (status == RCP_WIFI_OK) {
		printf("Default Scan request sent\n");
	} else {
		printf("Scan Request failed: %d\n", status);
	}
}

static bool iperf_parse_server_ip(const char *ip_str)
{
	ip_addr_t server_ip;

	if (ipaddr_aton(ip_str, &server_ip) == 0) {
		printf("Failed to parse IP address: %s\n", ip_str);
		return false;
	}

	return true;
}

static void iperf_warn_extra_args(int argc, int expected_argc)
{
	if (argc > expected_argc) {
		printf("Using default port 5001, extra arguments are ignored\n");
	}
}

static void cmd_iperf_tcp_tx(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: iperf_tcp_tx <server_ip>\n");
		return;
	}

	if (!iperf_parse_server_ip(argv[1])) {
		return;
	}

	iperf_warn_extra_args(argc, 2);
	iperf_start(1, 0, argv[1]);
	printf("Started iperf TCP TX to %s on port 5001\n", argv[1]);
}

static void cmd_iperf_tcp_rx(int argc, char **argv)
{
	iperf_warn_extra_args(argc, 1);
	iperf_start(0, 0, "0.0.0.0");
	printf("Started iperf TCP RX on port 5001\n");
}

static void cmd_iperf_udp_tx(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: iperf_udp_tx <server_ip>\n");
		return;
	}

	if (!iperf_parse_server_ip(argv[1])) {
		return;
	}

	iperf_warn_extra_args(argc, 2);
	iperf_start(1, 1, argv[1]);
	printf("Started iperf UDP TX to %s on port 5001\n", argv[1]);
}

static void cmd_iperf_udp_rx(int argc, char **argv)
{
	iperf_warn_extra_args(argc, 1);
	iperf_start(0, 1, "0.0.0.0");
	printf("Started iperf UDP RX on port 5001\n");
}

static void cmd_iperf_stop(int argc, char **argv)
{
	TaskHandle_t iperf_handle;

	(void)argc;
	(void)argv;

	iperf_handle = fhost_iperf_msg_handle_get();
	if (iperf_handle == NULL) {
		printf("No running iperf session\n");
		return;
	}

	if (fhost_iperf_sigkill_handler(iperf_handle) != 0) {
		printf("Failed to stop iperf session\n");
	}
}

/* ============================================================================
   Bluetooth Commands
   ============================================================================ */
//
//static void cmd_bt_init(int argc, char **argv)
//{
//	printf("Initializing Bluetooth...\n");
//	if (bt_hci_init_default()) {
//		printf("Bluetooth initialized successfully\n");
//	} else {
//		printf("Bluetooth initialization failed\n");
//	}
//}
//
//static void cmd_bt_scan(int argc, char **argv)
//{
//	printf("Starting BLE scan...\n");
//	if (bt_le_scan(bt_scan_callback)) {
//		printf("BLE scan started\n");
//	} else {
//		printf("Failed to start BLE scan\n");
//	}
//}
//
//static void cmd_bt_scan_stop(int argc, char **argv)
//{
//	printf("Stopping BLE scan...\n");
//	if (bt_le_scan_stop()) {
//		printf("BLE scan stopped\n");
//	} else {
//		printf("Failed to stop BLE scan\n");
//	}
//}
//
//static void cmd_bt_adv_start(int argc, char **argv)
//{
//	printf("Starting BLE advertising...\n");
//
//	/* Default advertising parameters */
//	struct bt_adv_params adv_param = {
//		.interval_min = 0x0020,
//		.interval_max = 0x0020,
//		.type = 0x00,
//		.own_addr_type = 0x00,
//		.direct_addr_type = 0x00,
//		.direct_addr = {0},
//		.channel_map = 0x07,
//		.filter_policy = 0x00,
//	};
//
//	/* Default advertising data */
//	uint8_t adv_data[] = {
//		2, 0x01, 0x06, 7, 0x09, 'Q', 'C', 'C', 'R', 'C', 'P',
//	};
//
//	if (bt_le_adv_start(&adv_param, adv_data, sizeof(adv_data))) {
//		printf("BLE advertising started\n");
//	} else {
//		printf("Failed to start BLE advertising\n");
//	}
//}
//
//static void cmd_bt_adv_stop(int argc, char **argv)
//{
//	printf("Stopping BLE advertising...\n");
//	if (bt_le_adv_stop()) {
//		printf("BLE advertising stopped\n");
//	} else {
//		printf("Failed to stop BLE advertising\n");
//	}
//}

static void cmd_rcp_test(int argc, char **argv)
{
	rcp_data_test_mode_t mode;
	uint32_t duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;

	if (argc < 2) {
		printf("usage: rcp_test tx [duration_s]\n");
		printf("       rcp_test rx\n");
		printf("       rcp_test stop\n");
		printf("  tx          : host only sends packets to the SDIO slave\n");
		printf("  rx          : host only receives packets until stop command\n");
		printf("  stop        : stop the running TX/RX test\n");
		printf("  duration_s : TX test duration in s (default: %lu)\n", (unsigned long)RCP_DATA_TEST_DEFAULT_DURATION_S);
		return;
	}

	if (strcmp(argv[1], "stop") == 0) {
		if (stop_data_test()) {
			printf("RCP data test stop requested\n");
		} else {
			printf("No RCP data test is running\n");
		}
		return;
	}

	if (rcp_test_task_handle != NULL) {
		printf("RCP data test is already running\n");
		return;
	}

	if (strcmp(argv[1], "tx") == 0) {
		mode = RCP_DATA_TEST_MODE_TX;
	} else if (strcmp(argv[1], "rx") == 0) {
		mode = RCP_DATA_TEST_MODE_RX;
	} else {
		printf("rcp_test mode must be 'tx', 'rx' or 'stop'\n");
		return;
	}

	if ((mode == RCP_DATA_TEST_MODE_TX) && (argc >= 3)) {
		duration_s = (uint32_t)atoi(argv[2]);
		if (duration_s == 0) {
			printf("Invalid duration, using default %lu s\n", (unsigned long)RCP_DATA_TEST_DEFAULT_DURATION_S);
			duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;
		}
	}
	if ((mode == RCP_DATA_TEST_MODE_RX) && (argc >= 3)) {
		printf("RX mode ignores duration and runs until stop command\n");
	}

	g_rcp_test_ctx.mode = mode;
	g_rcp_test_ctx.duration_ms = duration_s * 1000U;
	rcp_test_task_handle = osThreadNew(rcp_test_task, &g_rcp_test_ctx, &rcp_test_task_attr);
	if (rcp_test_task_handle == NULL) {
		printf("Failed to start RCP data test task\n");
	}
}
