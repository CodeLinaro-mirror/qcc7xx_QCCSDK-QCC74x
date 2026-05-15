/**
 * @file shell_commands.c
 * @brief UART-based shell commands for WiFi and BT RCP control
 */

#include "shell_commands.h"
#include "main.h"
#include "stm32h7rsxx_hal_uart_ex.h"
// #include "bt_host.h"
#include "app_ble.h"
#include "rcp_api.h"
#include "FreeRTOS.h"
#include "main.h"
#include "sockets.h"
#include "stm32h7rsxx_hal_uart_ex.h"
#include "stream_buffer.h"
#include "task.h"
#include <bt_hci.h>
#include <cmsis_os2.h>
#include <lwip/dhcp.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/prot/dhcp.h>
#include <rcp_data_test.h>
#include <rcp_interface.h>
#include <rcp_bus_sdio_master.h>
#include <rcp_lwip_netif.h>
#include <rcp_mcu_capability.h>
#include <rcp_user_config.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <wifi/rcp_wifi_mgmt.h>

void iperf_start(uint8_t is_client, uint8_t is_udp, const char *ip);
TaskHandle_t fhost_iperf_msg_handle_get(void);
int fhost_iperf_sigkill_handler(TaskHandle_t iperf_handle);

/* External UART handle */
extern UART_HandleTypeDef huart3;

#define TESTBUFF_SIZE 1600
#define OTA_TCP_DEFAULT_PORT 3365
#define OTA_HEADER_PACKET_SIZE 512U
#define OTA_TRANSFER_CHUNK_SIZE 1024U
#define MINIDUMP_CACHE_SIZE 1024U
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

typedef struct {
  union {
    struct {
      uint8_t header[16];
      uint8_t type[4];
      uint32_t len;
      uint8_t pad0[8];
      uint8_t ver_hardware[16];
      uint8_t ver_software[16];
      uint8_t sha256[32];
    } s;
    uint8_t raw[OTA_HEADER_PACKET_SIZE];
  } u;
} ota_header_t;

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
static int ota_recv_exact(int sock, uint8_t *buf, uint32_t len);
static int ota_parse_header(const ota_header_t *ota_header, uint32_t *body_len,
                            uint8_t *use_xz);
static void ota_dump_sha256(const uint8_t sha256[32]);

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
static bool hci_initialized = false;
static void hci_cli_init(void);
static void cmd_hci_reset(int argc, char **argv);
static void cmd_hci_set_event_mask(int argc, char **argv);
static void cmd_hci_le_set_event_mask(int argc, char **argv);
static void cmd_hci_read_version(int argc, char **argv);
static void cmd_hci_send(int argc, char **argv);
static void cmd_bt_adv_start(int argc, char **argv);
static void cmd_bt_adv_stop(int argc, char **argv);
static void cmd_bt_add_service(int argc, char **argv);

/* System commands */
static void cmd_help(int argc, char **argv);
static void cmd_rcp_test(int argc, char **argv);
static void cmd_dump(int argc, char **argv);

