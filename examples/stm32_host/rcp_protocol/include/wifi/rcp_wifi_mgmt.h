#ifndef __RCP_WIFI_MGMT_H__
#define __RCP_WIFI_MGMT_H__

#include <stdbool.h>
#include <stdint.h>

#include "rcp_api.h"

#define WIFI_MAX_SSID_SIZE 32
#define WIFI_MAX_PSK_SIZE  64
#define WIFI_BSSID_SIZE    6

#define WIFI_MAX_ERR_MSG_SIZE 50
#define RCP_WIFI_MAX_L2_INTERFACES 2

#define RCP_WIFI_SCAN_SSID_FILT_MAX 8
#define RCP_WIFI_SCAN_CHAN_MAX_MANUAL 10

/* RCP WiFi status code */
enum rcp_wifi_status {
	/* Operation successful */
	RCP_WIFI_OK = 0,
	/* Generic error */
	RCP_WIFI_ERR,
	/* Function or parameter not supported */
	RCP_WIFI_ERR_NOT_SUPP,
	/* Not enough space to perform operation */
	RCP_WIFI_ERR_MEM,
};

/* Convert an RCP driver status to the WiFi management status domain.
 *
 * status - status returned by an RCP_* or RCP_WiFi_* API
 *
 * Returns RCP_WIFI_OK for RCP_STATUS_OK, otherwise RCP_WIFI_ERR.
 */
static inline enum rcp_wifi_status rcp_wifi_status_from_rcp_status(RCP_Status_t status)
{
	return (status == RCP_STATUS_OK) ? RCP_WIFI_OK : RCP_WIFI_ERR;
}

enum rcp_netif_type {
	RCP_NET_IF_TYPE_UNKNOWN = 0,
	RCP_NET_IF_TYPE_WIFI = 1,
	RCP_NET_IF_TYPE_OPENTHREAD = 2,
};

/* WiFi security type */
enum rcp_wifi_sec_type {
	RCP_WIFI_SEC_TYPE_NONE = 0,
	RCP_WIFI_SEC_TYPE_PSK = 1,   // WPA/WPA2 PSK (Zephyr lumps WPA2-PSK here)
	RCP_WIFI_SEC_TYPE_PSK_SHA256 = 2,
	RCP_WIFI_SEC_TYPE_SAE = 3,   // WPA3 SAE
	RCP_WIFI_SEC_TYPE_SAE_HNP = 3, // alias for RCP_WIFI_SEC_TYPE_SAE
	RCP_WIFI_SEC_TYPE_SAE_H2E = 4,
	RCP_WIFI_SEC_TYPE_SAE_AUTO = 5,
	RCP_WIFI_SEC_TYPE_WAPI = 6,
	RCP_WIFI_SEC_TYPE_EAP = 7,   // Enterprise (EAP)
	RCP_WIFI_SEC_TYPE_EAP_TLS = 7, // alias for RCP_WIFI_SEC_TYPE_EAP
	RCP_WIFI_SEC_TYPE_WEP = 8,
	RCP_WIFI_SEC_TYPE_WPA_PSK = 9,
	RCP_WIFI_SEC_TYPE_WPA_AUTO_PERSONAL = 10,
	RCP_WIFI_SEC_TYPE_DPP = 11,
	RCP_WIFI_SEC_TYPE_EAP_PEAP_MSCHAPV2 = 12,
	RCP_WIFI_SEC_TYPE_EAP_PEAP_GTC = 13,
	RCP_WIFI_SEC_TYPE_EAP_TTLS_MSCHAPV2 = 14,
	RCP_WIFI_SEC_TYPE_EAP_PEAP_TLS = 15,
	RCP_WIFI_SEC_TYPE_FT_PSK = 16,
	RCP_WIFI_SEC_TYPE_FT_SAE = 17,
	RCP_WIFI_SEC_TYPE_FT_EAP = 18,
	RCP_WIFI_SEC_TYPE_FT_EAP_SHA384 = 19,
	RCP_WIFI_SEC_TYPE_SAE_EXT_KEY = 20,
	RCP_WIFI_SEC_TYPE_UNKNOWN = 21,
	// Not supported directly by Zephyr.
	RCP_WIFI_SEC_TYPE_OSEN = 22,
	RCP_WIFI_SEC_TYPE_OWE = 23,
	RCP_WIFI_SEC_TYPE_WPA3_EAP_SUITE_B = 24,
};

