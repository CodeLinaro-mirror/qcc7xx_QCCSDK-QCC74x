/**
 * @file rcp_unit_test.c
 * @brief Shell commands for direct RCP API unit tests
 */

#include "rcp_unit_test.h"

#include "FreeRTOS.h"
#include "sockets.h"
#include "rcp_api.h"
#include "task.h"
#include <lwip/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <wifi/rcp_wifi_mgmt.h>

#define MINIDUMP_CACHE_SIZE 1024U
#define OTA_HEADER_PACKET_SIZE 512U
#define OTA_TRANSFER_CHUNK_SIZE 1024U

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

static void cmd_send_RCP(int argc, char **argv);
static void cmd_send_RCP_RST(int argc, char **argv);
static void cmd_send_RCP_SYSRAM(int argc, char **argv);
static void cmd_send_RCP_RESTORE(int argc, char **argv);
static void cmd_send_RCP_SYSSTORE(int argc, char **argv);
static void cmd_send_RCP_EFUSE_R(int argc, char **argv);
static void cmd_send_RCP_IORST(int argc, char **argv);
static void cmd_send_RCP_OTASTART(int argc, char **argv);
static void cmd_send_RCP_OTAFIN(int argc, char **argv);
static void cmd_send_RCP_FS_DEL(int argc, char **argv);
static void cmd_send_RCP_FS_CREATE(int argc, char **argv);
static void cmd_send_RCP_FS_SIZE(int argc, char **argv);
static void cmd_send_RCP_FS_LS(int argc, char **argv);
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
static void cmd_send_RCP_EFUSE_R_HEX(int argc, char **argv);
static void cmd_send_RCP_FLASH_W(int argc, char **argv);
static void cmd_send_RCP_FLASH_W_HEX(int argc, char **argv);
static void cmd_send_RCP_FLASH_R(int argc, char **argv);
static void cmd_send_RCP_FLASH_R_HEX(int argc, char **argv);
static void cmd_send_RCP_PART(int argc, char **argv);
static void cmd_send_RCP_MINIDUMP(int argc, char **argv);
static void cmd_send_RCP_PWR(int argc, char **argv);
static void cmd_send_RCP_SetWakeUpPin(int argc, char **argv);
static void cmd_wifi_connect(int argc, char **argv);
static void cmd_wifi_disconnect(int argc, char **argv);
static void cmd_ota_start(int argc, char **argv);
static int ota_recv_exact(int sock, uint8_t *buf, uint32_t len);
static int ota_parse_header(const ota_header_t *ota_header, uint32_t *body_len,
                            uint8_t *use_xz);
static void ota_dump_sha256(const uint8_t sha256[32]);
static void cmd_wifi_sta_start(int argc, char **argv);
static void cmd_wifi_auto_conn_set(int argc, char **argv);
static void cmd_wifi_auto_conn_get(int argc, char **argv);
static void cmd_wifi_scan(int argc, char **argv);
static void cmd_wifi_set_scan_opts(int argc, char **argv);
static void cmd_wifi_scan_start(int argc, char **argv);
static void cmd_wifi_reconn_set(int argc, char **argv);
static void cmd_wifi_connect_opts(int argc, char **argv);
static void cmd_wifi_disconnect_restore(int argc, char **argv);
static void cmd_wifi_connect_info(int argc, char **argv);
static void cmd_wifi_sta_mac(int argc, char **argv);
static void cmd_wifi_set_mac(int argc, char **argv);
static void cmd_wifi_get_mac(int argc, char **argv);
static void cmd_wifi_country_get(int argc, char **argv);
static void cmd_wifi_country_set(int argc, char **argv);
static void cmd_wifi_dual_mode(int argc, char **argv);
static void cmd_wifi_ap_start(int argc, char **argv);
static void cmd_wifi_ap_stop(int argc, char **argv);
static void cmd_wifi_ap_config(int argc, char **argv);
static void cmd_wifi_ap_list_sta(int argc, char **argv);
static void cmd_wifi_ap_disconnect_sta(int argc, char **argv);
static void cmd_wifi_ap_mode_get(int argc, char **argv);
static void cmd_wifi_ap_mode_set(int argc, char **argv);
static void cmd_wifi_ap_mac(int argc, char **argv);
static void cmd_wifi_l2_ifaces(int argc, char **argv);
static void cmd_wifi_iface_status(int argc, char **argv);
static void cmd_wifi_pwr(int argc, char **argv);
static void cmd_wifi_get_pwr(int argc, char **argv);
static void cmd_wifi_wkio(int argc, char **argv);
static void cmd_wifi_dewkio(int argc, char **argv);
static void cmd_wifi_wktimer(int argc, char **argv);
static void cmd_wifi_set_dtim(int argc, char **argv);
static void cmd_wifi_clr_dtim(int argc, char **argv);
static void cmd_wifi_get_dtim(int argc, char **argv);
static void cmd_wifi_keepalive_start(int argc, char **argv);
static void cmd_wifi_keepalive_stop(int argc, char **argv);
static void cmd_wifi_arp_start(int argc, char **argv);
static void cmd_wifi_arp_stop(int argc, char **argv);
static void cmd_wifi_clk_set(int argc, char **argv);
static void cmd_wifi_clk_get(int argc, char **argv);
static void cmd_wifi_twt_setup(int argc, char **argv);
static void cmd_wifi_twt_sleep(int argc, char **argv);
static void cmd_wifi_twt_status(int argc, char **argv);
static void cmd_wifi_twt_supported(int argc, char **argv);
static void cmd_wifi_twt_teardown(int argc, char **argv);
static void cmd_wifi_pwr_clear(int argc, char **argv);
static void cmd_wifi_pwr_get(int argc, char **argv);
static void cmd_wifi_lp_interval(int argc, char **argv);
static void cmd_wifi_coex_duty_set(int argc, char **argv);
static void cmd_wifi_coex_duty_get(int argc, char **argv);
static void cmd_wifi_coex_en_set(int argc, char **argv);
static void cmd_wifi_coex_en_get(int argc, char **argv);
static void cmd_wifi_ant_set(int argc, char **argv);
static void cmd_wifi_ant_get(int argc, char **argv);
static void cmd_wifi_ant_used(int argc, char **argv);