/* AT commands */
static void cmd_ota_start(int argc, char **argv);
static void cmd_send_RCP(int argc, char **argv);
static void cmd_send_RCP_RST(int argc, char **argv);
static void cmd_send_RCP_SYSRAM(int argc, char **argv);
static void cmd_send_RCP_RESTORE(int argc, char **argv);
static void cmd_send_RCP_SYSSTORE(int argc, char **argv);
static void cmd_send_RCP_EFUSE_R(int argc, char **argv);
static void cmd_send_RCP_IORST(int argc, char **argv);
static void cmd_send_RCP_FS_DEL(int argc, char **argv);
static void cmd_send_RCP_FS_CREATE(int argc, char **argv);
static void cmd_send_RCP_FS_SIZE(int argc, char **argv);
static void cmd_send_RCP_FS_LS(int argc, char **argv);
static void cmd_send_RCP_OTASEND(int argc, char **argv);
static void cmd_send_RCP_FS_WRITE(int argc, char **argv);
static void cmd_send_RCP_FS_READ(int argc, char **argv);
static void cmd_send_RCP_CMD(int argc, char **argv);
static void cmd_send_RCPE(int argc, char **argv);
static void cmd_send_RCP_GMAC(int argc, char **argv);
static void cmd_send_RCP_GMR(int argc, char **argv);
static void cmd_send_RCP_PN(int argc, char **argv);
static void cmd_send_RCP_VBAT(int argc, char **argv);
static void cmd_send_RCP_SYSMSG(int argc, char **argv);
static void cmd_send_RCP_SYSLOG(int argc, char **argv);
static void cmd_send_RCP_TEMP(int argc, char **argv);
static void cmd_send_RCP_EFUSE_CFM(int argc, char **argv);
static void cmd_send_RCP_FLASH_E(int argc, char **argv);
static void cmd_send_RCP_IOPUPD(int argc, char **argv);
static void cmd_send_RCP_IOOUT(int argc, char **argv);
static void cmd_send_RCP_IOIN(int argc, char **argv);
static void cmd_send_RCP_SYSSTORE_SET(int argc, char **argv);
static void cmd_send_RCP_EFUSE_W(int argc, char **argv);
static void cmd_send_RCP_EFUSE_W_HEX(int argc, char **argv);
static void cmd_send_RCP_EFUSE_R(int argc, char **argv);
static void cmd_send_RCP_EFUSE_R_HEX(int argc, char **argv);
static void cmd_send_RCP_FLASH_W(int argc, char **argv);
static void cmd_send_RCP_FLASH_W_HEX(int argc, char **argv);
static void cmd_send_RCP_FLASH_R(int argc, char **argv);
static void cmd_send_RCP_FLASH_R_HEX(int argc, char **argv);
static void cmd_send_RCP_PART(int argc, char **argv);
static void cmd_send_RCP_MINIDUMP(int argc, char **argv);
static void cmd_send_RCP_PWR(int argc, char **argv);

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
	{RCP_MCU_CAP_ANY, "dump", cmd_dump, "Dump host RCP/SDIO/flow-control/buffer status"},
    {RCP_MCU_CAP_ANY, "rcp_test", cmd_rcp_test,
     "Run RCP throughput test: rcp_test tx [duration_s] | rcp_test rx | "
     "rcp_test stop"},
    //	/* WiFi commands */

    {RCP_MCU_CAP_WIFI, "wifi_connect", cmd_wifi_connect,
     "Connect to WiFi: wifi_connect <SSID> [PSK]"},
    {RCP_MCU_CAP_WIFI, "wifi_disconnect", cmd_wifi_disconnect,
     "Disconnect from WiFi"},
    //	{RCP_MCU_CAP_WIFI, "wifi_status", cmd_wifi_status, "Show WiFi and
    // network status (queried from RCP over SDIO)"}, 	{RCP_MCU_CAP_WIFI,
    //"wifi_scan", cmd_wifi_scan, "Run a default scan for APs"},

    	/* HCI commands */
  	{RCP_MCU_CAP_BT, "hci_reset", cmd_hci_reset, "Send HCI Reset command"},
	  {RCP_MCU_CAP_BT, "hci_set_event_mask", cmd_hci_set_event_mask, "Send HCI Set Event Mask"},
	  {RCP_MCU_CAP_BT, "hci_le_set_event_mask", cmd_hci_le_set_event_mask, "Send LE Set Event Mask"},
	  {RCP_MCU_CAP_BT, "hci_read_version", cmd_hci_read_version, "Read Local Version Information"},
	  {RCP_MCU_CAP_BT, "bt_adv_start", cmd_bt_adv_start, "Start BLE advertising: bt_adv_start [name]"},
	  {RCP_MCU_CAP_BT, "bt_adv_stop", cmd_bt_adv_stop, "Stop BLE advertising"},
	  {RCP_MCU_CAP_BT, "bt_add_service", cmd_bt_add_service, "Add example GATT service (call before advertising)"},

    {RCP_MCU_CAP_WIFI, "iperf_c", cmd_iperf_tcp_tx,"Start iperf TCP TX: iperf_tcp_tx <server_ip>"},
    {RCP_MCU_CAP_WIFI, "iperf_s", cmd_iperf_tcp_rx,"Start iperf TCP RX on default port 5001"},
    {RCP_MCU_CAP_WIFI, "iperf_u_c", cmd_iperf_udp_tx,"Start iperf UDP TX: iperf_udp_tx <server_ip>"},
    {RCP_MCU_CAP_WIFI, "iperf_u_s", cmd_iperf_udp_rx,"Start iperf UDP RX on default port 5001"},
    {RCP_MCU_CAP_WIFI, "iperf_stop", cmd_iperf_stop,"Stop the running iperf session"},

    {RCP_MCU_CAP_ANY, "ota_start", cmd_ota_start,"ota_start <server_ip> [port], default port is 3365"},
    {RCP_MCU_CAP_ANY, "RCP", cmd_send_RCP, "send basic AT probe to the module"},
    {RCP_MCU_CAP_ANY, "RCP_SYSRAM", cmd_send_RCP_SYSRAM,"send RCP_SYSRAM to the module"},
    {RCP_MCU_CAP_ANY, "RCP_SYSSTORE", cmd_send_RCP_SYSSTORE,"query SYSSTORE mode"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE-R", cmd_send_RCP_EFUSE_R,"read EFUSE: RCP_EFUSE-R <addr> <nbytes> [reload]"},
    {RCP_MCU_CAP_ANY, "RCP_IORST", cmd_send_RCP_IORST,"reset GPIO: RCP_IORST <pin>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_DEL", cmd_send_RCP_FS_DEL,"delete file: RCP_FS_DEL <filename>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_CREATE", cmd_send_RCP_FS_CREATE,"create file: RCP_FS_CREATE <filename>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_WRITE", cmd_send_RCP_FS_WRITE,"write file data: RCP_FS_WRITE <filename> <offset> <data>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_READ", cmd_send_RCP_FS_READ,"read file data: RCP_FS_READ <filename> <offset> <len>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_SIZE", cmd_send_RCP_FS_SIZE,"get file size: RCP_FS_SIZE <filename>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_LS", cmd_send_RCP_FS_LS,"list files in filesystem"},
    {RCP_MCU_CAP_ANY, "RCPE", cmd_send_RCPE,"set echo mode: RCPE <0|1> (0=off, 1=on)"},
    {RCP_MCU_CAP_ANY, "RCP_GMAC", cmd_send_RCP_GMAC,"read 3 MAC slots: RCP_GMAC"},
    {RCP_MCU_CAP_ANY, "RCP_GMR", cmd_send_RCP_GMR,"query module version info: RCP_GMR"},
    {RCP_MCU_CAP_ANY, "RCP_PN", cmd_send_RCP_PN, "read part number: RCP_PN"},
    {RCP_MCU_CAP_ANY, "RCP_VBAT", cmd_send_RCP_VBAT,"read supply voltage: RCP_VBAT"},
    {RCP_MCU_CAP_ANY, "RCP_SYSMSG", cmd_send_RCP_SYSMSG,"query/set system message: RCP_SYSMSG [state]"},
    {RCP_MCU_CAP_ANY, "RCP_SYSLOG", cmd_send_RCP_SYSLOG,"query/set error code output: RCP_SYSLOG [0|1]"},
    {RCP_MCU_CAP_ANY, "RCP_TEMP", cmd_send_RCP_TEMP,"read chip temperature: RCP_TEMP"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE-CFM", cmd_send_RCP_EFUSE_CFM,"confirm eFuse write: RCP_EFUSE-CFM"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH-E", cmd_send_RCP_FLASH_E,"erase flash: RCP_FLASH-E <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_IOPUPD", cmd_send_RCP_IOPUPD,"GPIO pull config: RCP_IOPUPD <pin> <0|1|2>"},
    {RCP_MCU_CAP_ANY, "RCP_IOOUT", cmd_send_RCP_IOOUT,"GPIO output level: RCP_IOOUT <pin> <0|1>"},
    {RCP_MCU_CAP_ANY, "RCP_IOIN", cmd_send_RCP_IOIN,"GPIO input: RCP_IOIN <pin> [pull]|?"},
    {RCP_MCU_CAP_ANY, "RCP_SYSSTORE_SET", cmd_send_RCP_SYSSTORE_SET,"set store mode: RCP_SYSSTORE_SET <0|1>"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_W", cmd_send_RCP_EFUSE_W,"write eFuse raw: RCP_EFUSE_W <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_W_HEX", cmd_send_RCP_EFUSE_W_HEX,"write eFuse hex: RCP_EFUSE_W_HEX <nbytes> <addr> <hex>"},
	{RCP_MCU_CAP_ANY, "RCP_EFUSE_R", cmd_send_RCP_EFUSE_R,"read eFuse hex: RCP_EFUSE_R <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_R_HEX", cmd_send_RCP_EFUSE_R_HEX,"read eFuse hex: RCP_EFUSE_R_HEX <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_W", cmd_send_RCP_FLASH_W,"write flash raw: RCP_FLASH_W <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_W_HEX", cmd_send_RCP_FLASH_W_HEX,"write flash hex: RCP_FLASH_W_HEX <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_R", cmd_send_RCP_FLASH_R,"read flash raw: RCP_FLASH_R <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_R_HEX", cmd_send_RCP_FLASH_R_HEX,"read flash hex: RCP_FLASH_R_HEX <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_PART", cmd_send_RCP_PART,"query partition table: RCP_PART"},
    {RCP_MCU_CAP_ANY, "RCP_MINIDUMP", cmd_send_RCP_MINIDUMP,"RCP_MINIDUMP"},
	{RCP_MCU_CAP_ANY, "RCP_PWR", cmd_send_RCP_PWR,"RCP_PWR"},
    //	{RCP_MCU_CAP_BT, "bt_scan", cmd_bt_scan, "Start BLE scan"},
    //	{RCP_MCU_CAP_BT, "bt_scan_stop", cmd_bt_scan_stop, "Stop BLE scan"},
    //	{RCP_MCU_CAP_BT, "bt_adv_start", cmd_bt_adv_start, "Start BLE
    // advertising"}, 	{RCP_MCU_CAP_BT, "bt_adv_stop", cmd_bt_adv_stop, "Stop
    // BLE advertising"},
};

/* Display Utilities */
static const char *security2str(enum rcp_wifi_sec_type security_type) {
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

static void bssid2str(char *bssi_str, uint8_t *bssid) {
  sprintf(bssi_str, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1],
          bssid[2], bssid[3], bssid[4], bssid[5]);
}

static const char *band2str(enum rcp_wifi_band band) {
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

static const char *wifi_state_txt(enum rcp_wifi_iface_state state) {
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

static const char *wifi_mode_txt(enum rcp_wifi_iface_mode mode) {
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

static const char *wifi_link_mode_txt(enum rcp_wifi_link_mode link_mode) {
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

static const char *wifi_wpa3_enterprise_txt(enum rcp_wifi_wpa_ent_type type) {
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

static const char *wifi_mfp_txt(enum rcp_wifi_mfp_options mfp) {
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

static void display_ipv4_config(void) {
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

static void display_iface_status(const struct rcp_wifi_evt_iface_status *evt) {
  printf("\nStatus: successful\n");
  printf("==================\n");
  printf("State: %s\n", wifi_state_txt(evt->status.state));

  if (evt->status.state >= RCP_WIFI_STATE_ASSOCIATED &&
      evt->status.state <= RCP_WIFI_STATE_COMPLETED) {
    char bssid_str[20] = {};
    bssid2str(bssid_str, (uint8_t *)evt->status.bssid);

    printf("Interface Mode: %s\n", wifi_mode_txt(evt->status.iface_mode));
    printf("Link Mode: %s\n", wifi_link_mode_txt(evt->status.link_mode));
    printf("SSID: %.*s\n", evt->status.ssid_len,
           (const char *)evt->status.ssid);
    printf("BSSID: %s\n", bssid_str);
    printf("Band: %s\n", band2str(evt->status.band));
    printf("Channel: %lu\n", evt->status.channel);
    printf("Security: %s %s\n",
           wifi_wpa3_enterprise_txt(evt->status.wpa3_ent_type),
           security2str(evt->status.security));
    printf("MFP: %s\n", wifi_mfp_txt(evt->status.mfp));
    if (evt->status.iface_mode == RCP_WIFI_MODE_INFRA) {
      printf("RSSI: %ld\n", evt->status.rssi);
    }
    printf("Beacon Interval: %lu\n", evt->status.beacon_interval);
    printf("DTIM: %lu\n", evt->status.dtim_period);
    printf("TWT: %s\n",
           evt->status.twt_capable ? "Supported" : "Not supported");
    printf("Current PHY TX rate (Mbps) : %lu\n",
           (evt->status.tx_bitrate_kbps / 1000U));
  }

  display_ipv4_config();
}

static void display_scan_result(struct rcp_wifi_evt_scan_result *result) {
  char ssid_str[WIFI_MAX_SSID_SIZE] = {};
  char bssid_str[20];
  bssid2str(bssid_str, result->bssid);
  memcpy(ssid_str, result->ssid.bytes, result->ssid.size);
  printf("SSID: %-32s    rssi %3d   sec: %s\n", ssid_str, result->rssi,
         security2str(result->sec_type));
  printf("   BSSID: %s   Band: %-7s    Channel: %d\n", bssid_str,
         band2str(result->band), result->channel);
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

/* Initialize shell */
bool shell_commands_init(void) {
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
void shell_task_start(void) {
  if (shell_rx_stream == NULL) {
    shell_rx_stream = xStreamBufferCreateStatic(sizeof(shell_rx_stream_storage),
                                                1, shell_rx_stream_storage,
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

static void shell_uart_rx_start(void) {
  (void)HAL_UARTEx_ReceiveToIdle_IT(&huart3, shell_uart_rx_chunk,
                                    sizeof(shell_uart_rx_chunk));
}

static void shell_handle_char(uint8_t ch) {
  if (ch == '\n' && shell_last_was_cr) {
    shell_last_was_cr = false;
    return;
  }

  shell_last_was_cr = (ch == '\r');

	if (ch == '\b' || ch == 127U) {
		if (shell_idx > 0) {
			shell_idx--;
			printf(" \b");

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

		return;
	}

  if (shell_idx < (SHELL_BUF_SIZE - 1U)) {
    shell_buffer[shell_idx++] = (char)ch;
  }
}

/* Shell task implementation */
static void shell_task(void *argument) {
  uint8_t rx_buf[SHELL_UART_RX_CHUNK_SIZE];
  size_t rx_len;
  size_t i;

  (void)argument;

	printf("rcp> ");


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

static void rcp_test_task(void *argument) {
  rcp_test_ctx_t ctx = *(rcp_test_ctx_t *)argument;

  (void)start_data_test(ctx.mode, ctx.duration_ms);
  rcp_test_task_handle = NULL;
  osThreadExit();
}

/* Process shell command */
static void shell_process_command(char *cmd) {
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
	for (size_t i = 0; i < NUM_COMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			if (commands[i].handler != NULL) {
				commands[i].handler(argc, argv);
			}
			return;
		}
	}

  printf("Unknown command: %s\n", argv[0]);
  printf("Type 'help' for available commands\n");
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
  BaseType_t higher_priority_task_woken = pdFALSE;
  size_t sent;

  if (huart == NULL || huart->Instance != USART3) {
    return;
  }

  if (size > 0U && shell_rx_stream != NULL) {
    sent = xStreamBufferSendFromISR(shell_rx_stream, shell_uart_rx_chunk, size,
                                    &higher_priority_task_woken);
    if (sent < size) {
      shell_rx_dropped += (uint32_t)(size - sent);
    }
  }

  shell_uart_rx_start();
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart == NULL || huart->Instance != USART3) {
    return;
  }

  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                   UART_CLEAR_PEF | UART_CLEAR_FEF |
                                   UART_CLEAR_IDLEF);
  shell_uart_rx_start();
}

/* Help command */
static void cmd_help(int argc, char **argv) {
  printf("\nAvailable commands:\n");
  printf("-------------------\n");
  for (size_t i = 0; i < NUM_COMMANDS; i++) {
    printf("  %-20s - %s\n", commands[i].name, commands[i].help);
  }
  printf("\n");
}

static void cmd_dump(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	printf("\n");
	rcp_dump_status();
	rcp_bus_sdio_master_dump();
	printf("\n");
}

/* ============================================================================
   WiFi Commands
   ============================================================================
 */

static void cmd_wifi_status(int argc, char **argv) {
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
    printf("Iface status request failed, error_code=%ld\n\n",
           (long)iface_status_err);
    return;
  }

  if (!iface_status_valid) {
    printf("Iface status response missing\n\n");
    return;
  }

  display_iface_status(&iface_status_last);
}

static void cmd_wifi_connect(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: wifi_connect <SSID> [PSK] [PMF:0|1|2]\n");
    return;
  }

  struct rcp_wifi_conn_req req = {0};
  strncpy((char *)req.ssid, argv[1], sizeof(req.ssid) - 1);
  req.ssid_len = strlen(argv[1]);
  req.pmf = RCP_WIFI_MFP_OPTIONAL;

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

  if (argc >= 4) {
    char *endptr = NULL;
    long pmf = strtol(argv[3], &endptr, 10);

    if (endptr == argv[3] || *endptr != '\0' ||
        pmf < RCP_WIFI_MFP_DISABLE || pmf > RCP_WIFI_MFP_REQUIRED) {
      printf("Invalid PMF value '%s', expected 0, 1, or 2\n", argv[3]);
      memset(&wifi_conn_info, 0, sizeof(wifi_conn_info));
      return;
    }

    req.pmf = (enum rcp_wifi_mfp_options)pmf;
  }

  printf("Connecting to '%s' (pmf=%d)...\n", argv[1], (int)req.pmf);
  enum rcp_wifi_status status = rcp_wifi_connect(&req);
  if (status == RCP_WIFI_OK) {
    printf("Connection request sent\n");
  } else {
    printf("Connection failed: %d\n", status);
    memset(&wifi_conn_info, 0, sizeof(wifi_conn_info));
  }
}

static void cmd_wifi_disconnect(int argc, char **argv) {
  printf("Disconnecting from WiFi...\n");
  enum rcp_wifi_status status = rcp_wifi_disconnect();
  if (status == RCP_WIFI_OK) {
    printf("Disconnect request sent\n");
  } else {
    printf("Disconnect failed: %d\n", status);
  }
}

static void cmd_wifi_scan(int argc, char **argv) {
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

static bool iperf_parse_server_ip(const char *ip_str) {
  ip_addr_t server_ip;

  if (ipaddr_aton(ip_str, &server_ip) == 0) {
    printf("Failed to parse IP address: %s\n", ip_str);
    return false;
  }

  return true;
}

static void iperf_warn_extra_args(int argc, int expected_argc) {
  if (argc > expected_argc) {
    printf("Using default port 5001, extra arguments are ignored\n");
  }
}

static void cmd_iperf_tcp_tx(int argc, char **argv) {
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

static void cmd_iperf_tcp_rx(int argc, char **argv) {
  iperf_warn_extra_args(argc, 1);
  iperf_start(0, 0, "0.0.0.0");
  printf("Started iperf TCP RX on port 5001\n");
}

static void cmd_iperf_udp_tx(int argc, char **argv) {
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

static void cmd_iperf_udp_rx(int argc, char **argv) {
  iperf_warn_extra_args(argc, 1);
  iperf_start(0, 1, "0.0.0.0");
  printf("Started iperf UDP RX on port 5001\n");
}

static void cmd_iperf_stop(int argc, char **argv) {
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

static int ota_recv_exact(int sock, uint8_t *buf, uint32_t len) {
  uint32_t total = 0;

  while (total < len) {
    int ret = recv(sock, buf + total, len - total, 0);
    if (ret <= 0) {
      return ret;
    }
    total += (uint32_t)ret;
  }

  return (int)total;
}

static void ota_dump_sha256(const uint8_t sha256[32]) {
  for (uint32_t i = 0; i < 32; i++) {
    printf("%02X", sha256[i]);
  }
  printf("\r\n");
}

static int ota_parse_header(const ota_header_t *ota_header, uint32_t *body_len,
                            uint8_t *use_xz) {
  char str[33];

  if ((ota_header == NULL) || (body_len == NULL) || (use_xz == NULL)) {
    return -1;
  }

  memcpy(str, ota_header->u.s.header, sizeof(ota_header->u.s.header));
  str[sizeof(ota_header->u.s.header)] = '\0';
  printf("[OTA] [HEADER] ota header is %s\r\n", str);
  if (strstr(str, "QCC74X_OTA") == NULL) {
    return -1;
  }

  memcpy(str, ota_header->u.s.type, sizeof(ota_header->u.s.type));
  str[sizeof(ota_header->u.s.type)] = '\0';
  printf("[OTA] [HEADER] file type is %s\r\n", str);
  *use_xz = (strstr(str, "XZ") != NULL) ? 1U : 0U;

  memcpy(body_len, &ota_header->u.s.len, sizeof(*body_len));
  printf("[OTA] [HEADER] file length (exclude ota header) is %lu\r\n",
         (unsigned long)*body_len);

  memcpy(str, ota_header->u.s.ver_hardware, sizeof(ota_header->u.s.ver_hardware));
  str[sizeof(ota_header->u.s.ver_hardware)] = '\0';
  printf("[OTA] [HEADER] ver_hardware is %s\r\n", str);

  memcpy(str, ota_header->u.s.ver_software, sizeof(ota_header->u.s.ver_software));
  str[sizeof(ota_header->u.s.ver_software)] = '\0';
  printf("[OTA] [HEADER] ver_software is %s\r\n", str);

  printf("[OTA] [HEADER] sha256 is ");
  ota_dump_sha256(ota_header->u.s.sha256);

  return 0;
}

static void cmd_ota_start(int argc, char **argv) {
  const char *addr;
  const char *port = "3365";
  uint8_t *ota_data = NULL;
  int sock_client = -1;
  uint32_t ota_body_len = 0;
  uint32_t total_sent = 0;
  uint8_t use_xz = 0;
  bool ota_started = false;
  bool ota_finished = false;
  ota_header_t ota_header;
  struct sockaddr_in remote_addr;
  RCP_Object_t *Obj = RCP_ObjGet();

  if (argc < 2) {
    printf("Usage: ota_start <server_ip> [port]\n");
    return;
  }

  addr = argv[1];
  if (argc >= 3) {
    port = argv[2];
  }

  ota_data = pvPortMalloc(OTA_TRANSFER_CHUNK_SIZE);
  if (ota_data == NULL) {
    printf("allocate ota buffer failed\n");
    return;
  }

  memset(&remote_addr, 0, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons((uint16_t)atoi(port));
  remote_addr.sin_addr.s_addr = inet_addr(addr);
  if ((remote_addr.sin_addr.s_addr == INADDR_NONE) &&
      (strcmp(addr, "255.255.255.255") != 0)) {
    printf("invalid server ip: %s\n", addr);
    goto __err_exit;
  }

  if (RCP_FWU_starts(Obj, 1) != RCP_STATUS_OK) {
    printf("RCP_OTASTART failed\n");
    goto __err_exit;
  }
  ota_started = true;

  printf("tcp client start ...\r\n");
  printf("Server ip Address : %s:%s\r\n", addr, port);

  /* create socket */
  if ((sock_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    printf("TCP Client create socket error\r\n");
    goto __err_exit;
  }

  /* connect socket */
  if (connect(sock_client, (struct sockaddr *)&remote_addr,
              sizeof(struct sockaddr)) != 0) {
    printf("TCP client connect server falied!\r\n");
    goto __err_exit;
  }
  printf("TCP client connect server success!\r\n");

  printf("[OTA] receive first %u-byte OTA header...\r\n",
         (unsigned int)OTA_HEADER_PACKET_SIZE);

  //if (send(sock_client, "start", strlen("OTA start"), 0) <= 0) {
  //  printf("send OTA start signal failed\r\n");
  //  goto __err_exit;
  //}


  if (ota_recv_exact(sock_client, ota_header.u.raw, OTA_HEADER_PACKET_SIZE) <= 0) {
    printf("recv ota header from server failed\r\n");
    goto __err_exit;
  }

  if (ota_parse_header(&ota_header, &ota_body_len, &use_xz) != 0) {
    printf("[OTA] invalid OTA header\r\n");
    goto __err_exit;
  }

  if (RCP_FWU_Send(Obj, ota_header.u.raw, OTA_HEADER_PACKET_SIZE) !=
      RCP_STATUS_OK) {
    printf("send ota header failed\r\n");
    goto __err_exit;
  }

  printf("[OTA] header sent, body size=%lu, type=%s\r\n",
         (unsigned long)ota_body_len, use_xz ? "XZ" : "RAW");

  while (total_sent < ota_body_len) {
    uint32_t to_recv = ota_body_len - total_sent;
    if (to_recv > OTA_TRANSFER_CHUNK_SIZE) {
      to_recv = OTA_TRANSFER_CHUNK_SIZE;
    }

    //if (send(sock_client, "start", strlen("OTA start"), 0) <= 0) {
    //  printf("send OTA start signal failed\r\n");
    //  goto __err_exit;
    //}

    int recv_len = recv(sock_client, ota_data, to_recv, 0);
    if (recv_len <= 0) {
      printf("recv ota data from server failed, total=%lu/%lu, ret=%d, errno=%d\r\n",
             (unsigned long)total_sent, (unsigned long)ota_body_len, recv_len, errno);
      goto __err_exit;
    }

    if (RCP_FWU_Send(Obj, ota_data, (uint32_t)recv_len) != RCP_STATUS_OK) {
      printf("send ota data failed\r\n");
      goto __err_exit;
    }

    total_sent += (uint32_t)recv_len;
    printf("[OTA] progress: %lu/%lu bytes\r\n", (unsigned long)total_sent,
           (unsigned long)ota_body_len);
  }

  if (total_sent < ota_body_len) {
    printf("[OTA] incomplete transfer: %lu/%lu bytes\r\n",
           (unsigned long)total_sent, (unsigned long)ota_body_len);
    goto __err_exit;
  }

  if (RCP_FWU_Finish(Obj) != RCP_STATUS_OK) {
    printf("ota check failed\n");
    goto __err_exit;
  }
  ota_finished = true;
  printf("[OTA] OTA transfer finished, module will reboot if validation passes\r\n");

__err_exit:
  if (sock_client >= 0) {
    closesocket(sock_client);
  }
  if (ota_data != NULL) {
    vPortFree(ota_data);
  }
  if (ota_started && !ota_finished) {
    (void)RCP_FWU_starts(Obj, 0);
  }
  return;
}

static void cmd_send_RCP(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_SendAT(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("send AT cmd failed\n");
  }
}

static void cmd_send_RCP_RST(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_Reset(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("send AT cmd failed\n");
  }
}

static void cmd_send_RCP_SYSRAM(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint32_t RemainingHeap = 0;
  uint32_t LwipHeap = 0;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetNcpHeapState(Obj, &RemainingHeap, &LwipHeap);

  if (ret == RCP_STATUS_OK) {
    printf("RemainingHeap: %d, LwipHeap: %d\n", RemainingHeap, LwipHeap);
  } else {
    printf("send AT cmd failed\n");
  }
}

static void cmd_send_RCP_RESTORE(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_ResetToFactoryDefault(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_RESTORE failed\n");
  } else {
    printf("RCP_RESTORE OK\n");
  }
}

static void cmd_send_RCP_SYSSTORE(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint32_t mode = 0;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetStoreMode(Obj, &mode);
  if (ret == RCP_STATUS_OK) {
    printf("+SYSSTORE:%lu\n", (unsigned long)mode);
  } else {
    printf("RCP_SYSSTORE query failed\n");
  }
}

static void cmd_send_RCP_EFUSE_R(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_EFUSE-R <addr> <nbytes> [reload]\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  uint32_t reload = 1;
  if (nbytes == 0 || nbytes > 256) {
    printf("nbytes must be 1~256\n");
    return;
  }
  if (argc >= 4) {
    reload = (uint32_t)strtoul(argv[3], NULL, 0);
  }
  uint8_t data[256];
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_EFuseRead(Obj, nbytes, argv[2], reload, data);
  if (ret == RCP_STATUS_OK) {
    printf("+EFUSE-R:%lu,", (unsigned long)nbytes);
    for (uint32_t i = 0; i < nbytes; i++) {
      printf("%02x", data[i]);
    }
    printf("\n");
  } else {
    printf("RCP_EFUSE-R failed\n");
  }
}

static void cmd_send_RCP_IORST(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_IORST <pin>\n");
    return;
  }
  uint32_t pin = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_RestoreGPIO(Obj, pin);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_IORST failed\n");
  }
}

static void cmd_send_RCP_OTASTART(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_OTASTART <0|1>\n");
    return;
  }
  uint32_t enable = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FWU_starts(Obj, enable);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_OTASTART failed\n");
  }
}

static void cmd_send_RCP_OTAFIN(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FWU_Finish(Obj);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_OTAFIN failed\n");
  }
}

static void cmd_send_RCP_FS_DEL(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_FS_DEL <filename>\n");
    return;
  }
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FS_DeleteFile(Obj, argv[1]);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_FS_DEL failed\n");
  }
}

static void cmd_send_RCP_FS_CREATE(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_FS_CREATE <filename>\n");
    return;
  }
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FS_CreateFile(Obj, argv[1]);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_FS_CREATE failed\n");
  }
}

static void cmd_send_RCP_FS_WRITE(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: RCP_FS_WRITE <filename> <offset> <data>\n");
    return;
  }

  uint32_t offset = (uint32_t)strtoul(argv[2], NULL, 0);
  uint32_t len = (uint32_t)strlen(argv[3]);
  RCP_Object_t *Obj = RCP_ObjGet();

  if (len == 0U) {
    printf("Error: data must not be empty\n");
    return;
  }

  int ret = RCP_FS_WriteFile(Obj, argv[1], offset, (uint8_t *)argv[3], len);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_FS_WRITE failed\n");
  }
}

static void cmd_send_RCP_FS_READ(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: RCP_FS_READ <filename> <offset> <len>\n");
    return;
  }

  uint32_t offset = (uint32_t)strtoul(argv[2], NULL, 0);
  uint32_t len = (uint32_t)strtoul(argv[3], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  uint8_t *data;

  if (len == 0U) {
    printf("Error: len must be greater than 0\n");
    return;
  }

  data = pvPortMalloc(len + 1U);
  if (data == NULL) {
    printf("allocate read buffer failed\n");
    return;
  }
  memset(data, 0, len + 1U);

  int ret = RCP_FS_ReadFile(Obj, argv[1], offset, data, len);
  if (ret == RCP_STATUS_OK) {
    printf("+FS:READ,%lu,%s\n", (unsigned long)len, (char *)data);
  } else {
    printf("RCP_FS_READ failed\n");
  }

  vPortFree(data);
}

static void cmd_send_RCP_FS_SIZE(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_FS_SIZE <filename>\n");
    return;
  }
  uint32_t size = 0;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FS_GetSizeFile(Obj, argv[1], &size);
  if (ret == RCP_STATUS_OK) {
    printf("+FS:SIZE,%lu\n", (unsigned long)size);
  } else {
    printf("RCP_FS_SIZE failed\n");
  }
}

static void cmd_send_RCP_FS_LS(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_FS_FilesList_t files_list = {0};
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FS_ListFiles(Obj, &files_list);
  if (ret == RCP_STATUS_OK) {
    printf("+FS:LIST\n");
    for (uint32_t i = 0; i < files_list.nb_files; i++) {
      printf("%s\n", files_list.filename[i]);
    }
  } else {
    printf("RCP_FS_LS failed\n");
  }
}

static void cmd_send_RCP_CMD(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetCmdList(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_CMD failed\n");
  }
}

static void cmd_send_RCPE(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCPE <0|1>\n");
    return;
  }
  uint32_t enable = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_SetEcho(Obj, enable);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCPE failed\n");
  }
}

static void cmd_send_RCP_GMAC(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint8_t slot0[6], slot1[6], slot2[6];
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetGMAC(Obj, slot0, slot1, slot2);
  if (ret == RCP_STATUS_OK) {
    printf("+GMACSLOT0:%02x:%02x:%02x:%02x:%02x:%02x\n", slot0[0], slot0[1],
           slot0[2], slot0[3], slot0[4], slot0[5]);
    printf("+GMACSLOT1:%02x:%02x:%02x:%02x:%02x:%02x\n", slot1[0], slot1[1],
           slot1[2], slot1[3], slot1[4], slot1[5]);
    printf("+GMACSLOT2:%02x:%02x:%02x:%02x:%02x:%02x\n", slot2[0], slot2[1],
           slot2[2], slot2[3], slot2[4], slot2[5]);
  } else {
    printf("RCP_GMAC failed\n");
  }
}

static void cmd_send_RCP_GMR(int argc, char **argv) {
  (void)argc;
  (void)argv;

  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetGMR(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_GMR failed\n");
  }
}

static void cmd_send_RCP_PN(int argc, char **argv) {
  (void)argc;
  (void)argv;
  char pn[64] = {0};
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetPN(Obj, pn, sizeof(pn));
  if (ret == RCP_STATUS_OK) {
    printf("+PN:%s\n", pn);
  } else {
    printf("RCP_PN failed\n");
  }
}

static void cmd_send_RCP_VBAT(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint32_t voltage = 0;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetVBAT(Obj, &voltage);
  if (ret == RCP_STATUS_OK) {
    printf("+VBAT:%lu mV\n", (unsigned long)voltage);
  } else {
    printf("RCP_VBAT failed\n");
  }
}

static void cmd_send_RCP_SYSMSG(int argc, char **argv) {
  RCP_Object_t *Obj = RCP_ObjGet();
  if (argc >= 2) {
    uint32_t state = (uint32_t)strtoul(argv[1], NULL, 0);
    int ret = RCP_SetSysMsg(Obj, state);
    if (ret == RCP_STATUS_OK) {
      printf("OK\n");
    } else {
      printf("RCP_SYSMSG set failed\n");
    }
  } else {
    uint32_t state = 0;
    int ret = RCP_GetSysMsg(Obj, &state);
    if (ret == RCP_STATUS_OK) {
      printf("+SYSMSG:%lu\n", (unsigned long)state);
    } else {
      printf("RCP_SYSMSG query failed\n");
    }
  }
}

static void cmd_send_RCP_SYSLOG(int argc, char **argv) {
  RCP_Object_t *Obj = RCP_ObjGet();
  if (argc >= 2) {
    uint32_t state = (uint32_t)strtoul(argv[1], NULL, 0);
    int ret = RCP_SetSysLog(Obj, state);
    if (ret == RCP_STATUS_OK) {
      printf("OK\n");
    } else {
      printf("RCP_SYSLOG set failed\n");
    }
  } else {
    uint32_t state = 0;
    int ret = RCP_GetSysLog(Obj, &state);
    if (ret == RCP_STATUS_OK) {
      printf("+SYSLOG:%lu\n", (unsigned long)state);
    } else {
      printf("RCP_SYSLOG query failed\n");
    }
  }
}

static void cmd_send_RCP_TEMP(int argc, char **argv) {
  (void)argc;
  (void)argv;
  float temp = 0.0f;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetTemp(Obj, &temp);
  if (ret == RCP_STATUS_OK) {
    printf("+TEMP:%f C\n", temp);
  } else {
    printf("RCP_TEMP failed\n");
  }
}

static void cmd_send_RCP_EFUSE_CFM(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_EFuseConfirm(Obj);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_EFUSE-CFM failed\n");
  }
}

static void cmd_send_RCP_FLASH_E(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_FLASH-E <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FlashErase(Obj, nbytes, argv[2]);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_FLASH-E failed\n");
  }
}