enum rcp_wifi_wpa_ent_type {
	RCP_WIFI_WPA3_TYPE_NA = 0,
	RCP_WIFI_WPA3_SUITEB = 1,
	RCP_WIFI_WPA3_SUITEB_192 = 2,
	RCP_WIFI_WPA3_PA3_ONLY = 3,
};

enum rcp_wifi_mfp_options {
	RCP_WIFI_MFP_DISABLE = 0,
	RCP_WIFI_MFP_OPTIONAL = 1,
	RCP_WIFI_MFP_REQUIRED = 2,
};

/* WiFi interface state (aligned with Zephyr enum wifi_iface_state). */
enum rcp_wifi_iface_state {
	/* Values aligned with Zephyr's enum wifi_iface_state (include/zephyr/net/wifi.h) */
	RCP_WIFI_STATE_DISCONNECTED = 0,
	RCP_WIFI_STATE_INTERFACE_DISABLED = 1,
	RCP_WIFI_STATE_INACTIVE = 2,
	RCP_WIFI_STATE_SCANNING = 3,
	RCP_WIFI_STATE_AUTHENTICATING = 4,
	RCP_WIFI_STATE_ASSOCIATING = 5,
	RCP_WIFI_STATE_ASSOCIATED = 6,
	RCP_WIFI_STATE_4WAY_HANDSHAKE = 7,
	RCP_WIFI_STATE_GROUP_HANDSHAKE = 8,
	RCP_WIFI_STATE_COMPLETED = 9,
	RCP_WIFI_STATE_UNKNOWN = 11,
};

/* WiFi interface mode (aligned with Zephyr enum wifi_iface_mode). */
enum rcp_wifi_iface_mode {
	/* Values aligned with Zephyr's enum wifi_iface_mode (include/zephyr/net/wifi.h) */
	RCP_WIFI_MODE_INFRA = 0,   /* STA */
	RCP_WIFI_MODE_IBSS = 1,    /* IBSS (ad-hoc) */
	RCP_WIFI_MODE_AP = 2,      /* SoftAP */
	RCP_WIFI_MODE_P2P_GO = 3,
	RCP_WIFI_MODE_P2P_GROUP_FORMATION = 4,
	RCP_WIFI_MODE_MESH = 5,
};

/* WiFi link mode / PHY (aligned with Zephyr enum wifi_link_mode). */
enum rcp_wifi_link_mode {
	/* Values aligned with Zephyr's enum wifi_link_mode (include/zephyr/net/wifi.h) */
	RCP_WIFI_LINK_MODE_WIFI_0 = 0,   /* 802.11 (legacy) */
	RCP_WIFI_LINK_MODE_WIFI_1 = 1,   /* 802.11b */
	RCP_WIFI_LINK_MODE_WIFI_2 = 2,   /* 802.11a */
	RCP_WIFI_LINK_MODE_WIFI_3 = 3,   /* 802.11g */
	RCP_WIFI_LINK_MODE_WIFI_4 = 4,   /* 802.11n (HT) */
	RCP_WIFI_LINK_MODE_WIFI_5 = 5,   /* 802.11ac (VHT) */
	RCP_WIFI_LINK_MODE_WIFI_6 = 6,   /* 802.11ax (HE) */
	RCP_WIFI_LINK_MODE_WIFI_6E = 7,  /* 802.11ax 6GHz (Wi-Fi 6E) */
	RCP_WIFI_LINK_MODE_WIFI_7 = 8,   /* 802.11be (EHT) */
	RCP_WIFI_LINK_MODE_UNKNOWN = 10,
};


/* WiFi frequency band */
enum rcp_wifi_band {
	RCP_WIFI_BAND_2_4_GHZ = 0,
	RCP_WIFI_BAND_5_GHZ = 1,
	RCP_WIFI_BAND_6_GHZ = 2,
	RCP_WIFI_BAND_UNKNOWN = 3,
	RCP_WIFI_BAND_60_GHZ = 4,
	RCP_WIFI_BAND_ANY = 5,
};

/* WiFi frequency bandwidth */
enum rcp_wifi_bw {
	RCP_WIFI_BW_UNKNOWN = 0,
	RCP_WIFI_BW_20MHZ = 1,
	RCP_WIFI_BW_40MHZ = 2,
	RCP_WIFI_BW_80MHZ = 3,
	RCP_WIFI_BW_160MHZ = 4,
	RCP_WIFI_BW_80_80_MHZ = 5,
};

