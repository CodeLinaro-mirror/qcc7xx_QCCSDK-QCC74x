#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rcp_api.h"
#include "rcp_bus.h"
#include "rcp_common.h"
#include "rcp_lwip_netif.h"
#include "rcp_os.h"
#include "rcp_tty.h"
#include "wifi/rcp_wifi_mgmt.h"

#define RCP_WIFI_DHCP_TIMEOUT_MS (15U * 1000U)

struct rcp_wifi_mgmt_ctx {
	uint8_t mac[WIFI_BSSID_SIZE];
	volatile bool mac_valid;
	bool dhcp_enabled;
	rcp_wifi_event_callback event_cb;
};

static struct rcp_wifi_mgmt_ctx g_rcp_wifi_mgmt;

static void rcp_wifi_sta_event_cb(RCP_event_id_t event_id, void *event_args);
static void rcp_wifi_notify(enum rcp_wifi_evt evt, const void *data);

static void rcp_wifi_security_to_api(enum rcp_wifi_sec_type sec_type,
				     RCP_WiFi_Connect_Opts_t *opts)
{
	if (sec_type == RCP_WIFI_SEC_TYPE_NONE) {
		opts->WEP = 0U;
		return;
	}

	if (sec_type == RCP_WIFI_SEC_TYPE_WEP) {
		opts->WEP = 1U;
		return;
	}

	opts->WEP = 0U;
}

static enum rcp_wifi_sec_type rcp_wifi_security_from_api(RCP_WiFi_SecurityType_e security)
{
	switch (security) {
	case RCP_WIFI_SECURITY_OPEN:
		return RCP_WIFI_SEC_TYPE_NONE;
	case RCP_WIFI_SECURITY_WEP:
		return RCP_WIFI_SEC_TYPE_WEP;
	case RCP_WIFI_SECURITY_WPA3_SAE:
	case RCP_WIFI_SECURITY_WPA2_WPA3_SAE:
		return RCP_WIFI_SEC_TYPE_SAE;
	case RCP_WIFI_SECURITY_WPA_ENT:
		return RCP_WIFI_SEC_TYPE_EAP;
	case RCP_WIFI_SECURITY_WPA_PSK:
	case RCP_WIFI_SECURITY_WPA2_PSK:
	case RCP_WIFI_SECURITY_WPA_WPA2_PSK:
	default:
		return RCP_WIFI_SEC_TYPE_PSK;
	}
}

static enum rcp_wifi_link_mode rcp_wifi_link_mode_from_api(RCP_WiFi_Protocol_e protocol)
{
	switch (protocol) {
	case RCP_WIFI_PROTOCOL_11B:
		return RCP_WIFI_LINK_MODE_WIFI_1;
	case RCP_WIFI_PROTOCOL_11G:
		return RCP_WIFI_LINK_MODE_WIFI_3;
	case RCP_WIFI_PROTOCOL_11N:
		return RCP_WIFI_LINK_MODE_WIFI_4;
	case RCP_WIFI_PROTOCOL_11AX:
		return RCP_WIFI_LINK_MODE_WIFI_6;
	case RCP_WIFI_PROTOCOL_UNKNOWN:
	default:
		return RCP_WIFI_LINK_MODE_UNKNOWN;
	}
}

static void rcp_wifi_notify(enum rcp_wifi_evt evt, const void *data)
{
	if (g_rcp_wifi_mgmt.event_cb != NULL) {
		g_rcp_wifi_mgmt.event_cb(evt, data);
	}
}

static void rcp_wifi_sta_event_cb(RCP_event_id_t event_id, void *event_args)
{
	RCP_Object_t *obj = RCP_ObjGet();
	struct rcp_wifi_evt_conn conn = {
		.status = 0,
	};

	(void)event_args;

	if (event_id == RCP_WIFI_EVT_SCAN_DONE_ID) {
		if (obj != NULL && obj->WifiCtx.scan_done_cb != NULL) {
			obj->WifiCtx.scan_done_cb(obj->WifiCtx.scan_status,
						  &obj->WifiCtx.ScanResults);
		}
		return;
	}

	if (event_id == RCP_WIFI_EVT_CONNECTED_ID) {
		if (g_rcp_wifi_mgmt.dhcp_enabled) {
			(void)rcp_lwip_dhcp_start(RCP_WIFI_DHCP_TIMEOUT_MS);
		}
		rcp_wifi_notify(RCP_WIFI_CONNECTED, &conn);
		return;
	}

	if (event_id == RCP_WIFI_EVT_DISCONNECTED_ID) {
		if (g_rcp_wifi_mgmt.dhcp_enabled) {
			(void)rcp_lwip_dhcp_done();
		}
		rcp_wifi_notify(RCP_WIFI_DISCONNECTED, &conn);
		return;
	}
}