static void cmd_send_RCP_IOPUPD(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_IOPUPD <pin> <0|1|2>\n");
    return;
  }
  uint32_t pin = (uint32_t)strtoul(argv[1], NULL, 0);
  uint32_t state = (uint32_t)strtoul(argv[2], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GPIOPullUpDn(Obj, pin, state);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_IOPUPD failed\n");
  }
}

static void cmd_send_RCP_IOOUT(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_IOOUT <pin> <0|1>\n");
    return;
  }
  uint32_t pin = (uint32_t)strtoul(argv[1], NULL, 0);
  uint32_t state = (uint32_t)strtoul(argv[2], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GPIOOutput(Obj, pin, state);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_IOOUT failed\n");
  }
}

static void cmd_send_RCP_IOIN(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_IOIN <pin> [pull_state] | RCP_IOIN <pin> ?\n");
    return;
  }
  uint32_t pin = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();

  if ((argc >= 3) && (strcmp(argv[2], "?") == 0)) {
    uint32_t state = 0;
    int ret = RCP_GPIOInputRead(Obj, pin, &state);
    if (ret == RCP_STATUS_OK) {
      printf("+IOIN=%lu:%lu\n", (unsigned long)pin, (unsigned long)state);
    } else {
      printf("RCP_IOIN query failed\n");
    }
  } else if (argc >= 3) {
    uint32_t pull_state = (uint32_t)strtoul(argv[2], NULL, 0);
    int ret = RCP_GPIOInputConfig(Obj, pin, pull_state);
    if (ret == RCP_STATUS_OK) {
      printf("OK\n");
    } else {
      printf("RCP_IOIN config failed\n");
    }
  } else {
    printf("Usage: RCP_IOIN <pin> [pull_state] | RCP_IOIN <pin> ?\n");
  }
}