enum rcp_scan_type {
	RCP_WIFI_SCAN_PASSIVE = 0,
	RCP_WIFI_SCAN_ACTIVE = 1,
};

struct rcp_wifi_band_channel {
	enum rcp_wifi_band band;
	uint32_t channel;
};

struct rcp_ssid {
	uint32_t size;
	uint8_t bytes[WIFI_MAX_SSID_SIZE];
};

/* RCP WiFi management events */
enum rcp_wifi_evt {
	RCP_WIFI_CONNECTED,
	RCP_WIFI_DISCONNECTED,
	/* Status response to WiFi commands */
	RCP_WIFI_STATUS_RESP,
	RCP_WIFI_L2_INTERFACES_RESP,
	RCP_WIFI_IFACE_STATUS_RESP,
	RCP_WIFI_SCAN_RESULT,
	RCP_WIFI_SCAN_DONE,
};

/* Command status event data. */
struct rcp_wifi_evt_status {
	bool success;
	int32_t error_code;
	char error_msg[WIFI_MAX_ERR_MSG_SIZE];
};

/* Connection / disconnection event data. */
struct rcp_wifi_evt_conn {
	uint32_t status;
};

/* AP info from scan */
struct rcp_wifi_evt_scan_result {
	struct rcp_ssid ssid;
	int16_t rssi;
	enum rcp_wifi_band band;
	uint32_t channel;
	enum rcp_wifi_wpa_ent_type wpa_ent_type;
	enum rcp_wifi_mfp_options mfp;
	enum rcp_wifi_sec_type sec_type;
	uint8_t bssid[WIFI_BSSID_SIZE];
};

/* WiFi interface status response payload (mirrors Zephyr wifi_iface_status). */
struct rcp_wifi_iface_status {
	enum rcp_wifi_iface_state state;
	enum rcp_wifi_iface_mode iface_mode;
	enum rcp_wifi_link_mode link_mode;

	uint8_t ssid[WIFI_MAX_SSID_SIZE];
	uint8_t ssid_len;
	uint8_t bssid[WIFI_BSSID_SIZE];

	enum rcp_wifi_band band;
	uint32_t channel;
	enum rcp_wifi_wpa_ent_type wpa3_ent_type;
	enum rcp_wifi_sec_type security;
	enum rcp_wifi_mfp_options mfp;

	int32_t rssi;
	uint32_t beacon_interval;
	uint32_t dtim_period;
	bool twt_capable;

	/* TX bitrate in kbps (proto carries kbps; host may present Mbps). */
	uint32_t tx_bitrate_kbps;
};

struct rcp_wifi_evt_iface_status {
	uint32_t iface_index;
	struct rcp_wifi_iface_status status;
};


typedef void (*rcp_wifi_event_callback)(enum rcp_wifi_evt evt, const void *data);

struct rcp_wifi_conn_req {
	uint8_t ssid[WIFI_MAX_SSID_SIZE];
	uint8_t ssid_len;
	enum rcp_wifi_sec_type sec_type;
	enum rcp_wifi_mfp_options pmf;
	uint32_t channel;
	uint8_t psk[WIFI_MAX_PSK_SIZE];
	uint8_t psk_len;
	enum rcp_wifi_band freq_band;
	enum rcp_wifi_bw freq_bw;
	uint32_t timeout_ms;
	uint8_t bssid[WIFI_BSSID_SIZE];
	uint8_t bssid_len;
    bool    dhcp;
};

/* L2 interfaces response data. */
struct rcp_wifi_l2_interface {
	uint32_t index;
	enum rcp_netif_type net_if_type;
	uint8_t mac_address[WIFI_BSSID_SIZE];
	bool offload_supported;
	uint32_t mtu;
};


struct rcp_wifi_l2_interfaces {
	uint32_t count;
	struct rcp_wifi_l2_interface net_ifs[RCP_WIFI_MAX_L2_INTERFACES];
};

struct rcp_wifi_scan_params {
	enum rcp_scan_type scan_type;
	uint32_t dwell_time_active;
	uint32_t dwell_time_passive;
	uint32_t bands_bitmap;
	uint16_t ssids_count;
	struct rcp_ssid ssids[RCP_WIFI_SCAN_SSID_FILT_MAX];
	int32_t max_bss_cnt;
    uint16_t band_chan_count;
    struct rcp_wifi_band_channel band_chan[RCP_WIFI_SCAN_CHAN_MAX_MANUAL];
};

