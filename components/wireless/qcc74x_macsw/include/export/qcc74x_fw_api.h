#ifndef __QCC74x_FW_API_H__
#define __QCC74x_FW_API_H__

/*--------------------------------------------------------------------*/
/* Status Codes - these codes are used in qcc74x fw actions      */
/* when an error action is taking place the status code can indicate  */
/* what the status code.                                              */
/* It is defined by qcc74xalb                                       */
/*--------------------------------------------------------------------*/
#define WLAN_FW_SUCCESSFUL                                        0
#define WLAN_FW_TX_AUTH_FRAME_ALLOCATE_FAIILURE                   1 
#define WLAN_FW_AUTHENTICATION_FAIILURE                           2
#define WLAN_FW_AUTH_ALGO_FAIILURE                                3
#define WLAN_FW_TX_ASSOC_FRAME_ALLOCATE_FAIILURE                  4 
#define WLAN_FW_ASSOCIATE_FAIILURE                                5
#define WLAN_FW_DEAUTH_BY_AP_WHEN_NOT_CONNECTION                  6
#define WLAN_FW_DEAUTH_BY_AP_WHEN_CONNECTION                      7
#define WLAN_FW_4WAY_HANDSHAKE_ERROR_PSK_TIMEOUT_FAILURE          8
#define WLAN_FW_4WAY_HANDSHAKE_TX_DEAUTH_FRAME_TRANSMIT_FAILURE   9
#define WLAN_FW_4WAY_HANDSHAKE_TX_DEAUTH_FRAME_ALLOCATE_FAIILURE 10
#define WLAN_FW_AUTH_OR_ASSOC_RESPONSE_TIMEOUT_FAILURE           11
#define WLAN_FW_SCAN_NO_BSSID_AND_CHANNEL                        12
#define WLAN_FW_CREATE_CHANNEL_CTX_FAILURE_WHEN_JOIN_NETWORK     13
#define WLAN_FW_JOIN_NETWORK_FAILURE                             14
#define WLAN_FW_ADD_STA_FAILURE                                  15
#define WLAN_FW_BEACON_LOSS                                      16
#define WLAN_FW_NETWORK_SECURITY_NOMATCH                         17
#define WLAN_FW_NETWORK_WEPLEN_ERROR                             18
#define WLAN_FW_DISCONNECT_BY_USER_WITH_DEAUTH                   19
#define WLAN_FW_DISCONNECT_BY_USER_NO_DEAUTH                     20
#define WLAN_FW_DISCONNECT_BY_FW_PS_TX_NULLFRAME_FAILURE         21
#define WLAN_FW_TRAFFIC_LOSS                                     22
#define WLAN_FW_SWITCH_CHANNEL_FAILURE                           23
#define WLAN_FW_AUTH_OR_ASSOC_RESPONSE_CFM_FAILURE               24
#define WLAN_FW_REASSOCIATE_STARING                              25

/*--------------------------------------------------------------------*/
/* AP Mode Status Codes - these codes are used in qcc74x fw actions      */
/* when an error action is taking place the status code can indicate  */
/* what the status code.                                              */
/* It is defined by qcc74xalb                                       */
/*--------------------------------------------------------------------*/
#define WLAN_FW_APM_SUCCESSFUL                                        0 
#define WLAN_FW_APM_DELETESTA_BY_USER                                 1
#define WLAN_FW_APM_DEATUH_BY_STA                                     2
#define WLAN_FW_APM_DISASSOCIATE_BY_STA                               3
#define WLAN_FW_APM_DELETECONNECTION_TIMEOUT                          4
#define WLAN_FW_APM_DELETESTA_FOR_NEW_CONNECTION                      5
#define WLAN_FW_APM_DEAUTH_BY_AUTHENTICATOR                           6

#define WLAN_CODE_NOTUSED 0xFFFF

#define WIFI_EVENT_BEACON_IND_AUTH_OPEN            0
#define WIFI_EVENT_BEACON_IND_AUTH_WEP             1
#define WIFI_EVENT_BEACON_IND_AUTH_WPA_PSK         2
#define WIFI_EVENT_BEACON_IND_AUTH_WPA2_PSK        3
#define WIFI_EVENT_BEACON_IND_AUTH_WPA_WPA2_PSK    4
#define WIFI_EVENT_BEACON_IND_AUTH_WPA_ENT         5
#define WIFI_EVENT_BEACON_IND_AUTH_WPA3_SAE        6
#define WIFI_EVENT_BEACON_IND_AUTH_WPA2_PSK_WPA3_SAE 7
#define WIFI_EVENT_BEACON_IND_AUTH_UNKNOWN      0xff

#define WIFI_EVENT_BEACON_IND_CIPHER_NONE           0
#define WIFI_EVENT_BEACON_IND_CIPHER_WEP            1
#define WIFI_EVENT_BEACON_IND_CIPHER_AES            2
#define WIFI_EVENT_BEACON_IND_CIPHER_TKIP           3
#define WIFI_EVENT_BEACON_IND_CIPHER_TKIP_AES       4

#define WIFI_WPA_PROTO_WPA (0)
#define WIFI_WPA_PROTO_RSN (1)

/*Wi-Fi Firmware Entry*/
void wifi_main(void *param);

void qcc74x_tpc_update_power_table(int8_t *power_table);
void qcc74x_tpc_power_table_get(int8_t *power_table);
/**
 ****************************************************************************************
 * @brief Set keep alive time for sta mode 
 *
 * @param[in] the keep alive time for sta mode(unit: seconds) 
 ****************************************************************************************
 */
void qcc74x_sta_set_keepalive_period(uint8_t time_seconds);
int qcc74x_wifi_sta_ps_active_ms(uint16_t active_ms);
int twt_get_flow_num(void);


#endif /*__QCC74x_FW_API_H__*/