static void cmd_send_RCP_SYSSTORE_SET(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: RCP_SYSSTORE_SET <0|1>\n");
    return;
  }
  uint32_t mode = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_SetStoreMode(Obj, mode);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_SYSSTORE_SET failed\n");
  }
}

static void hexstr_to_bytes(const char *hexstr, uint8_t *buf,
                            uint32_t max_len) {
  uint32_t i;
  for (i = 0; (hexstr[0] != '\0') && (hexstr[1] != '\0') && (i < max_len);
       i++) {
    char tmp[3] = {hexstr[0], hexstr[1], '\0'};
    buf[i] = (uint8_t)strtoul(tmp, NULL, 16);
    hexstr += 2;
  }
}

static void cmd_send_RCP_EFUSE_W(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: RCP_EFUSE_W <nbytes> <addr> <hex_data>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  uint8_t data[256];
  if (nbytes > sizeof(data)) {
    printf("Error: nbytes > %zu\n", sizeof(data));
    return;
  }
  hexstr_to_bytes(argv[3], data, nbytes);
  int ret = RCP_EFuseWrite(Obj, nbytes, argv[2], data);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_EFUSE_W failed\n");
  }
}

static void cmd_send_RCP_EFUSE_W_HEX(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: RCP_EFUSE_W_HEX <nbytes> <addr> <hex_string>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_EFuseWriteHex(Obj, nbytes, argv[2], argv[3]);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_EFUSE_W_HEX failed\n");
  }
}