typedef RCP_WiFi_Connect_Opts_t rcp_wifi_connect_opts_t;
typedef RCP_WiFi_Scan_Opts_t rcp_wifi_scan_opts_t;
typedef RCP_WiFi_Scan_Result_cb_t rcp_wifi_scan_result_cb_t;
typedef RCP_WiFi_SecurityType_e rcp_wifi_security_type_t;
typedef RCP_WiFi_Protocol_e rcp_wifi_protocol_t;
typedef RCP_WiFi_ApConfig_t rcp_wifi_ap_config_t;
typedef RCP_WiFi_Connected_Sta_t rcp_wifi_connected_sta_t;
typedef RCP_WiFi_TWT_Setup_Params_t rcp_wifi_twt_setup_params_t;
typedef RCP_WiFi_TWT_Status_t rcp_wifi_twt_status_t;
typedef RCP_WiFi_TWT_Teardown_Params_t rcp_wifi_twt_teardown_params_t;
typedef RCP_WiFi_AntennaMode_e rcp_wifi_antenna_mode_t;

/* Initialize the RCP WiFi management subsystem.
 *
 * Initializes the TTY transport, initializes the RCP WiFi AT layer, and
 * registers the WiFi event callback used by this management layer.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_init(void);

/* Deinitialize the RCP WiFi management subsystem.
 *
 * Clears local management state and deinitializes the RCP WiFi AT layer.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_deinit(void);

/* Register RCP WiFi event callback.
 *
 * evt_cb - event callback function
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_register_callback(rcp_wifi_event_callback evt_cb);

/* Connect to a WiFi network.
 *
 * req - pointer to connect request parameters
 *
 * Returns RCP WiFi status code.
 *
 * Specific error codes:
 * - RCP_WIFI_ERR_NOT_SUPP - one or more request parameters not supported by the device
 */
enum rcp_wifi_status rcp_wifi_connect(struct rcp_wifi_conn_req *req);

