/* rcp_wifi_mgmt.h
 *
 * RCP WiFi management module.
 *
 * WiFi management module for Qualcomm RCP MCU.
 * This module aims at providing Qualcomm RCP WiFi management functionality
 * for platforms without own WiFi management frameworks (e.g. STM32 applications
 * with lwIP networking).
 *
 * For certain host platforms a dedicated driver (if available) will be a better choice.
 * For example:
 * - Linux OS - RCP cfg80211 driver,
 * - Zephyr RTOS - RCP WiFi device driver.
 */
#ifndef __RCP_WIFI_MGMT_H__
#define __RCP_WIFI_MGMT_H__

#include <stdbool.h>
#include <stdint.h>

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
	uint8_t rssi;
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
typedef void (*rcp_wifi_tty_forwarder_t)(const uint8_t *data, uint16_t len, void *arg);

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

/* Initialize RCP WiFi management subsystem.
 *
 * Returns RCP WiFi status code
 */
enum rcp_wifi_status rcp_wifi_init(void);

/* Register RCP WiFi event callback.
 *
 * evt_cb - event callback function
 *
 * Returns RCP WiFi status code
 *
 * Specific error codes:
 * - RCP_WIFI_ERR_NO_MEM - too many callbacks registered
 */
enum rcp_wifi_status rcp_wifi_register_callback(rcp_wifi_event_callback evt_cb);

/* Connect to WiFi network
 *
 * req - pointer to connect request parameters
 *
 * Returns RCP WiFi status code.
 *
 * Specific error codes:
 * - RCP_WIFI_ERR_NOT_SUPP - one or more request parameters not supported by the device
 */
enum rcp_wifi_status rcp_wifi_connect(struct rcp_wifi_conn_req *req);

/* Disconnect from WiFi network
 *
 * req - pointer to connect request parameters
 *
 * Returns RCP WiFi status code.
 *
 * Specific error codes:
 * - RCP_WIFI_ERR_NOT_SUPP - one or more request parameters not supported by the device
 */
enum rcp_wifi_status rcp_wifi_disconnect(void);

/* Query L2 interfaces exposed by the RCP (inventory).
 *
 * net_if_type - filter (RCP_NET_IF_TYPE_UNKNOWN => all)
 * index       - filter (0 => any)

 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_get_l2_interfaces(enum rcp_netif_type net_if_type, uint32_t index);

/* Request WiFi scan.
 *
 * if_index    - Indicates which interface to scan on 
 * params      - Criteria for scan

 * Returns RCP WiFi status code.
 */
enum rcp_wifi_status rcp_wifi_scan(uint32_t if_index, struct rcp_wifi_scan_params *params);

/* Request WiFi interface status (STA/AP state, RSSI, rates, etc.). */
enum rcp_wifi_status rcp_wifi_get_iface_status(uint32_t if_index);

/* Cache the MAC address learned from the device data path. */
void rcp_wifi_set_mac(const uint8_t mac[6]);

/* Wait until a valid MAC address is available. */
bool rcp_wifi_get_mac(uint8_t mac[6], uint32_t timeout_ms);
#endif