static void cmd_send_RCP_EFUSE_R_HEX(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_EFUSE_R_HEX <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  char hex_data[256];
  if (2U * nbytes + 1U > sizeof(hex_data)) {
    printf("Error: result buffer too small\n");
    return;
  }
  int ret = RCP_EFuseReadHex(Obj, nbytes, argv[2], 1, hex_data);
  if (ret == RCP_STATUS_OK) {
    printf("+EFUSE-R-HEX:%lu,%s\n", (unsigned long)nbytes, hex_data);
  } else {
    printf("RCP_EFUSE_R_HEX failed\n");
  }
}

static void cmd_send_RCP_FLASH_W(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: RCP_FLASH_W <nbytes> <addr> <hex_data>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  uint8_t data[256];
  if (nbytes > sizeof(data)) {
    printf("Error: nbytes > %zu\n", sizeof(data));
    return;
  }
  hexstr_to_bytes(argv[3], data, nbytes);
  int ret = RCP_FlashWrite(Obj, nbytes, argv[2], data);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_FLASH_W failed\n");
  }
}

static void cmd_send_RCP_FLASH_W_HEX(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: RCP_FLASH_W_HEX <nbytes> <addr> <hex_string>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FlashWriteHex(Obj, nbytes, argv[2], argv[3]);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_FLASH_W_HEX failed\n");
  }
}