enum rcp_wifi_status rcp_wifi_register_callback(rcp_wifi_event_callback evt_cb)
{
	if (evt_cb == NULL) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	g_rcp_wifi_mgmt.event_cb = evt_cb;
	return RCP_WIFI_OK;
}

enum rcp_wifi_status rcp_wifi_connect(struct rcp_wifi_conn_req *req)
{
	RCP_Object_t *obj = RCP_ObjGet();
	RCP_WiFi_Connect_Opts_t opts = {0};

	if (obj == NULL || req == NULL || req->ssid_len == 0U ||
	    req->ssid_len > WIFI_MAX_SSID_SIZE) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	if (req->psk_len > RCP_WIFI_MAX_PASSWORD_SIZE ||
	    req->bssid_len > WIFI_BSSID_SIZE) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}
	if (req->pmf > RCP_WIFI_MFP_REQUIRED) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	memcpy(opts.SSID, req->ssid, req->ssid_len);
	opts.SSID[req->ssid_len] = '\0';

	if (req->psk_len > 0U) {
		memcpy(opts.Password, req->psk, req->psk_len);
		opts.Password[req->psk_len] = '\0';
	}

	if (req->bssid_len == WIFI_BSSID_SIZE) {
		memcpy(opts.MAC, req->bssid, sizeof(opts.MAC));
	}

	opts.Reconnection_interval = 0U;
	opts.Reconnection_nb_attempts = 0U;
	opts.WPS = 0U;
	opts.PMF = req->pmf;
	rcp_wifi_security_to_api(req->sec_type, &opts);
	g_rcp_wifi_mgmt.dhcp_enabled = req->dhcp;

    // In RCP mode, turn off the dhcp of Softap and STA on the device side.
    RCP_Net_DhcpType_e dhcp_state = RCP_NET_DHCP_STA_AP_ENABLED;
    uint32_t dhcp_operate = 0U;
    int ret = RCP_Net_SetDhcpConfig(obj, &dhcp_state, &dhcp_operate);
    if (ret != RCP_STATUS_OK) {
      SYS_LOG_ERROR("Disable device DHCP failed\n");
	  return RCP_WIFI_ERR;
    }
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_Connect(obj, &opts));
}

enum rcp_wifi_status rcp_wifi_disconnect(void)
{
	return rcp_wifi_disconnect_restore(0U);
}

enum rcp_wifi_status rcp_wifi_disconnect_restore(uint32_t restore)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_Disconnect(RCP_ObjGet(), restore));
}

enum rcp_wifi_status rcp_wifi_ap_start(rcp_wifi_ap_config_t *ap_config)
{
	enum rcp_wifi_status status;

	status = rcp_wifi_status_from_rcp_status(RCP_WiFi_AP_Start(RCP_ObjGet(), ap_config));

	return status;
}

enum rcp_wifi_status rcp_wifi_ap_stop(uint8_t reconnect)
{
	enum rcp_wifi_status status;

	status = rcp_wifi_status_from_rcp_status(RCP_WiFi_AP_Stop(RCP_ObjGet(), reconnect));
	if (status == RCP_WIFI_OK && rcp_lwip_dhcpd_stop() != ERR_OK) {
		return RCP_WIFI_ERR;
	}

	return status;
}