/* Disconnect from the current WiFi network without clearing saved credentials.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_disconnect(void);

/* Disconnect from the current WiFi network.
 *
 * restore - nonzero to remove saved connection information on the device
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_disconnect_restore(uint32_t restore);

/* Query L2 interfaces exposed by the RCP (inventory).
 *
 * net_if_type - filter (RCP_NET_IF_TYPE_UNKNOWN => all)
 * index       - filter (0 => any)
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_l2_interfaces(enum rcp_netif_type net_if_type, uint32_t index);

/* Request WiFi scan.
 *
 * scan_opts      - scan options consumed by the RCP WiFi AT layer
 * scan_result_cb - callback invoked on scan done with the scan status and result list
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_scan(rcp_wifi_scan_opts_t *scan_opts,
				   rcp_wifi_scan_result_cb_t scan_result_cb);

/* Request WiFi interface status.
 *
 * if_index - interface index to query
 *
 * Posts RCP_WIFI_IFACE_STATUS_RESP through the registered callback on success.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_iface_status(uint32_t if_index);

/* Start station mode.
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_station_start(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_Station_Start(RCP_ObjGet()));
}

/* Enable or disable station auto-connect.
 *
 * onoff - 0 disables auto-connect, 1 enables auto-connect
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_auto_connect(uint32_t onoff)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_SetAutoConnect(RCP_ObjGet(), onoff));
}

/* Read station auto-connect state.
 *
 * onoff - output pointer receiving 0 or 1
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_get_auto_connect(uint32_t *onoff)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_GetAutoConnect(RCP_ObjGet(), onoff));
}

/* Set raw RCP WiFi scan options.
 *
 * scan_opts - scan option structure consumed by the RCP WiFi AT layer
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_scan_opts(rcp_wifi_scan_opts_t *scan_opts)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_SetScanOpts(RCP_ObjGet(), scan_opts));
}

/* Start a scan using the current raw RCP WiFi scan options.
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_scan_start(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_Scan(RCP_ObjGet()));
}

/* Set raw RCP WiFi reconnection options.
 *
 * connect_opts - connection options containing reconnection interval and count
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_reconnection_opts(rcp_wifi_connect_opts_t *connect_opts)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetReconnectionOpts(RCP_ObjGet(), connect_opts));
}

/* Connect using raw RCP WiFi connection options.
 *
 * connect_opts - raw RCP WiFi connection options
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_connect_opts(rcp_wifi_connect_opts_t *connect_opts)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_Connect(RCP_ObjGet(), connect_opts));
}

/* Query current station connection information.
 *
 * rssi     - output pointer receiving RSSI
 * security - output pointer receiving security mode
 * protocol - output pointer receiving WiFi protocol mode
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_get_connect_info(int32_t *rssi,
                                                             rcp_wifi_security_type_t *security,
                                                             rcp_wifi_protocol_t *protocol)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetConnectInfo(RCP_ObjGet(), rssi, security, protocol));
}

/* Query the station MAC address.
 *
 * mac - output buffer for the 6-byte station MAC address
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_station_get_mac_address(uint8_t *mac)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_Station_GetMACAddress(RCP_ObjGet(), mac));
}

/* Query the configured country code.
 *
 * policy         - output pointer receiving country policy
 * country_string - output buffer receiving the two-letter country code
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_get_country_code(uint32_t *policy,
                                                             char *country_string)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetCountryCode(RCP_ObjGet(), policy, country_string));
}

/* Set the country code.
 *
 * policy         - pointer to country policy value
 * country_string - two-letter country code string
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_country_code(uint32_t *policy,
                                                             char *country_string)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_SetCountryCode(RCP_ObjGet(), policy, country_string));
}

/* Enable station plus Soft-AP dual mode.
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_dual_mode(void)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_SetDualMode(RCP_ObjGet()));
}

/* Configure and start Soft-AP mode.
 *
 * ap_config - Soft-AP configuration
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_ap_start(rcp_wifi_ap_config_t *ap_config);

/* Stop Soft-AP mode.
 *
 * reconnect - nonzero to reconnect the station if it was previously connected
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_ap_stop(uint8_t reconnect);

/* Query Soft-AP configuration.
 *
 * ap_config - output Soft-AP configuration
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_ap_get_config(rcp_wifi_ap_config_t *ap_config)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_AP_GetConfig(RCP_ObjGet(), ap_config));
}

/* List stations connected to the Soft-AP.
 *
 * stations - output connected-station list
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_ap_list_connected_stations(rcp_wifi_connected_sta_t *stations)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_AP_ListConnectedStations(RCP_ObjGet(), stations));
}

/* Disconnect a station from the Soft-AP.
 *
 * mac - 6-byte MAC address of the station to disconnect
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_ap_disconnect_station(uint8_t *mac)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_AP_DisconnectStation(RCP_ObjGet(), mac));
}

/* Query Soft-AP protocol mode.
 *
 * protocol - output pointer receiving the Soft-AP protocol mode
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_ap_get_mode(rcp_wifi_protocol_t *protocol)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_AP_GetMode(RCP_ObjGet(), protocol));
}

/* Set Soft-AP protocol mode.
 *
 * protocol - Soft-AP protocol mode
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_ap_set_mode(rcp_wifi_protocol_t protocol)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_AP_SetMode(RCP_ObjGet(), protocol));
}

/* Query the Soft-AP MAC address.
 *
 * mac - output buffer for the 6-byte Soft-AP MAC address
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_ap_get_mac_address(uint8_t *mac)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_AP_GetMACAddress(RCP_ObjGet(), mac));
}

/* Set DTIM period.
 *
 * dtim - DTIM period to configure
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_dtim(uint32_t dtim)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_SetDTIM(RCP_ObjGet(), dtim));
}

/* Configure WiFi low-power mode.
 *
 * ps_mode   - power save mode
 * hbn_level - hibernate wakeup source selection
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_set_power_mode(uint32_t ps_mode, uint32_t hbn_level);

/* Query cached WiFi low-power mode.
 *
 * ps_mode - output pointer receiving cached power mode
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_power_mode(uint32_t *ps_mode);

/* Set wake-up GPIO.
 *
 * wakeup_pin - wake-up GPIO index
 * mode       - wake-up GPIO trigger mode
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_set_wakeup_pin(uint32_t wakeup_pin, uint32_t mode);

/* Clear configured DTIM period.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_clear_dtim(void);

/* Query the AP DTIM period.
 *
 * dtim - output pointer receiving DTIM period
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_get_dtim_ap(uint32_t *dtim)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_GetDTIM_AP(RCP_ObjGet(), dtim));
}

/* Delete a wake-up GPIO configuration.
 *
 * wakeup_pin - wake-up GPIO index
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_delete_wakeup_pin(uint32_t wakeup_pin);

/* Configure wake-up timer.
 *
 * mode       - wake-up timer mode
 * timeout_ms - wake-up timeout in milliseconds
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_set_wakeup_timer(uint32_t mode, uint32_t timeout_ms);

/* Start low-power keepalive timer.
 *
 * period - keepalive period
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_start_keepalive(uint32_t period);

/* Stop low-power keepalive timer.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_stop_keepalive(void);

/* Start low-power ARP announcement timer.
 *
 * period - ARP announcement period
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_start_arp(uint32_t period);

/* Stop low-power ARP announcement timer.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_stop_arp(void);

/* Set module clock source.
 *
 * source - clock source value
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_set_clock_source(uint32_t source);

/* Query module clock source.
 *
 * source - output pointer receiving clock source
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_clock_source(uint32_t *source);

/* Set up TWT power-save parameters.
 *
 * twt_params - TWT setup parameters
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_twt_setup(rcp_wifi_twt_setup_params_t *twt_params)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_TWT_Setup(RCP_ObjGet(), twt_params));
}

/* Enter TWT sleep.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_twt_sleep(void);

/* Query current TWT status.
 *
 * twt_status - output TWT status
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_twt_get_status(rcp_wifi_twt_status_t *twt_status)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_TWT_GetStatus(RCP_ObjGet(), twt_status));
}

/* Query whether TWT is supported.
 *
 * is_supported - output pointer receiving 0 or 1
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_twt_is_supported(uint32_t *is_supported)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_TWT_IsSupported(RCP_ObjGet(), is_supported));
}

/* Tear down TWT power-save flows.
 *
 * twt_params - TWT teardown parameters
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_twt_teardown(rcp_wifi_twt_teardown_params_t *twt_params)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_TWT_Teardown(RCP_ObjGet(), twt_params));
}

/* Clear device power statistics.
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_clear_power_stats(void);

/* Query device average power statistics.
 *
 * power - output pointer receiving average power value
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_power_stats(uint64_t *power);

/* Query low-power interval.
 *
 * interval - output pointer receiving low-power interval
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_lp_interval(uint32_t *interval);

/* Set coexistence duty active time.
 *
 * active_ms - active time in milliseconds
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_set_coex_duty(uint32_t active_ms);

/* Query coexistence duty active time.
 *
 * active_ms - output pointer receiving active time in milliseconds
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_coex_duty(uint32_t *active_ms);

/* Enable or disable coexistence power-save.
 *
 * enable - 0 disables, 1 enables
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_set_coex_enable(uint32_t enable);

/* Query coexistence power-save enable state.
 *
 * enable - output pointer receiving enable state
 *
 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_coex_enable(uint32_t *enable);

/* Enable antenna diversity mode.
 *
 * mode - antenna mode to configure
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_set_antenna_enable(rcp_wifi_antenna_mode_t mode)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_SetAntennaEnable(RCP_ObjGet(), mode));
}

/* Query antenna diversity mode.
 *
 * mode - output pointer receiving antenna mode
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_get_antenna_enable(rcp_wifi_antenna_mode_t *mode)
{
	return rcp_wifi_status_from_rcp_status(RCP_WiFi_GetAntennaEnable(RCP_ObjGet(), mode));
}

/* Query the currently used antenna.
 *
 * antenna_id - output pointer receiving antenna ID
 *
 * Returns RCP WiFi status code.
 */
static inline enum rcp_wifi_status rcp_wifi_get_antenna_used(uint32_t *antenna_id)
{
	return rcp_wifi_status_from_rcp_status(
		RCP_WiFi_GetAntennaUsed(RCP_ObjGet(), antenna_id));
}

/* Cache the MAC address learned from the device data path.
 *
 * mac - 6-byte MAC address to cache
 */
void rcp_wifi_set_mac(const uint8_t mac[6]);

/* Wait until a valid MAC address is available.
 *
 * mac        - output buffer for the 6-byte cached MAC address
 * timeout_ms - timeout in milliseconds
 *
 * Returns true if a MAC address was copied, false on timeout or invalid input.
 */
bool rcp_wifi_get_mac(uint8_t mac[6], uint32_t timeout_ms);
#endif