static void cmd_send_RCP_FLASH_R(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_FLASH_R <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  uint8_t data[256];
  if (nbytes > sizeof(data)) {
    printf("Error: nbytes > %zu\n", sizeof(data));
    return;
  }
  int ret = RCP_FlashRead(Obj, nbytes, argv[2], data);
  if (ret == RCP_STATUS_OK) {
    printf("+FLASH-R:%lu,", (unsigned long)nbytes);
    for (uint32_t i = 0; i < nbytes; i++) {
      printf("%02x", data[i]);
    }
    printf("\nOK\n");
  } else {
    printf("RCP_FLASH_R failed\n");
  }
}

static void cmd_send_RCP_FLASH_R_HEX(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: RCP_FLASH_R_HEX <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = (uint32_t)strtoul(argv[1], NULL, 0);
  RCP_Object_t *Obj = RCP_ObjGet();
  char hex_data[256];
  if (2U * nbytes + 1U > sizeof(hex_data)) {
    printf("Error: result buffer too small\n");
    return;
  }
  int ret = RCP_FlashReadHex(Obj, nbytes, argv[2], hex_data);
  if (ret == RCP_STATUS_OK) {
    printf("+FLASH-R-HEX:%lu,%s\n", (unsigned long)nbytes, hex_data);
  } else {
    printf("RCP_FLASH_R_HEX failed\n");
  }
}