enum rcp_wifi_status rcp_wifi_get_l2_interfaces(enum rcp_netif_type net_if_type,
						uint32_t index)
{
	struct rcp_wifi_l2_interfaces resp = {0};
	struct rcp_wifi_l2_interface *iface = &resp.net_ifs[0];

	if (net_if_type != RCP_NET_IF_TYPE_UNKNOWN && net_if_type != RCP_NET_IF_TYPE_WIFI) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	if (index > 1U) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	resp.count = 1U;
	iface->index = 1U;
	iface->net_if_type = RCP_NET_IF_TYPE_WIFI;
	iface->offload_supported = false;
	iface->mtu = 1500U;
	(void)rcp_wifi_get_mac(iface->mac_address, 0U);

	rcp_wifi_notify(RCP_WIFI_L2_INTERFACES_RESP, &resp);
	return RCP_WIFI_OK;
}

enum rcp_wifi_status rcp_wifi_scan(rcp_wifi_scan_opts_t *opts,
				   rcp_wifi_scan_result_cb_t scan_result_cb)
{
	RCP_Object_t *obj = RCP_ObjGet();
	RCP_Status_t ret;

	if (obj == NULL || opts == NULL || scan_result_cb == NULL) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	obj->WifiCtx.scan_done_cb = scan_result_cb;

	ret = RCP_WiFi_SetScanOpts(obj, opts);
	if (ret != RCP_STATUS_OK) {
		return rcp_wifi_status_from_rcp_status(ret);
	}

	ret = RCP_WiFi_Scan(obj);
	obj->WifiCtx.scan_status = ret;
	return rcp_wifi_status_from_rcp_status(ret);
}

enum rcp_wifi_status rcp_wifi_get_iface_status(uint32_t if_index)
{
	RCP_Object_t *obj = RCP_ObjGet();
	struct rcp_wifi_evt_iface_status resp = {0};
	RCP_WiFi_SecurityType_e security = RCP_WIFI_SECURITY_UNKNOWN;
	RCP_WiFi_Protocol_e protocol = RCP_WIFI_PROTOCOL_UNKNOWN;
	int32_t rssi = 0;
	RCP_Status_t ret;

	if (obj == NULL || if_index != 1U) {
		return RCP_WIFI_ERR_NOT_SUPP;
	}

	ret = RCP_WiFi_GetConnectInfo(obj, &rssi, &security, &protocol);
	if (ret != RCP_STATUS_OK) {
		return rcp_wifi_status_from_rcp_status(ret);
	}

	resp.iface_index = if_index;
	resp.status.iface_mode = RCP_WIFI_MODE_INFRA;
	resp.status.rssi = rssi;
	resp.status.channel = obj->WifiCtx.STASettings.Channel;
	resp.status.security = rcp_wifi_security_from_api(security);
	resp.status.link_mode = rcp_wifi_link_mode_from_api(protocol);
	resp.status.ssid_len = (uint8_t)strnlen((const char *)obj->WifiCtx.SSID,
						sizeof(resp.status.ssid));
	memcpy(resp.status.ssid, obj->WifiCtx.SSID, resp.status.ssid_len);
	memcpy(resp.status.bssid, obj->WifiCtx.APSettings.MAC_Addr, sizeof(resp.status.bssid));

	if (obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_CONNECTED ||
	    obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_GOT_IP) {
		resp.status.state = RCP_WIFI_STATE_COMPLETED;
	} else {
		resp.status.state = RCP_WIFI_STATE_DISCONNECTED;
	}

	rcp_wifi_notify(RCP_WIFI_IFACE_STATUS_RESP, &resp);
	return RCP_WIFI_OK;
}

enum rcp_wifi_status rcp_wifi_set_power_mode(uint32_t ps_mode, uint32_t hbn_level)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetPowerMode(RCP_ObjGet(), ps_mode, hbn_level));
}

enum rcp_wifi_status rcp_wifi_get_power_mode(uint32_t *ps_mode)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetPowerMode(RCP_ObjGet(), ps_mode));
}

enum rcp_wifi_status rcp_wifi_set_wakeup_pin(uint32_t wakeup_pin, uint32_t mode)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetWakeUpPin(RCP_ObjGet(), wakeup_pin, mode));
}

enum rcp_wifi_status rcp_wifi_clear_dtim(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_ClearDTIM(RCP_ObjGet()));
}

enum rcp_wifi_status rcp_wifi_delete_wakeup_pin(uint32_t wakeup_pin)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_DeleteWakeUpPin(RCP_ObjGet(), wakeup_pin));
}