const shell_cmd_t rcp_unit_test_commands[] = {
    {RCP_MCU_CAP_ANY, "RCP", cmd_send_RCP, "send basic AT probe to the module"},
    {RCP_MCU_CAP_ANY, "RCP_RST", cmd_send_RCP_RST, "reset module: RCP_RST"},
    {RCP_MCU_CAP_ANY, "RCP_SYSRAM", cmd_send_RCP_SYSRAM, "send RCP_SYSRAM to the module"},
    {RCP_MCU_CAP_ANY, "RCP_RESTORE", cmd_send_RCP_RESTORE, "factory reset module: RCP_RESTORE"},
    {RCP_MCU_CAP_ANY, "RCP_SYSSTORE", cmd_send_RCP_SYSSTORE, "query SYSSTORE mode"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE-R", cmd_send_RCP_EFUSE_R, "read EFUSE: RCP_EFUSE-R <nbytes> <addr> [reload]"},
    {RCP_MCU_CAP_ANY, "RCP_IORST", cmd_send_RCP_IORST, "reset GPIO: RCP_IORST <pin>"},
    {RCP_MCU_CAP_ANY, "RCP_OTASTART", cmd_send_RCP_OTASTART, "start/stop OTA mode: RCP_OTASTART <0|1>"},
    {RCP_MCU_CAP_ANY, "RCP_OTAFIN", cmd_send_RCP_OTAFIN, "finish OTA mode: RCP_OTAFIN"},
    {RCP_MCU_CAP_ANY, "RCP_FS_DEL", cmd_send_RCP_FS_DEL, "delete file: RCP_FS_DEL <filename>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_CREATE", cmd_send_RCP_FS_CREATE, "create file: RCP_FS_CREATE <filename>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_WRITE", cmd_send_RCP_FS_WRITE, "write file data: RCP_FS_WRITE <filename> <offset> <data>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_READ", cmd_send_RCP_FS_READ, "read file data: RCP_FS_READ <filename> <offset> <len>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_SIZE", cmd_send_RCP_FS_SIZE, "get file size: RCP_FS_SIZE <filename>"},
    {RCP_MCU_CAP_ANY, "RCP_FS_LS", cmd_send_RCP_FS_LS, "list files in filesystem"},
    {RCP_MCU_CAP_ANY, "RCP_CMD", cmd_send_RCP_CMD, "query supported AT command list: RCP_CMD"},
    {RCP_MCU_CAP_ANY, "RCPE", cmd_send_RCPE, "set echo mode: RCPE <0|1>"},
    {RCP_MCU_CAP_ANY, "RCP_GMAC", cmd_send_RCP_GMAC, "read 3 MAC slots: RCP_GMAC"},
    {RCP_MCU_CAP_ANY, "RCP_GMR", cmd_send_RCP_GMR, "query module version info: RCP_GMR"},
    {RCP_MCU_CAP_ANY, "RCP_PN", cmd_send_RCP_PN, "read part number: RCP_PN"},
    {RCP_MCU_CAP_ANY, "RCP_VBAT", cmd_send_RCP_VBAT, "read supply voltage: RCP_VBAT"},
    {RCP_MCU_CAP_ANY, "RCP_SYSMSG", cmd_send_RCP_SYSMSG, "query/set system message: RCP_SYSMSG [state]"},
    {RCP_MCU_CAP_ANY, "RCP_SYSLOG", cmd_send_RCP_SYSLOG, "query/set error code output: RCP_SYSLOG [0|1]"},
    {RCP_MCU_CAP_ANY, "RCP_TEMP", cmd_send_RCP_TEMP, "read chip temperature: RCP_TEMP"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE-CFM", cmd_send_RCP_EFUSE_CFM, "confirm eFuse write: RCP_EFUSE-CFM"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH-E", cmd_send_RCP_FLASH_E, "erase flash: RCP_FLASH-E <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_IOPUPD", cmd_send_RCP_IOPUPD, "GPIO pull config: RCP_IOPUPD <pin> <0|1|2>"},
    {RCP_MCU_CAP_ANY, "RCP_IOOUT", cmd_send_RCP_IOOUT, "GPIO output level: RCP_IOOUT <pin> <0|1>"},
    {RCP_MCU_CAP_ANY, "RCP_IOIN", cmd_send_RCP_IOIN, "GPIO input: RCP_IOIN <pin> [pull]|?"},
    {RCP_MCU_CAP_ANY, "RCP_SYSSTORE_SET", cmd_send_RCP_SYSSTORE_SET, "set store mode: RCP_SYSSTORE_SET <0|1>"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_W", cmd_send_RCP_EFUSE_W, "write eFuse raw: RCP_EFUSE_W <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_W_HEX", cmd_send_RCP_EFUSE_W_HEX, "write eFuse hex: RCP_EFUSE_W_HEX <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_R", cmd_send_RCP_EFUSE_R, "read eFuse raw: RCP_EFUSE_R <nbytes> <addr> [reload]"},
    {RCP_MCU_CAP_ANY, "RCP_EFUSE_R_HEX", cmd_send_RCP_EFUSE_R_HEX, "read eFuse hex: RCP_EFUSE_R_HEX <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_W", cmd_send_RCP_FLASH_W, "write flash raw: RCP_FLASH_W <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_W_HEX", cmd_send_RCP_FLASH_W_HEX, "write flash hex: RCP_FLASH_W_HEX <nbytes> <addr> <hex>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_R", cmd_send_RCP_FLASH_R, "read flash raw: RCP_FLASH_R <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_FLASH_R_HEX", cmd_send_RCP_FLASH_R_HEX, "read flash hex: RCP_FLASH_R_HEX <nbytes> <addr>"},
    {RCP_MCU_CAP_ANY, "RCP_PART", cmd_send_RCP_PART, "query partition table: RCP_PART"},
    {RCP_MCU_CAP_ANY, "RCP_MINIDUMP", cmd_send_RCP_MINIDUMP, "RCP_MINIDUMP"},
    {RCP_MCU_CAP_ANY, "RCP_PWR", cmd_send_RCP_PWR, "RCP_PWR <ps_mode> [hbn_level]"},
    {RCP_MCU_CAP_ANY, "RCP_SetWakeUpPin", cmd_send_RCP_SetWakeUpPin, "RCP_SetWakeUpPin <pin>"},
    {RCP_MCU_CAP_WIFI, "wifi_connect", cmd_wifi_connect, "Connect to WiFi: wifi_connect <SSID> [PSK] [PMF:0|1|2]"},
    {RCP_MCU_CAP_WIFI, "wifi_disconnect", cmd_wifi_disconnect, "Disconnect from WiFi"},
    {RCP_MCU_CAP_ANY, "ota_start", cmd_ota_start, "ota_start <server_ip> [port], default port is 3365"},
    {RCP_MCU_CAP_WIFI, "WIFI_STA_START", cmd_wifi_sta_start, "rcp_wifi_station_start"},
    {RCP_MCU_CAP_WIFI, "WIFI_AUTO_SET", cmd_wifi_auto_conn_set, "RCP_WIFI_AUTO_SET <0|1>"},
    {RCP_MCU_CAP_WIFI, "WIFI_AUTO_GET", cmd_wifi_auto_conn_get, "rcp_wifi_get_auto_connect"},
    {RCP_MCU_CAP_WIFI, "WIFI_SCAN", cmd_wifi_scan, "RCP_WIFI_SCAN [active|passive] [channel] [max_cnt] [ssid]"},
    {RCP_MCU_CAP_WIFI, "WIFI_SET_SCAN_OPTS", cmd_wifi_set_scan_opts, "RCP_WIFI_SET_SCAN_OPTS [active|passive] [channel] [max_cnt] [ssid] [mac]"},
    {RCP_MCU_CAP_WIFI, "WIFI_SCAN_START", cmd_wifi_scan_start, "rcp_wifi_scan_start"},
    {RCP_MCU_CAP_WIFI, "WIFI_RECONN_SET", cmd_wifi_reconn_set, "RCP_WIFI_RECONN_SET <interval_s> <attempts>"},
    {RCP_MCU_CAP_WIFI, "WIFI_CONNECT_OPTS", cmd_wifi_connect_opts, "RCP_WIFI_CONNECT_OPTS <ssid> [psk] [pmf] [wep] [wps] [interval_s] [attempts] [bssid]"},
    {RCP_MCU_CAP_WIFI, "WIFI_DISCONN_RESTORE", cmd_wifi_disconnect_restore, "RCP_WIFI_DISCONN_RESTORE <0|1>"},
    {RCP_MCU_CAP_WIFI, "WIFI_CONN_INFO", cmd_wifi_connect_info, "rcp_wifi_get_connect_info"},
    {RCP_MCU_CAP_WIFI, "WIFI_STA_MAC", cmd_wifi_sta_mac, "rcp_wifi_station_get_mac_address"},
    {RCP_MCU_CAP_WIFI, "WIFI_SET_MAC", cmd_wifi_set_mac, "RCP_WIFI_SET_MAC <mac>"},
    {RCP_MCU_CAP_WIFI, "WIFI_GET_MAC", cmd_wifi_get_mac, "RCP_WIFI_GET_MAC [timeout_ms]"},
    {RCP_MCU_CAP_WIFI, "WIFI_COUNTRY_GET", cmd_wifi_country_get, "rcp_wifi_get_country_code"},
    {RCP_MCU_CAP_WIFI, "WIFI_COUNTRY_SET", cmd_wifi_country_set, "RCP_WIFI_COUNTRY_SET <policy> <CN|JP|US|EU|00>"},
    {RCP_MCU_CAP_WIFI, "WIFI_DUAL_MODE", cmd_wifi_dual_mode, "rcp_wifi_set_dual_mode"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_START", cmd_wifi_ap_start, "RCP_WIFI_AP_START <ssid> <psk> <channel> <security> [max_conn] [hidden] [protocol]"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_STOP", cmd_wifi_ap_stop, "RCP_WIFI_AP_STOP [reconnect]"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_CONFIG", cmd_wifi_ap_config, "rcp_wifi_ap_get_config"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_LIST_STA", cmd_wifi_ap_list_sta, "rcp_wifi_ap_list_connected_stations"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_DISCONN_STA", cmd_wifi_ap_disconnect_sta, "RCP_WIFI_AP_DISCONN_STA <mac>"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_MODE_GET", cmd_wifi_ap_mode_get, "rcp_wifi_ap_get_mode"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_MODE_SET", cmd_wifi_ap_mode_set, "RCP_WIFI_AP_MODE_SET <protocol>"},
    {RCP_MCU_CAP_WIFI, "WIFI_AP_MAC", cmd_wifi_ap_mac, "rcp_wifi_ap_get_mac_address"},
    {RCP_MCU_CAP_WIFI, "WIFI_L2_IFACES", cmd_wifi_l2_ifaces, "RCP_WIFI_L2_IFACES [net_if_type] [index]"},
    {RCP_MCU_CAP_WIFI, "WIFI_IFACE_STATUS", cmd_wifi_iface_status, "RCP_WIFI_IFACE_STATUS [index]"},
    {RCP_MCU_CAP_WIFI, "WIFI_PWR", cmd_wifi_pwr, "RCP_WIFI_PWR <ps_mode> <hbn_level>"},
    {RCP_MCU_CAP_WIFI, "WIFI_GET_PWR", cmd_wifi_get_pwr, "rcp_wifi_get_power_mode"},
    {RCP_MCU_CAP_WIFI, "WIFI_WKIO", cmd_wifi_wkio, "RCP_WIFI_WKIO <pin> <mode>"},
    {RCP_MCU_CAP_WIFI, "WIFI_DEWKIO", cmd_wifi_dewkio, "RCP_WIFI_DEWKIO <pin>"},
    {RCP_MCU_CAP_WIFI, "WIFI_WKTIMER", cmd_wifi_wktimer, "RCP_WIFI_WKTIMER <mode> <timeout_ms>"},
    {RCP_MCU_CAP_WIFI, "WIFI_SET_DTIM", cmd_wifi_set_dtim, "RCP_WIFI_SET_DTIM <dtim>"},
    {RCP_MCU_CAP_WIFI, "WIFI_CLR_DTIM", cmd_wifi_clr_dtim, "rcp_wifi_clear_dtim"},
    {RCP_MCU_CAP_WIFI, "WIFI_GET_DTIM", cmd_wifi_get_dtim, "rcp_wifi_get_dtim_ap"},
    {RCP_MCU_CAP_WIFI, "WIFI_KEEPALIVE_START", cmd_wifi_keepalive_start, "RCP_WIFI_KEEPALIVE_START <period>"},
    {RCP_MCU_CAP_WIFI, "WIFI_KEEPALIVE_STOP", cmd_wifi_keepalive_stop, "rcp_wifi_stop_keepalive"},
    {RCP_MCU_CAP_WIFI, "WIFI_ARP_START", cmd_wifi_arp_start, "RCP_WIFI_ARP_START <period>"},
    {RCP_MCU_CAP_WIFI, "WIFI_ARP_STOP", cmd_wifi_arp_stop, "rcp_wifi_stop_arp"},
    {RCP_MCU_CAP_WIFI, "WIFI_CLK_SET", cmd_wifi_clk_set, "RCP_WIFI_CLK_SET <source>"},
    {RCP_MCU_CAP_WIFI, "WIFI_CLK_GET", cmd_wifi_clk_get, "rcp_wifi_get_clock_source"},
    {RCP_MCU_CAP_WIFI, "WIFI_TWT_SETUP", cmd_wifi_twt_setup, "RCP_WIFI_TWT_SETUP <setup_type> <flow_type> <wake_exp> <min_dur> <mantissa>"},
    {RCP_MCU_CAP_WIFI, "WIFI_TWT_SLEEP", cmd_wifi_twt_sleep, "rcp_wifi_twt_sleep"},
    {RCP_MCU_CAP_WIFI, "WIFI_TWT_STATUS", cmd_wifi_twt_status, "rcp_wifi_twt_get_status"},
    {RCP_MCU_CAP_WIFI, "WIFI_TWT_SUPPORTED", cmd_wifi_twt_supported, "rcp_wifi_twt_is_supported"},
    {RCP_MCU_CAP_WIFI, "WIFI_TWT_TEARDOWN", cmd_wifi_twt_teardown, "RCP_WIFI_TWT_TEARDOWN [all] [id]"},
    {RCP_MCU_CAP_WIFI, "WIFI_PWR_CLEAR", cmd_wifi_pwr_clear, "rcp_wifi_clear_power_stats"},
    {RCP_MCU_CAP_WIFI, "WIFI_PWR_GET", cmd_wifi_pwr_get, "rcp_wifi_get_power_stats"},
    {RCP_MCU_CAP_WIFI, "WIFI_LP_INTERVAL", cmd_wifi_lp_interval, "rcp_wifi_get_lp_interval"},
    {RCP_MCU_CAP_WIFI, "WIFI_COEX_DUTY_SET", cmd_wifi_coex_duty_set, "RCP_WIFI_COEX_DUTY_SET <active_ms>"},
    {RCP_MCU_CAP_WIFI, "WIFI_COEX_DUTY_GET", cmd_wifi_coex_duty_get, "rcp_wifi_get_coex_duty"},
    {RCP_MCU_CAP_WIFI, "WIFI_COEX_EN_SET", cmd_wifi_coex_en_set, "RCP_WIFI_COEX_EN_SET <0|1>"},
    {RCP_MCU_CAP_WIFI, "WIFI_COEX_EN_GET", cmd_wifi_coex_en_get, "rcp_wifi_get_coex_enable"},
    {RCP_MCU_CAP_WIFI, "WIFI_ANT_SET", cmd_wifi_ant_set, "RCP_WIFI_ANT_SET <0|1|2>"},
    {RCP_MCU_CAP_WIFI, "WIFI_ANT_GET", cmd_wifi_ant_get, "rcp_wifi_get_antenna_enable"},
    {RCP_MCU_CAP_WIFI, "WIFI_ANT_USED", cmd_wifi_ant_used, "rcp_wifi_get_antenna_used"},
};

const size_t rcp_unit_test_commands_count =
    sizeof(rcp_unit_test_commands) / sizeof(rcp_unit_test_commands[0]);

static uint32_t arg_u32(char **argv, int index)
{
  return (uint32_t)strtoul(argv[index], NULL, 0);
}

static void print_wifi_status(const char *name, enum rcp_wifi_status status)
{
  if (status == RCP_WIFI_OK) {
    printf("%s OK\n", name);
  } else {
    printf("%s failed: %d\n", name, status);
  }
}

static void print_mac(const uint8_t mac[6])
{
  printf("%02x:%02x:%02x:%02x:%02x:%02x",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool parse_mac(const char *text, uint8_t mac[6])
{
  unsigned int value[6];

  if (sscanf(text, "%02x:%02x:%02x:%02x:%02x:%02x",
             &value[0], &value[1], &value[2], &value[3], &value[4],
             &value[5]) != 6) {
    return false;
  }

  for (uint32_t i = 0U; i < 6U; i++) {
    if (value[i] > 0xffU) {
      return false;
    }
    mac[i] = (uint8_t)value[i];
  }

  return true;
}

static void rcp_unit_test_wifi_event_cb(enum rcp_wifi_evt evt, const void *data)
{
  if (evt == RCP_WIFI_L2_INTERFACES_RESP) {
    const struct rcp_wifi_l2_interfaces *resp =
        (const struct rcp_wifi_l2_interfaces *)data;

    if (resp == NULL) {
      printf("l2 interfaces: null response\n");
      return;
    }

    printf("l2 interface count=%lu\n", (unsigned long)resp->count);
    for (uint32_t i = 0U; i < resp->count && i < RCP_WIFI_MAX_L2_INTERFACES;
         i++) {
      printf("if[%lu]: index=%lu type=%d mtu=%lu offload=%d mac=",
             (unsigned long)i, (unsigned long)resp->net_ifs[i].index,
             resp->net_ifs[i].net_if_type,
             (unsigned long)resp->net_ifs[i].mtu,
             resp->net_ifs[i].offload_supported);
      print_mac(resp->net_ifs[i].mac_address);
      printf("\n");
    }
    return;
  }

  if (evt == RCP_WIFI_IFACE_STATUS_RESP) {
    const struct rcp_wifi_evt_iface_status *resp =
        (const struct rcp_wifi_evt_iface_status *)data;

    if (resp == NULL) {
      printf("iface status: null response\n");
      return;
    }

    printf("iface index=%lu state=%d mode=%d link=%d channel=%lu rssi=%ld "
           "security=%d pmf=%d ssid_len=%u ssid=",
           (unsigned long)resp->iface_index, resp->status.state,
           resp->status.iface_mode, resp->status.link_mode,
           (unsigned long)resp->status.channel, (long)resp->status.rssi,
           resp->status.security, resp->status.mfp, resp->status.ssid_len);
    for (uint32_t i = 0U; i < resp->status.ssid_len; i++) {
      putchar(resp->status.ssid[i]);
    }
    printf(" bssid=");
    print_mac(resp->status.bssid);
    printf("\n");
    return;
  }
}

static void hexstr_to_bytes(const char *hexstr, uint8_t *buf,
                            uint32_t max_len)
{
  uint32_t i;

  for (i = 0; (hexstr[0] != '\0') && (hexstr[1] != '\0') && (i < max_len);
       i++) {
    char tmp[3] = {hexstr[0], hexstr[1], '\0'};
    buf[i] = (uint8_t)strtoul(tmp, NULL, 16);
    hexstr += 2;
  }
}

static void print_scan_result(int32_t status, RCP_WiFi_Scan_Result_t *results)
{
  if (status != RCP_STATUS_OK) {
    printf("scan failed: %ld\n", (long)status);
    return;
  }
  if (results == NULL || results->AP == NULL) {
    printf("scan done: no result buffer\n");
    return;
  }

  printf("scan done: %lu APs\n", (unsigned long)results->Count);
  for (uint32_t i = 0U; i < results->Count; i++) {
    RCP_WiFi_AP_t *ap = &results->AP[i];
    printf("AP[%lu]: ssid=%s rssi=%d channel=%u security=%d mac=",
           (unsigned long)i, ap->SSID, ap->RSSI, ap->Channel,
           ap->Security);
    print_mac(ap->MAC);
    printf("\n");
  }
}

static void cmd_send_RCP(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_SendAT(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("send AT cmd failed\n");
  }
}

static void cmd_send_RCP_RST(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_Reset(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("send AT cmd failed\n");
  }
}

static void cmd_send_RCP_SYSRAM(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  uint32_t RemainingHeap = 0;
  uint32_t LwipHeap = 0;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetNcpHeapState(Obj, &RemainingHeap, &LwipHeap);

  if (ret == RCP_STATUS_OK) {
    printf("RemainingHeap: %lu, LwipHeap: %lu\n",
           (unsigned long)RemainingHeap, (unsigned long)LwipHeap);
  } else {
    printf("send AT cmd failed\n");
  }
}

static void cmd_send_RCP_RESTORE(int argc, char **argv)
{
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

static void cmd_send_RCP_SYSSTORE(int argc, char **argv)
{
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

static void cmd_send_RCP_EFUSE_R(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_EFUSE_R <nbytes> <addr> [reload]\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
  uint32_t reload = 1;
  if (nbytes == 0 || nbytes > 256) {
    printf("nbytes must be 1~256\n");
    return;
  }
  if (argc >= 4) {
    reload = arg_u32(argv, 3);
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

static void cmd_send_RCP_IORST(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_IORST <pin>\n");
    return;
  }
  uint32_t pin = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_RestoreGPIO(Obj, pin);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_IORST failed\n");
  }
}

static void cmd_send_RCP_OTASTART(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_OTASTART <0|1>\n");
    return;
  }
  uint32_t enable = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FWU_starts(Obj, enable);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_OTASTART failed\n");
  }
}

static void cmd_send_RCP_OTAFIN(int argc, char **argv)
{
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

static void cmd_send_RCP_FS_DEL(int argc, char **argv)
{
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

static void cmd_send_RCP_FS_CREATE(int argc, char **argv)
{
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

static void cmd_send_RCP_FS_WRITE(int argc, char **argv)
{
  if (argc < 4) {
    printf("Usage: RCP_FS_WRITE <filename> <offset> <data>\n");
    return;
  }

  uint32_t offset = arg_u32(argv, 2);
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

static void cmd_send_RCP_FS_READ(int argc, char **argv)
{
  if (argc < 4) {
    printf("Usage: RCP_FS_READ <filename> <offset> <len>\n");
    return;
  }

  uint32_t offset = arg_u32(argv, 2);
  uint32_t len = arg_u32(argv, 3);
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

static void cmd_send_RCP_FS_SIZE(int argc, char **argv)
{
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

static void cmd_send_RCP_FS_LS(int argc, char **argv)
{
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

static void cmd_send_RCP_CMD(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetCmdList(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_CMD failed\n");
  }
}

static void cmd_send_RCPE(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCPE <0|1>\n");
    return;
  }
  uint32_t enable = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_SetEcho(Obj, enable);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCPE failed\n");
  }
}

static void cmd_send_RCP_GMAC(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  uint8_t slot0[6], slot1[6], slot2[6];
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetGMAC(Obj, slot0, slot1, slot2);
  if (ret == RCP_STATUS_OK) {
    printf("+GMACSLOT0:");
    print_mac(slot0);
    printf("\n+GMACSLOT1:");
    print_mac(slot1);
    printf("\n+GMACSLOT2:");
    print_mac(slot2);
    printf("\n");
  } else {
    printf("RCP_GMAC failed\n");
  }
}

static void cmd_send_RCP_GMR(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GetGMR(Obj);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_GMR failed\n");
  }
}

static void cmd_send_RCP_PN(int argc, char **argv)
{
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

static void cmd_send_RCP_VBAT(int argc, char **argv)
{
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

static void cmd_send_RCP_SYSMSG(int argc, char **argv)
{
  RCP_Object_t *Obj = RCP_ObjGet();
  if (argc >= 2) {
    uint32_t state = arg_u32(argv, 1);
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

static void cmd_send_RCP_SYSLOG(int argc, char **argv)
{
  RCP_Object_t *Obj = RCP_ObjGet();
  if (argc >= 2) {
    uint32_t state = arg_u32(argv, 1);
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

static void cmd_send_RCP_TEMP(int argc, char **argv)
{
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

static void cmd_send_RCP_EFUSE_CFM(int argc, char **argv)
{
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

static void cmd_send_RCP_FLASH_E(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_FLASH-E <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FlashErase(Obj, nbytes, argv[2]);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_FLASH-E failed\n");
  }
}

static void cmd_send_RCP_IOPUPD(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_IOPUPD <pin> <0|1|2>\n");
    return;
  }
  uint32_t pin = arg_u32(argv, 1);
  uint32_t state = arg_u32(argv, 2);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GPIOPullUpDn(Obj, pin, state);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_IOPUPD failed\n");
  }
}

static void cmd_send_RCP_IOOUT(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_IOOUT <pin> <0|1>\n");
    return;
  }
  uint32_t pin = arg_u32(argv, 1);
  uint32_t state = arg_u32(argv, 2);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_GPIOOutput(Obj, pin, state);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_IOOUT failed\n");
  }
}

static void cmd_send_RCP_IOIN(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_IOIN <pin> [pull_state] | RCP_IOIN <pin> ?\n");
    return;
  }
  uint32_t pin = arg_u32(argv, 1);
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
    uint32_t pull_state = arg_u32(argv, 2);
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

static void cmd_send_RCP_SYSSTORE_SET(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_SYSSTORE_SET <0|1>\n");
    return;
  }
  uint32_t mode = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_SetStoreMode(Obj, mode);
  if (ret == RCP_STATUS_OK) {
    printf("OK\n");
  } else {
    printf("RCP_SYSSTORE_SET failed\n");
  }
}

static void cmd_send_RCP_EFUSE_W(int argc, char **argv)
{
  if (argc < 4) {
    printf("Usage: RCP_EFUSE_W <nbytes> <addr> <hex_data>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
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

static void cmd_send_RCP_EFUSE_W_HEX(int argc, char **argv)
{
  if (argc < 4) {
    printf("Usage: RCP_EFUSE_W_HEX <nbytes> <addr> <hex_string>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_EFuseWriteHex(Obj, nbytes, argv[2], argv[3]);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_EFUSE_W_HEX failed\n");
  }
}

static void cmd_send_RCP_EFUSE_R_HEX(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_EFUSE_R_HEX <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
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

static void cmd_send_RCP_FLASH_W(int argc, char **argv)
{
  if (argc < 4) {
    printf("Usage: RCP_FLASH_W <nbytes> <addr> <hex_data>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
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

static void cmd_send_RCP_FLASH_W_HEX(int argc, char **argv)
{
  if (argc < 4) {
    printf("Usage: RCP_FLASH_W_HEX <nbytes> <addr> <hex_string>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
  RCP_Object_t *Obj = RCP_ObjGet();
  int ret = RCP_FlashWriteHex(Obj, nbytes, argv[2], argv[3]);
  if (ret == RCP_STATUS_OK) {
    printf("SEND OK\n");
  } else {
    printf("RCP_FLASH_W_HEX failed\n");
  }
}

static void cmd_send_RCP_FLASH_R(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_FLASH_R <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
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

static void cmd_send_RCP_FLASH_R_HEX(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_FLASH_R_HEX <nbytes> <addr>\n");
    return;
  }
  uint32_t nbytes = arg_u32(argv, 1);
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

static void cmd_send_RCP_PART(int argc, char **argv)
{
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

static void cmd_send_RCP_MINIDUMP(int argc, char **argv)
{
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

static void cmd_send_RCP_PWR(int argc, char **argv)
{
  uint32_t ps_mode = 0, hbn_level = 0;
  int ret;
  RCP_Object_t *Obj = RCP_ObjGet();

  if (argc < 2) {
    printf("Usage: RCP_PWR <ps_mode> [hbn_level]\n");
    return;
  }

  ps_mode = arg_u32(argv, 1);
  if (argc >= 3) {
    hbn_level = arg_u32(argv, 2);
  }
  ret = RCP_SetPowerMode(Obj, ps_mode, hbn_level);
  if (ret != RCP_STATUS_OK) {
    printf("RCP_PWR failed\n");
    return;
  }
  printf("\nOK\n");
}

static void cmd_send_RCP_SetWakeUpPin(int argc, char **argv)
{
  int ret;
  RCP_Object_t *Obj = RCP_ObjGet();

  if (argc < 2) {
    printf("Usage: RCP_SetWakeUpPin <pin>\n");
    return;
  }
  ret = RCP_SetWakeUpPin(Obj, arg_u32(argv, 1));
  if (ret != RCP_STATUS_OK) {
    printf("SetWakeUpPin failed\n");
    return;
  }
  printf("\nOK\n");
}

static void cmd_wifi_connect(int argc, char **argv)
{
  struct rcp_wifi_conn_req req = {0};

  if (argc < 2) {
    printf("Usage: wifi_connect <SSID> [PSK] [PMF:0|1|2]\n");
    return;
  }

  strncpy((char *)req.ssid, argv[1], sizeof(req.ssid) - 1U);
  req.ssid_len = (uint8_t)strlen(argv[1]);
  req.pmf = RCP_WIFI_MFP_OPTIONAL;
  req.dhcp = true;

  if (argc >= 3) {
    strncpy((char *)req.psk, argv[2], sizeof(req.psk) - 1U);
    req.psk_len = (uint8_t)strlen(argv[2]);
    req.sec_type = RCP_WIFI_SEC_TYPE_PSK;
  } else {
    req.sec_type = RCP_WIFI_SEC_TYPE_NONE;
  }

  if (argc >= 4) {
    uint32_t pmf = arg_u32(argv, 3);
    if (pmf > RCP_WIFI_MFP_REQUIRED) {
      printf("Invalid PMF value '%s', expected 0, 1, or 2\n", argv[3]);
      return;
    }
    req.pmf = (enum rcp_wifi_mfp_options)pmf;
  }

  printf("Connecting to '%s' (pmf=%d)...\n", argv[1], (int)req.pmf);
  (void)rcp_wifi_station_start();
  print_wifi_status("rcp_wifi_connect", rcp_wifi_connect(&req));
}

static void cmd_wifi_disconnect(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  printf("Disconnecting from WiFi...\n");
  print_wifi_status("rcp_wifi_disconnect", rcp_wifi_disconnect());
}

static int ota_recv_exact(int sock, uint8_t *buf, uint32_t len)
{
  uint32_t total = 0U;

  while (total < len) {
    int ret = recv(sock, buf + total, len - total, 0);
    if (ret <= 0) {
      return ret;
    }
    total += (uint32_t)ret;
  }

  return (int)total;
}

static void ota_dump_sha256(const uint8_t sha256[32])
{
  for (uint32_t i = 0U; i < 32U; i++) {
    printf("%02X", sha256[i]);
  }
  printf("\r\n");
}

static int ota_parse_header(const ota_header_t *ota_header, uint32_t *body_len,
                            uint8_t *use_xz)
{
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

  memcpy(str, ota_header->u.s.ver_hardware,
         sizeof(ota_header->u.s.ver_hardware));
  str[sizeof(ota_header->u.s.ver_hardware)] = '\0';
  printf("[OTA] [HEADER] ver_hardware is %s\r\n", str);

  memcpy(str, ota_header->u.s.ver_software,
         sizeof(ota_header->u.s.ver_software));
  str[sizeof(ota_header->u.s.ver_software)] = '\0';
  printf("[OTA] [HEADER] ver_software is %s\r\n", str);

  printf("[OTA] [HEADER] sha256 is ");
  ota_dump_sha256(ota_header->u.s.sha256);

  return 0;
}

static void cmd_ota_start(int argc, char **argv)
{
  const char *addr;
  const char *port = "3365";
  uint8_t *ota_data = NULL;
  int sock_client = -1;
  uint32_t ota_body_len = 0U;
  uint32_t total_sent = 0U;
  uint8_t use_xz = 0U;
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

  sock_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_client < 0) {
    printf("TCP Client create socket error\r\n");
    goto __err_exit;
  }

  if (connect(sock_client, (struct sockaddr *)&remote_addr,
              sizeof(struct sockaddr)) != 0) {
    printf("TCP client connect server falied!\r\n");
    goto __err_exit;
  }
  printf("TCP client connect server success!\r\n");

  printf("[OTA] receive first %u-byte OTA header...\r\n",
         (unsigned int)OTA_HEADER_PACKET_SIZE);

  if (ota_recv_exact(sock_client, ota_header.u.raw, OTA_HEADER_PACKET_SIZE) <=
      0) {
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
    int recv_len;

    if (to_recv > OTA_TRANSFER_CHUNK_SIZE) {
      to_recv = OTA_TRANSFER_CHUNK_SIZE;
    }

    recv_len = recv(sock_client, ota_data, to_recv, 0);
    if (recv_len <= 0) {
      printf("recv ota data from server failed, total=%lu/%lu, ret=%d, errno=%d\r\n",
             (unsigned long)total_sent, (unsigned long)ota_body_len, recv_len,
             errno);
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
}

static void cmd_wifi_sta_start(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_station_start", rcp_wifi_station_start());
}

static void cmd_wifi_auto_conn_set(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_AUTO_SET <0|1>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_auto_connect",
                    rcp_wifi_set_auto_connect(arg_u32(argv, 1)));
}

static void cmd_wifi_auto_conn_get(int argc, char **argv)
{
  uint32_t onoff = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_auto_connect(&onoff);
  if (status == RCP_WIFI_OK) {
    printf("auto_connect=%lu\n", (unsigned long)onoff);
  } else {
    print_wifi_status("rcp_wifi_get_auto_connect", status);
  }
}

static void cmd_wifi_scan(int argc, char **argv)
{
  rcp_wifi_scan_opts_t opts = {0};
  enum rcp_wifi_status status;

  opts.scan_type = RCP_WIFI_SCAN_STATE_ACTIVE;
  opts.MaxCnt = 50U;
  if (argc >= 2) {
    if (strcmp(argv[1], "passive") == 0) {
      opts.scan_type = RCP_WIFI_SCAN_STATE_PASSIVE;
    } else if (strcmp(argv[1], "active") == 0) {
      opts.scan_type = RCP_WIFI_SCAN_STATE_ACTIVE;
    } else {
      opts.scan_type = (RCP_WiFi_scan_type_e)arg_u32(argv, 1);
    }
  }
  if (argc >= 3) {
    opts.Channel = (uint8_t)arg_u32(argv, 2);
  }
  if (argc >= 4) {
    opts.MaxCnt = (uint8_t)arg_u32(argv, 3);
  }
  if (argc >= 5) {
    strncpy((char *)opts.SSID, argv[4], sizeof(opts.SSID) - 1U);
  }

  status = rcp_wifi_scan(&opts, print_scan_result);
  print_wifi_status("rcp_wifi_scan", status);
}

static void cmd_wifi_set_scan_opts(int argc, char **argv)
{
  rcp_wifi_scan_opts_t opts = {0};
  uint8_t mac[6];

  opts.scan_type = RCP_WIFI_SCAN_STATE_ACTIVE;
  opts.MaxCnt = 50U;
  if (argc >= 2) {
    if (strcmp(argv[1], "passive") == 0) {
      opts.scan_type = RCP_WIFI_SCAN_STATE_PASSIVE;
    } else if (strcmp(argv[1], "active") == 0) {
      opts.scan_type = RCP_WIFI_SCAN_STATE_ACTIVE;
    } else {
      opts.scan_type = (RCP_WiFi_scan_type_e)arg_u32(argv, 1);
    }
  }
  if (argc >= 3) {
    opts.Channel = (uint8_t)arg_u32(argv, 2);
  }
  if (argc >= 4) {
    opts.MaxCnt = (uint8_t)arg_u32(argv, 3);
  }
  if (argc >= 5) {
    strncpy((char *)opts.SSID, argv[4], sizeof(opts.SSID) - 1U);
  }
  if (argc >= 6) {
    if (!parse_mac(argv[5], mac)) {
      printf("Usage: RCP_WIFI_SET_SCAN_OPTS [active|passive] [channel] [max_cnt] [ssid] [mac]\n");
      return;
    }
    memcpy(opts.MAC, mac, sizeof(opts.MAC));
  }

  print_wifi_status("rcp_wifi_set_scan_opts", rcp_wifi_set_scan_opts(&opts));
}

static void cmd_wifi_scan_start(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_scan_start", rcp_wifi_scan_start());
}

static void cmd_wifi_reconn_set(int argc, char **argv)
{
  rcp_wifi_connect_opts_t opts = {0};

  if (argc < 3) {
    printf("Usage: RCP_WIFI_RECONN_SET <interval_s> <attempts>\n");
    return;
  }

  opts.Reconnection_interval = (uint16_t)arg_u32(argv, 1);
  opts.Reconnection_nb_attempts = (uint16_t)arg_u32(argv, 2);
  print_wifi_status("rcp_wifi_set_reconnection_opts",
                    rcp_wifi_set_reconnection_opts(&opts));
}

static void cmd_wifi_connect_opts(int argc, char **argv)
{
  rcp_wifi_connect_opts_t opts = {0};
  uint8_t mac[6];

  if (argc < 2) {
    printf("Usage: RCP_WIFI_CONNECT_OPTS <ssid> [psk] [pmf] [wep] [wps] [interval_s] [attempts] [bssid]\n");
    return;
  }

  strncpy((char *)opts.SSID, argv[1], sizeof(opts.SSID) - 1U);
  if (argc >= 3) {
    strncpy((char *)opts.Password, argv[2], sizeof(opts.Password) - 1U);
  }
  if (argc >= 4) {
    opts.PMF = arg_u32(argv, 3);
  }
  if (argc >= 5) {
    opts.WEP = arg_u32(argv, 4);
  }
  if (argc >= 6) {
    opts.WPS = arg_u32(argv, 5);
  }
  if (argc >= 7) {
    opts.Reconnection_interval = (uint16_t)arg_u32(argv, 6);
  }
  if (argc >= 8) {
    opts.Reconnection_nb_attempts = (uint16_t)arg_u32(argv, 7);
  }
  if (argc >= 9) {
    if (!parse_mac(argv[8], mac)) {
      printf("Usage: RCP_WIFI_CONNECT_OPTS <ssid> [psk] [pmf] [wep] [wps] [interval_s] [attempts] [bssid]\n");
      return;
    }
    memcpy(opts.MAC, mac, sizeof(opts.MAC));
  }

  print_wifi_status("rcp_wifi_connect_opts", rcp_wifi_connect_opts(&opts));
}

static void cmd_wifi_disconnect_restore(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_DISCONN_RESTORE <0|1>\n");
    return;
  }
  print_wifi_status("rcp_wifi_disconnect_restore",
                    rcp_wifi_disconnect_restore(arg_u32(argv, 1)));
}

static void cmd_wifi_connect_info(int argc, char **argv)
{
  int32_t rssi = 0;
  rcp_wifi_security_type_t security = RCP_WIFI_SECURITY_UNKNOWN;
  rcp_wifi_protocol_t protocol = RCP_WIFI_PROTOCOL_UNKNOWN;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_connect_info(&rssi, &security, &protocol);
  if (status == RCP_WIFI_OK) {
    printf("rssi=%ld security=%d protocol=%d\n", (long)rssi, security, protocol);
  } else {
    print_wifi_status("rcp_wifi_get_connect_info", status);
  }
}

static void cmd_wifi_sta_mac(int argc, char **argv)
{
  uint8_t mac[6] = {0};
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_station_get_mac_address(mac);
  if (status == RCP_WIFI_OK) {
    printf("sta_mac=");
    print_mac(mac);
    printf("\n");
  } else {
    print_wifi_status("rcp_wifi_station_get_mac_address", status);
  }
}

static void cmd_wifi_set_mac(int argc, char **argv)
{
  uint8_t mac[6];

  if (argc < 2 || !parse_mac(argv[1], mac)) {
    printf("Usage: RCP_WIFI_SET_MAC <mac>\n");
    return;
  }

  rcp_wifi_set_mac(mac);
  printf("rcp_wifi_set_mac OK\n");
}

static void cmd_wifi_get_mac(int argc, char **argv)
{
  uint8_t mac[6] = {0};
  uint32_t timeout_ms = (argc >= 2) ? arg_u32(argv, 1) : 0U;

  if (rcp_wifi_get_mac(mac, timeout_ms)) {
    printf("cached_mac=");
    print_mac(mac);
    printf("\n");
  } else {
    printf("rcp_wifi_get_mac timeout\n");
  }
}

static void cmd_wifi_country_get(int argc, char **argv)
{
  uint32_t policy = 0;
  char country[4] = {0};
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_country_code(&policy, country);
  if (status == RCP_WIFI_OK) {
    printf("country policy=%lu code=%s\n", (unsigned long)policy, country);
  } else {
    print_wifi_status("rcp_wifi_get_country_code", status);
  }
}

static void cmd_wifi_country_set(int argc, char **argv)
{
  uint32_t policy;

  if (argc < 3) {
    printf("Usage: RCP_WIFI_COUNTRY_SET <policy> <CN|JP|US|EU|00>\n");
    return;
  }
  policy = arg_u32(argv, 1);
  print_wifi_status("rcp_wifi_set_country_code",
                    rcp_wifi_set_country_code(&policy, argv[2]));
}

static void cmd_wifi_dual_mode(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  if (rcp_lwip_dhcpd_start() != ERR_OK) {
    printf("rcp_lwip_dhcpd_start fail\r\n");
    return RCP_WIFI_ERR;
  }

  print_wifi_status("rcp_wifi_set_dual_mode", rcp_wifi_set_dual_mode());
}

static void cmd_wifi_ap_start(int argc, char **argv)
{
  rcp_wifi_ap_config_t config = {0};

  if (argc < 5) {
    printf("Usage: RCP_WIFI_AP_START <ssid> <psk> <channel> <security> [max_conn] [hidden] [protocol]\n");
    return;
  }

  strncpy((char *)config.SSID, argv[1], sizeof(config.SSID) - 1U);
  strncpy((char *)config.Password, argv[2], sizeof(config.Password) - 1U);
  config.Channel = arg_u32(argv, 3);
  config.Security = (RCP_WiFi_ApSecurityType_e)arg_u32(argv, 4);
  config.MaxConnections = (argc >= 6) ? arg_u32(argv, 5) : 4U;
  config.Hidden = (argc >= 7) ? arg_u32(argv, 6) : 0U;
  config.Protocol = (argc >= 8) ? (rcp_wifi_protocol_t)arg_u32(argv, 7)
                                : RCP_WIFI_PROTOCOL_11N;
  print_wifi_status("rcp_wifi_ap_start", rcp_wifi_ap_start(&config));
}

static void cmd_wifi_ap_stop(int argc, char **argv)
{
  uint8_t reconnect = (argc >= 2) ? (uint8_t)arg_u32(argv, 1) : 0U;

  print_wifi_status("rcp_wifi_ap_stop", rcp_wifi_ap_stop(reconnect));
}

static void cmd_wifi_ap_config(int argc, char **argv)
{
  rcp_wifi_ap_config_t config = {0};
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_ap_get_config(&config);
  if (status == RCP_WIFI_OK) {
    printf("ap ssid=%s channel=%lu security=%d protocol=%d\n",
           config.SSID, (unsigned long)config.Channel, config.Security,
           config.Protocol);
  } else {
    print_wifi_status("rcp_wifi_ap_get_config", status);
  }
}

static void cmd_wifi_ap_list_sta(int argc, char **argv)
{
  rcp_wifi_connected_sta_t stations = {0};
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_ap_list_connected_stations(&stations);
  if (status != RCP_WIFI_OK) {
    print_wifi_status("rcp_wifi_ap_list_connected_stations", status);
    return;
  }

  printf("sta_count=%lu\n", (unsigned long)stations.Count);
  for (uint32_t i = 0U; i < stations.Count; i++) {
    printf("STA[%lu]: mac=", (unsigned long)i);
    print_mac(stations.STA[i].MAC);
    printf(" ip=%u.%u.%u.%u\n", stations.STA[i].IP[0], stations.STA[i].IP[1],
           stations.STA[i].IP[2], stations.STA[i].IP[3]);
  }
}

static void cmd_wifi_ap_disconnect_sta(int argc, char **argv)
{
  uint8_t mac[6];

  if (argc < 2 || !parse_mac(argv[1], mac)) {
    printf("Usage: RCP_WIFI_AP_DISCONN_STA <mac>\n");
    return;
  }

  print_wifi_status("rcp_wifi_ap_disconnect_station",
                    rcp_wifi_ap_disconnect_station(mac));
}

static void cmd_wifi_ap_mode_get(int argc, char **argv)
{
  rcp_wifi_protocol_t protocol = RCP_WIFI_PROTOCOL_UNKNOWN;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_ap_get_mode(&protocol);
  if (status == RCP_WIFI_OK) {
    printf("ap_protocol=%d\n", protocol);
  } else {
    print_wifi_status("rcp_wifi_ap_get_mode", status);
  }
}

static void cmd_wifi_ap_mode_set(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_AP_MODE_SET <protocol>\n");
    return;
  }
  print_wifi_status("rcp_wifi_ap_set_mode",
                    rcp_wifi_ap_set_mode((rcp_wifi_protocol_t)arg_u32(argv, 1)));
}

static void cmd_wifi_ap_mac(int argc, char **argv)
{
  uint8_t mac[6] = {0};
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_ap_get_mac_address(mac);
  if (status == RCP_WIFI_OK) {
    printf("ap_mac=");
    print_mac(mac);
    printf("\n");
  } else {
    print_wifi_status("rcp_wifi_ap_get_mac_address", status);
  }
}

static void cmd_wifi_l2_ifaces(int argc, char **argv)
{
  enum rcp_netif_type type = RCP_NET_IF_TYPE_UNKNOWN;
  uint32_t index = 0U;

  if (argc >= 2) {
    type = (enum rcp_netif_type)arg_u32(argv, 1);
  }
  if (argc >= 3) {
    index = arg_u32(argv, 2);
  }

  (void)rcp_wifi_register_callback(rcp_unit_test_wifi_event_cb);
  print_wifi_status("rcp_wifi_get_l2_interfaces",
                    rcp_wifi_get_l2_interfaces(type, index));
}

static void cmd_wifi_iface_status(int argc, char **argv)
{
  uint32_t index = (argc >= 2) ? arg_u32(argv, 1) : 1U;

  (void)rcp_wifi_register_callback(rcp_unit_test_wifi_event_cb);
  print_wifi_status("rcp_wifi_get_iface_status",
                    rcp_wifi_get_iface_status(index));
}

static void cmd_wifi_pwr(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_WIFI_PWR <ps_mode> <hbn_level>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_power_mode",
                    rcp_wifi_set_power_mode(arg_u32(argv, 1), arg_u32(argv, 2)));
}

static void cmd_wifi_get_pwr(int argc, char **argv)
{
  uint32_t ps_mode = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_power_mode(&ps_mode);
  if (status == RCP_WIFI_OK) {
    printf("ps_mode=%lu\n", (unsigned long)ps_mode);
  } else {
    print_wifi_status("rcp_wifi_get_power_mode", status);
  }
}

static void cmd_wifi_wkio(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_WIFI_WKIO <pin> <mode>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_wakeup_pin",
                    rcp_wifi_set_wakeup_pin(arg_u32(argv, 1),
                                            arg_u32(argv, 2)));
}

static void cmd_wifi_dewkio(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_DEWKIO <pin>\n");
    return;
  }
  print_wifi_status("rcp_wifi_delete_wakeup_pin",
                    rcp_wifi_delete_wakeup_pin(arg_u32(argv, 1)));
}

static void cmd_wifi_wktimer(int argc, char **argv)
{
  if (argc < 3) {
    printf("Usage: RCP_WIFI_WKTIMER <mode> <timeout_ms>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_wakeup_timer",
                    rcp_wifi_set_wakeup_timer(arg_u32(argv, 1),
                                              arg_u32(argv, 2)));
}

static void cmd_wifi_set_dtim(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_SET_DTIM <dtim>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_dtim", rcp_wifi_set_dtim(arg_u32(argv, 1)));
}

static void cmd_wifi_clr_dtim(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_clear_dtim", rcp_wifi_clear_dtim());
}

static void cmd_wifi_get_dtim(int argc, char **argv)
{
  uint32_t dtim = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_dtim_ap(&dtim);
  if (status == RCP_WIFI_OK) {
    printf("dtim=%lu\n", (unsigned long)dtim);
  } else {
    print_wifi_status("rcp_wifi_get_dtim_ap", status);
  }
}

static void cmd_wifi_keepalive_start(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_KEEPALIVE_START <period>\n");
    return;
  }
  print_wifi_status("rcp_wifi_start_keepalive",
                    rcp_wifi_start_keepalive(arg_u32(argv, 1)));
}

static void cmd_wifi_keepalive_stop(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_stop_keepalive", rcp_wifi_stop_keepalive());
}

static void cmd_wifi_arp_start(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_ARP_START <period>\n");
    return;
  }
  print_wifi_status("rcp_wifi_start_arp",
                    rcp_wifi_start_arp(arg_u32(argv, 1)));
}

static void cmd_wifi_arp_stop(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_stop_arp", rcp_wifi_stop_arp());
}

static void cmd_wifi_clk_set(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_CLK_SET <source>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_clock_source",
                    rcp_wifi_set_clock_source(arg_u32(argv, 1)));
}

static void cmd_wifi_clk_get(int argc, char **argv)
{
  uint32_t source = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_clock_source(&source);
  if (status == RCP_WIFI_OK) {
    printf("clock_source=%lu\n", (unsigned long)source);
  } else {
    print_wifi_status("rcp_wifi_get_clock_source", status);
  }
}

static void cmd_wifi_twt_setup(int argc, char **argv)
{
  rcp_wifi_twt_setup_params_t params = {0};

  if (argc < 6) {
    printf("Usage: RCP_WIFI_TWT_SETUP <setup_type> <flow_type> <wake_exp> <min_dur> <mantissa>\n");
    return;
  }

  params.setup_type = (uint8_t)arg_u32(argv, 1);
  params.flow_type = (uint8_t)arg_u32(argv, 2);
  params.wake_int_exp = arg_u32(argv, 3);
  params.min_twt_wake_dur = arg_u32(argv, 4);
  params.wake_int_mantissa = arg_u32(argv, 5);
  print_wifi_status("rcp_wifi_twt_setup", rcp_wifi_twt_setup(&params));
}

static void cmd_wifi_twt_sleep(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_twt_sleep", rcp_wifi_twt_sleep());
}

static void cmd_wifi_twt_status(int argc, char **argv)
{
  rcp_wifi_twt_status_t status_data = {0};
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_twt_get_status(&status_data);
  if (status != RCP_WIFI_OK) {
    print_wifi_status("rcp_wifi_twt_get_status", status);
    return;
  }

  printf("twt supported=%u flow_count=%u\n", status_data.is_supported,
         status_data.flow_count);
  for (uint32_t i = 0U; i < status_data.flow_count && i < RCP_WIFI_MAX_TWT_FLOWS;
       i++) {
    printf("flow[%lu]: type=%u exp=%lu min_dur=%lu mantissa=%lu\n",
           (unsigned long)i, status_data.flow[i].flow_type,
           (unsigned long)status_data.flow[i].wake_int_exp,
           (unsigned long)status_data.flow[i].min_twt_wake_dur,
           (unsigned long)status_data.flow[i].wake_int_mantissa);
  }
}

static void cmd_wifi_twt_supported(int argc, char **argv)
{
  uint32_t is_supported = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_twt_is_supported(&is_supported);
  if (status == RCP_WIFI_OK) {
    printf("twt_supported=%lu\n", (unsigned long)is_supported);
  } else {
    print_wifi_status("rcp_wifi_twt_is_supported", status);
  }
}

static void cmd_wifi_twt_teardown(int argc, char **argv)
{
  rcp_wifi_twt_teardown_params_t params = {0};

  if (argc >= 2) {
    params.all_twt = (uint8_t)arg_u32(argv, 1);
  }
  if (argc >= 3) {
    params.id = (uint8_t)arg_u32(argv, 2);
  }
  print_wifi_status("rcp_wifi_twt_teardown", rcp_wifi_twt_teardown(&params));
}

static void cmd_wifi_pwr_clear(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  print_wifi_status("rcp_wifi_clear_power_stats", rcp_wifi_clear_power_stats());
}

static void cmd_wifi_pwr_get(int argc, char **argv)
{
  uint64_t power = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_power_stats(&power);
  if (status == RCP_WIFI_OK) {
    printf("average_power=%llu\n", (unsigned long long)power);
  } else {
    print_wifi_status("rcp_wifi_get_power_stats", status);
  }
}

static void cmd_wifi_lp_interval(int argc, char **argv)
{
  uint32_t interval = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_lp_interval(&interval);
  if (status == RCP_WIFI_OK) {
    printf("lp_interval=%lu\n", (unsigned long)interval);
  } else {
    print_wifi_status("rcp_wifi_get_lp_interval", status);
  }
}

static void cmd_wifi_coex_duty_set(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_COEX_DUTY_SET <active_ms>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_coex_duty",
                    rcp_wifi_set_coex_duty(arg_u32(argv, 1)));
}

static void cmd_wifi_coex_duty_get(int argc, char **argv)
{
  uint32_t active_ms = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_coex_duty(&active_ms);
  if (status == RCP_WIFI_OK) {
    printf("coex_duty_active_ms=%lu\n", (unsigned long)active_ms);
  } else {
    print_wifi_status("rcp_wifi_get_coex_duty", status);
  }
}

static void cmd_wifi_coex_en_set(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_COEX_EN_SET <0|1>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_coex_enable",
                    rcp_wifi_set_coex_enable(arg_u32(argv, 1)));
}

static void cmd_wifi_coex_en_get(int argc, char **argv)
{
  uint32_t enable = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_coex_enable(&enable);
  if (status == RCP_WIFI_OK) {
    printf("coex_enable=%lu\n", (unsigned long)enable);
  } else {
    print_wifi_status("rcp_wifi_get_coex_enable", status);
  }
}

static void cmd_wifi_ant_set(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: RCP_WIFI_ANT_SET <0|1|2>\n");
    return;
  }
  print_wifi_status("rcp_wifi_set_antenna_enable",
                    rcp_wifi_set_antenna_enable(
                        (rcp_wifi_antenna_mode_t)arg_u32(argv, 1)));
}

static void cmd_wifi_ant_get(int argc, char **argv)
{
  rcp_wifi_antenna_mode_t mode = RCP_WIFI_ANTENNA_UNKNOWN;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_antenna_enable(&mode);
  if (status == RCP_WIFI_OK) {
    printf("antenna_mode=%d\n", mode);
  } else {
    print_wifi_status("rcp_wifi_get_antenna_enable", status);
  }
}

static void cmd_wifi_ant_used(int argc, char **argv)
{
  uint32_t antenna_id = 0;
  enum rcp_wifi_status status;

  (void)argc;
  (void)argv;
  status = rcp_wifi_get_antenna_used(&antenna_id);
  if (status == RCP_WIFI_OK) {
    printf("antenna_used=%lu\n", (unsigned long)antenna_id);
  } else {
    print_wifi_status("rcp_wifi_get_antenna_used", status);
  }
}