static void cmd_send_RCP_PART(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  RCP_PartInfo_t parts[16] = {0};
  uint32_t nb_parts = (uint32_t)(sizeof(parts) / sizeof(parts[0]));
  int ret = RCP_PartQuery(Obj, parts, &nb_parts);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_PART failed\n");
    return;
  }

  for (uint32_t i = 0; i < nb_parts; i++) {
    printf(
        "PART[%lu]: active=%lu age=%lu name=\"%s\" addr0=0x%08lx addr1=0x%08lx "
        "len0=%lu len1=%lu\n",
        (unsigned long)i, (unsigned long)parts[i].active_idx,
        (unsigned long)parts[i].age, parts[i].name,
        (unsigned long)parts[i].addr0, (unsigned long)parts[i].addr1,
        (unsigned long)parts[i].len0, (unsigned long)parts[i].len1);
  }
}

static void cmd_send_RCP_MINIDUMP(int argc, char **argv) {
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  uint8_t *dump_data;
  uint32_t dump_len = MINIDUMP_CACHE_SIZE;
  uint32_t i;
  int ret;

  dump_data = (uint8_t *)pvPortMalloc(MINIDUMP_CACHE_SIZE);
  if (dump_data == NULL) {
    printf("RCP_MINIDUMP alloc failed\n");
    return;
  }

  printf("--- Minidump Output (%lu bytes max) ---\n",
         (unsigned long)MINIDUMP_CACHE_SIZE);
  ret = RCP_Minidump(Obj, dump_data, &dump_len);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_MINIDUMP failed, dump_len=%lu\n", (unsigned long)dump_len);
    vPortFree(dump_data);
    printf("--- End of Minidump ---\n");
    return;
  }

  printf("Minidump length: %lu bytes\n", (unsigned long)dump_len);
  for (i = 0U; i < dump_len; i++) {
    if ((i % 16U) == 0U) {
      printf("%08lX: ", (unsigned long)i);
    }
    printf("%02X ", dump_data[i]);
    if (((i + 1U) % 16U) == 0U) {
      printf("\n");
    }
  }
  if ((dump_len % 16U) != 0U) {
    printf("\n");
  }
  vPortFree(dump_data);
  printf("--- End of Minidump ---\n");
}

static void cmd_send_RCP_PWR(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint32_t ps_mode = 0, hbn_level = 0;
  int ret;

  RCP_Object_t *Obj = RCP_ObjGet();

  if (argc < 2) {
	  return;
  }
  ps_mode = (uint32_t)strtoul(argv[1], NULL, 0);
  if (argc >= 3) {
	  hbn_level = (uint32_t)strtoul(argv[2], NULL, 0);
  }
  ret = RCP_SetPowerMode(Obj, ps_mode, hbn_level);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_PWR failed\n");
    return;
  }
}

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
    printf("  duration_s : TX test duration in s (default: %lu)\n",
           (unsigned long)RCP_DATA_TEST_DEFAULT_DURATION_S);
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
      printf("Invalid duration, using default %lu s\n",
             (unsigned long)RCP_DATA_TEST_DEFAULT_DURATION_S);
      duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;
    }
  }
  if ((mode == RCP_DATA_TEST_MODE_RX) && (argc >= 3)) {
    printf("RX mode ignores duration and runs until stop command\n");
  }

  g_rcp_test_ctx.mode = mode;
  g_rcp_test_ctx.duration_ms = duration_s * 1000U;
  rcp_test_task_handle =
      osThreadNew(rcp_test_task, &g_rcp_test_ctx, &rcp_test_task_attr);
  if (rcp_test_task_handle == NULL) {
    printf("Failed to start RCP data test task\n");
  }
}
/* HCI RX callback - prints received data directly */
static void hci_rx_callback(const uint8_t *data, uint16_t len)
{
    printf("\n[HCI RX] Type: 0x%02X, len=%u\n", data[0], len);
    printf("Data: ");
    for (int i = 0; i < len && i < 32; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 32) {
        printf("...");
    }
	printf("\n");
}