enum rcp_wifi_status rcp_wifi_set_wakeup_timer(uint32_t mode, uint32_t timeout_ms)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetWakeUpTimer(RCP_ObjGet(), mode, timeout_ms));
}

enum rcp_wifi_status rcp_wifi_start_keepalive(uint32_t period)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_StartKeepAlive(RCP_ObjGet(), period));
}

enum rcp_wifi_status rcp_wifi_stop_keepalive(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_StopKeepAlive(RCP_ObjGet()));
}

enum rcp_wifi_status rcp_wifi_start_arp(uint32_t period)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_StartArp(RCP_ObjGet(), period));
}

enum rcp_wifi_status rcp_wifi_stop_arp(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_StopArp(RCP_ObjGet()));
}

enum rcp_wifi_status rcp_wifi_set_clock_source(uint32_t source)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetClockSource(RCP_ObjGet(), source));
}

enum rcp_wifi_status rcp_wifi_get_clock_source(uint32_t *source)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetClockSource(RCP_ObjGet(), source));
}

enum rcp_wifi_status rcp_wifi_twt_sleep(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_TWT_Sleep(RCP_ObjGet()));
}

enum rcp_wifi_status rcp_wifi_clear_power_stats(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_ClearPowerStats(RCP_ObjGet()));
}

enum rcp_wifi_status rcp_wifi_get_power_stats(uint64_t *power)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetPowerStats(RCP_ObjGet(), power));
}

enum rcp_wifi_status rcp_wifi_get_lp_interval(uint32_t *interval)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetLPInterval(RCP_ObjGet(), interval));
}

enum rcp_wifi_status rcp_wifi_set_coex_duty(uint32_t active_ms)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetCoexDuty(RCP_ObjGet(), active_ms));
}

enum rcp_wifi_status rcp_wifi_get_coex_duty(uint32_t *active_ms)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetCoexDuty(RCP_ObjGet(), active_ms));
}

enum rcp_wifi_status rcp_wifi_set_coex_enable(uint32_t enable)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetCoexEnable(RCP_ObjGet(), enable));
}

enum rcp_wifi_status rcp_wifi_get_coex_enable(uint32_t *enable)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetCoexEnable(RCP_ObjGet(), enable));
}

void rcp_wifi_set_mac(const uint8_t mac[6])
{
	if (mac == NULL) {
		return;
	}

	memcpy(g_rcp_wifi_mgmt.mac, mac, sizeof(g_rcp_wifi_mgmt.mac));
	g_rcp_wifi_mgmt.mac_valid = true;
}

bool rcp_wifi_get_mac(uint8_t mac[6], uint32_t timeout_ms)
{
	uint32_t start_ms;

	if (mac == NULL) {
		return false;
	}

	start_ms = rcp_os_time_ms();
	while (!g_rcp_wifi_mgmt.mac_valid) {
		if ((rcp_os_time_ms() - start_ms) >= timeout_ms) {
			return false;
		}
		rcp_os_delay_ms(10);
	}

	memcpy(mac, g_rcp_wifi_mgmt.mac, sizeof(g_rcp_wifi_mgmt.mac));
	return true;
}

enum rcp_wifi_status rcp_wifi_deinit(void)
{
	RCP_Object_t *obj = RCP_ObjGet();

	g_rcp_wifi_mgmt.mac_valid = false;
	g_rcp_wifi_mgmt.dhcp_enabled = false;
	g_rcp_wifi_mgmt.event_cb = NULL;
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_DeInit(obj));
}

enum rcp_wifi_status rcp_wifi_init(void)
{
	RCP_Object_t *obj = RCP_ObjGet();
	int ret;

	ret = rcp_tty_init();
	if (ret != RCP_BUS_OK) {
		return (ret == RCP_BUS_ERR_MEMORY) ? RCP_WIFI_ERR_MEM : RCP_WIFI_ERR;
	}

	if (RCP_WiFi_Init(obj) != RCP_STATUS_OK) {
		return RCP_WIFI_ERR;
	}
	g_rcp_wifi_mgmt.dhcp_enabled = 1;

    return rcp_wifi_status_from_rcp_status(RCP_RegisterULcb(obj, rcp_wifi_sta_event_cb, NULL, NULL, NULL, NULL));
}