static void hci_cli_init(void)
{
    if (!hci_initialized) {
        printf("Initializing HCI...\n");
        if (bt_hci_init(hci_rx_callback)) {
            printf("✓ HCI initialized successfully\n");
            hci_initialized = true;
        } else {
            printf("✗ HCI initialization failed\n");
        }
    } else {
        printf("HCI already initialized\n");
    }
}

/**
 * @brief CLI command: Send HCI Reset
 * Usage: hci_reset
 */
static void cmd_hci_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== HCI Reset Command =====\n");

    hci_cli_init();

    /* HCI Reset: H4_TYPE(0x01) + OCF(0x03) + OGF(0x0C) + ParamLen(0x00) */
    uint8_t hci_reset_cmd[] = {0x01, 0x03, 0x0C, 0x00};

    printf("Sending: 01 03 0C 00\n");

    /* Send command */
    if (!bt_hci_send(hci_reset_cmd, sizeof(hci_reset_cmd))) {
        printf("Failed to send HCI Reset\n");
        return;
    }

}

/**
 * @brief CLI command: Send HCI Set Event Mask
 * Usage: hci_set_event_mask
 */
static void cmd_hci_set_event_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== HCI Set Event Mask =====\n");

    hci_cli_init();

    /* Set Event Mask: H4_TYPE + OCF + OGF + ParamLen + Mask[8] */
    uint8_t set_event_mask_cmd[] = {
        0x01,              /* H4 Command */
        0x01, 0x0C,        /* Opcode: 0x0C01 */
        0x08,              /* Parameter length */
        0xFD, 0x8C, 0xE2, 0x02, 0x06, 0xC8, 0x2F, 0x24  /* Event mask */
    };

    printf("Sending: 01 01 0C 08 FD 8C E2 02 06 C8 2F 24\n");

    if (!bt_hci_send(set_event_mask_cmd, sizeof(set_event_mask_cmd))) {
        printf("Failed to send Set Event Mask\n");
        return;
    }

}

/**
 * @brief CLI command: Send LE Set Event Mask
 * Usage: hci_le_set_event_mask
 */
static void cmd_hci_le_set_event_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== LE Set Event Mask =====\n");

    hci_cli_init();

    /* LE Set Event Mask: H4_TYPE + OCF + OGF + ParamLen + Mask[8] */
    uint8_t le_set_event_mask_cmd[] = {
        0x01,              /* H4 Command */
        0x01, 0x20,        /* Opcode: 0x2001 */
        0x08,              /* Parameter length */
        0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00  /* LE event mask */
    };

    printf("Sending: 01 01 20 08 FF FF FF FF 03 00 00 00\n");

    if (!bt_hci_send(le_set_event_mask_cmd, sizeof(le_set_event_mask_cmd))) {
        printf("Failed to send LE Set Event Mask\n");
        return;
    }

}

/**
 * @brief CLI command: Read Local Version
 * Usage: hci_read_version
 */
static void cmd_hci_read_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== Read Local Version =====\n");

    hci_cli_init();

    /* Read Local Version Info: H4_TYPE + OCF + OGF + ParamLen */
    uint8_t read_version_cmd[] = {0x01, 0x01, 0x10, 0x00};

    printf("Sending: 01 01 10 00\n");

    if (!bt_hci_send(read_version_cmd, sizeof(read_version_cmd))) {
        printf("Failed to send Read Local Version\n");
        return;
    }

}

/**
 * @brief CLI command: Send raw HCI data
 * Usage: hci_send <hex_bytes>
 * Example: hci_send 01 03 0C 00
 */
static void cmd_hci_send(int argc, char **argv)
{
    uint8_t hci_data[256];

    if (argc < 2) {
        printf("Usage: hci_send <hex_bytes>\n");
        printf("Examples:\n");
        printf("  hci_send 01 03 0C 00           (HCI Reset)\n");
        printf("  hci_send 01 01 0C 08 FD 8C...  (Set Event Mask)\n");
        printf("  hci_send 02 00 01 00 0C        (ACL Data)\n");
        printf("H4 types: 01=CMD, 02=ACL, 03=SCO, 04=EVT\n");
        return;
    }

    printf("\n===== Send Raw HCI =====\n");

    hci_cli_init();

    /* Parse hex bytes */
    uint8_t total_len = 0;
    for (int i = 1; i < argc && total_len < sizeof(hci_data); i++) {
        unsigned int byte_val;
        if (sscanf(argv[i], "%02x", &byte_val) == 1 || sscanf(argv[i], "%02X", &byte_val) == 1) {
            hci_data[total_len++] = (uint8_t)byte_val;
        } else {
            printf("Invalid hex value: %s\n", argv[i]);
            return;
        }
    }

    if (total_len < 1) {
        printf("Error: No data provided\n");
        return;
    }

    /* Show H4 packet type */
    const char *type_str = "Unknown";
    switch (hci_data[0]) {
        case 0x01: type_str = "HCI Command"; break;
        case 0x02: type_str = "ACL Data"; break;
        case 0x03: type_str = "SCO Data"; break;
        case 0x04: type_str = "HCI Event"; break;
    }
    printf("Sending %s (%u bytes)\n", type_str, total_len);

    /* Send data */
    printf("TX: ");
    for (int i = 0; i < total_len && i < 32; i++) {
        printf("%02X ", hci_data[i]);
    }
    if (total_len > 32) {
        printf("...");
    }
    printf("\n");

    if (!bt_hci_send(hci_data, total_len)) {
        printf("Failed to send HCI data\n");
        return;
    }

}

static void cmd_bt_adv_start(int argc, char **argv)
{
    uint8_t dev_name[32];
    uint8_t dev_name_len;

    if (argc >= 2) {
        /* bt_adv_start <name> */
        dev_name[0] = 0x09; /* AD_TYPE_COMPLETE_LOCAL_NAME */
        size_t name_len = strlen(argv[1]);
        if (name_len > sizeof(dev_name) - 1)
            name_len = sizeof(dev_name) - 1;
        memcpy(dev_name + 1, argv[1], name_len);
        dev_name_len = (uint8_t)(1 + name_len);
    } else {
        /* default name */
        const uint8_t default_name[] = {0x09, 'Q','C','C','7','4','x','-','R','C','P'};
        memcpy(dev_name, default_name, sizeof(default_name));
        dev_name_len = sizeof(default_name);
    }

    if (BleAppAdvStart(0x00 /*ADV_IND*/, 0x0020, 0x0020, dev_name, dev_name_len)) {
        printf("BLE advertising started\n");
    } else {
        printf("Failed to start BLE advertising\n");
    }
}

static void cmd_bt_adv_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (BleAppAdvStop()) {
        printf("BLE advertising stopped\n");
    } else {
        printf("Failed to stop BLE advertising\n");
    }
}

static void cmd_bt_add_service(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (BleAppAddService()) {
        printf("GATT service added\n");
    } else {
        printf("Failed to add GATT service\n");
    }
}
