#ifndef _EXPORT_MACSW_H_
#define _EXPORT_MACSW_H_
#include "stdio.h"
#include "check_cfg.h"
#include "rwnx_config.h"
#include "export/common/co_list.h"
#include "export/common/co_status.h"
#include "phy.h"
#include "llc.h"
#include "export/ke/ke_task.h"

#include "export_struct.h"

#define struct_func_alloc_def(name)                                             \
    void *name##_alloc();                                                       \

#include "export_malloc_list.h"
#undef struct_func_alloc_def

void *vif_info_get_vif(int index);
uint8_t mac_vif_get_sta_ap_id(void *macif);
uint16_t mac_vif_get_bcn_int(void *macvif);
void mac_vif_get_channel(void *macvif, struct mac_chan_op* chan);
void mac_vif_get_sta_status(void *macvif, struct mac_addr *bssid, uint16_t *aid, int8_t *rssi);
void mac_vif_set_sta_rssi(void *macif, int8_t rssi);
struct co_list *mac_vif_get_sta_list(void *macif);
int mac_vif_get_key_info_len(void *macif);
struct key_info_tag *mac_vif_get_key(void *macif, int key_idx);
uint8_t mac_vif_get_index(void *macif);
enum mac_vif_type mac_vif_get_type(void *macif);
bool mac_vif_get_active(void *macif);

uint8_t sta_get_staid(void *p);
void *sta_getp_mac_addr(void *p);
void *sta_get_list_next(void *p);
uint32_t sta_idx_get_last_active(uint8_t sta_idx);
void *sta_idx_getp_mac_addr(uint8_t sta_idx);
uint8_t sta_idx_get_mlink_idx(uint8_t sta_idx);
uint8_t sta_idx_get_key_idx(uint8_t sta_idx);
void *sta_idx_getp_key_info(uint8_t sta_idx);
bool sta_idx_get_valid(uint8_t sta_idx);
uint8_t sta_idx_get_inst_nbr(uint8_t sta_idx);
uint8_t sta_idx_get_uapsd_queues(uint8_t sta_idx);

uint8_t export_rc_get_format_mod(uint32_t rate_config);
uint8_t export_rc_get_mcs_index(uint32_t rate_config);
uint8_t export_rc_get_nss(uint32_t rate_config);
uint8_t export_rc_get_bw(uint32_t rate_config);
uint8_t export_rc_get_pre_type(uint32_t rate_config);
uint8_t export_rc_get_sgi(uint32_t rate_config);

int8_t export_hal_desc_get_rssi(void *rx_vec_1, int8_t *rx_rssi);
uint16_t export_hal_desc_get_ht_length(void *rx_vec_1);
uint8_t export_MM_STA_TO_KEY(uint8_t sta_idx);
uint8_t export_MM_MLINK_TO_MFP_KEY(uint8_t key_idx, uint8_t mlink_idx);
uint8_t export_MM_MLINK_TO_KEY(uint8_t key_idx, uint8_t mlink_idx);
uint8_t export_MM_VIF_TO_KEY(uint8_t key_idx, uint8_t vif_idx);
uint8_t export_MM_VIF_TO_MFP_KEY(uint8_t key_idx, uint8_t vif_idx);
void me_env_set_ps_mode(int mode);
int get_time_SINCE_EPOCH(uint32_t *sec, uint32_t *usec);
int get_time_SINCE_BOOT(uint32_t *sec, uint32_t *usec);

struct fhost_tx_desc_tag;
struct txdesc;
bool inline_hal_machw_he_support(void);
uint32_t inline_hal_machw_time(void);
bool inline_hal_machw_time_cmp(uint32_t time1, uint32_t time2);
bool inline_hal_machw_time_past(uint32_t time);
int get_nx_txdesc_cnt(int i);
uint8_t inline_nxmac_tkip_getf();
uint8_t inline_nxmac_ccmp_getf();
uint8_t inline_nxmac_gcmp_getf();

// wifi statistics functions
uint8_t export_stats_get_tx_mcs();
uint8_t export_stats_get_rx_mcs();
char* export_stats_get_rx_format();
char* export_stats_get_tx_format();

// redefinition
typedef void (*cb_timer_func_ptr)(void *);
struct mm_timer_tag
{
    /// List element for chaining
    struct co_list_hdr list_hdr;
    /// Function to be called upon timer expiry
    cb_timer_func_ptr cb;
    /// Pointer to be passed to the callback
    void *env;
    /// Absolute expiry time
    uint32_t time;
};
struct me_chan_config_req
{
    /// List of 2.4GHz supported channels
    struct mac_chan_def chan2G4[MAC_DOMAINCHANNEL_24G_MAX];
    /// List of 5GHz supported channels
    struct mac_chan_def chan5G[MAC_DOMAINCHANNEL_5G_MAX];
    /// Number of 2.4GHz channels in the list
    uint8_t chan2G4_cnt;
    /// Number of 5GHz channels in the list
    uint8_t chan5G_cnt;
};
struct rxu_stat_val
{
    /// Host Buffer Address
    uint32_t host_id;
    /// Length
    uint32_t frame_len;
    /// Status (@ref rx_status_bits)
    uint16_t status;
};

void mm_timer_set(struct mm_timer_tag *timer, uint32_t value);
void mm_timer_clear(struct mm_timer_tag *timer);
void tx_txdesc_release(struct txdesc *txdesc);
void me_init_chan(struct mac_chan_op *chan);
uint8_t vif_mgmt_get_staid(void *vif, const struct mac_addr *sta_addr);
uint32_t dbg_snprintf(char *buffer, uint32_t size, const char *fmt, ...);
int macif_kmsg_push(int msg_id, int task_id, void *param, int param_len);
int macif_kmsg_push_t(int msg_id, int task_id, int nb_msg, ...);
void macif_rx_buf_ind(void);
void macif_tx_data_ind(int queue_idx);
void macif_coex_tx_pause_set(uint8_t vif_index, bool tx_paused);
bool macif_coex_tx_is_paused(void);
void time_init(uint32_t sec, uint32_t usec);

bool mfp_is_robust_frame(uint16_t frame_cntl, uint8_t action);

#define INVALID_VIF_IDX 0xFF
#define LLC_ETHER_MTU                1500
#define LLC_ETHER_HDR_LEN            14
#define LLC_LLC_LEN                  3
#define LLC_SNAP_LEN                 5
#define LLC_802_2_HDR_LEN            (LLC_LLC_LEN + LLC_SNAP_LEN)
#ifdef CFG_ADHOC_ENABLE
#define WLAN_802_11_MTU                   2304
#define RX_MAX_AMSDU_SUBFRAME_LEN (WLAN_802_11_MTU + LLC_ETHER_HDR_LEN + LLC_802_2_HDR_LEN)
#else
#define RX_MAX_AMSDU_SUBFRAME_LEN (LLC_ETHER_MTU + LLC_ETHER_HDR_LEN + LLC_802_2_HDR_LEN)
#endif
#define TX_PBD_CNT            5
#define BCN_MAX_CSA_CPT 2
#define SCAN_SSID_MAX   2
#define SCAN_CHANNEL_MAX (MAC_DOMAINCHANNEL_24G_MAX + MAC_DOMAINCHANNEL_5G_MAX)
#define RC_DCM_MOD_OFT           14
#define RC_DCM_MOD_MASK          CO_BIT(RC_DCM_MOD_OFT)
#define RX_FLAGS_IS_AMSDU_BIT         CO_BIT(0)
#define RX_FLAGS_IS_MPDU_BIT          CO_BIT(1)
#define RX_FLAGS_4_ADDR_BIT           CO_BIT(2)
#define RX_FLAGS_NEW_MESH_PEER_BIT    CO_BIT(3)
#define RX_FLAGS_NON_MSDU_MSK        (RX_FLAGS_IS_MPDU_BIT)
#define RX_FLAGS_VIF_INDEX_OFT  8
#define RX_FLAGS_VIF_INDEX_MSK  (0xFF << RX_FLAGS_VIF_INDEX_OFT)
#define RX_FLAGS_DST_INDEX_OFT  24
#define RX_FLAGS_DST_INDEX_MSK  (0xFF << RX_FLAGS_DST_INDEX_OFT)
#define TXU_CNTRL_RETRY         CO_BIT(0)
#define TXU_CNTRL_UNDER_BA      CO_BIT(1)
#define TXU_CNTRL_MORE_DATA     CO_BIT(2)
#define TXU_CNTRL_MGMT          CO_BIT(3)
#define TXU_CNTRL_MGMT_NO_CCK   CO_BIT(4)
#define TXU_CNTRL_MGMT_ROBUST   CO_BIT(7)
#define TXU_CNTRL_EOSP          CO_BIT(9)
#define TXU_CNTRL_REUSE_SN      CO_BIT(15)
#define TX_STATUS_DONE              CO_BIT(0)
#define TX_STATUS_RETRY_REQUIRED    CO_BIT(1)
#define TX_STATUS_SW_RETRY_REQUIRED CO_BIT(2)
#define TX_STATUS_ACKNOWLEDGED      CO_BIT(3)
#define TX_STATUS_SN_OFT            4
#define TX_STATUS_SN_MSK            (0xFFF << (TX_STATUS_SN_OFT))
#define FORMATMOD_NON_HT          0
#define FORMATMOD_NON_HT_DUP_OFDM 1
#define FORMATMOD_HT_MF           2
#define FORMATMOD_HT_GF           3
#define FORMATMOD_VHT             4
#define FORMATMOD_HE_SU           5
#define FORMATMOD_HE_MU           6
#define FORMATMOD_HE_ER           7

struct fhost_statistic {
    //TX
    //AMPDU related
    uint32_t tx_ampdu_retry_limit_count;
    uint32_t tx_ampdu_lft_expired_count;
    uint32_t tx_ampdu_sw_retry;
    uint32_t tx_ampdu_unvalid_ba;
    uint32_t tx_ampdu_agg_force_finish;
    uint32_t tx_ampdu_rts_drop_to_cts_self;
};
extern struct fhost_statistic fhost_statistic;

enum ME_PARAM_ID_E {
    ME_PARAM_ID_TX_SINGLE_RETRY_CNT_LIMIT,
    ME_PARAM_ID_TX_AMPDU_RETRY_CNT_LIMIT,
    ME_PARAM_ID_TX_AMPDU_PROTECT_ENABLE,
    ME_PARAM_ID_TX_AMPDU_DROP_TO_SINGLETON_RETRYCNT_THRESHOLD,
    ME_PARAM_ID_TX_POWER,
    /// Hard upper-limit power table; caps the final per-rate TX power
    /// regardless of whether it comes from ME_PARAM_ID_TX_POWER or the
    /// regulatory/calibration table.
    ME_PARAM_ID_TX_POWER_LIMIT,
    ME_PARAM_ID_MAX,
};

enum ME_PARAM_CMD_E {
    ME_PARAM_CMD_SET,
    ME_PARAM_CMD_GET,
    /// Revoke a previously SET value and fall back to the default/regulatory
    /// behavior. Currently only meaningful for ME_PARAM_ID_TX_POWER.
    ME_PARAM_CMD_CLEAR,
};

/// Structure containing the parameters of the @ref ME_PARAM_REQ message
struct me_param_req
{
    /// Indicates the param ID
    enum ME_PARAM_ID_E id;
    enum ME_PARAM_CMD_E cmd;//GET or SET.
    /// payload of the param. Max is 32 Bytes
    uint8_t value[50];//GET or SET through value
};
/// Pointer to callback function
typedef void (*cb_raw_send_wait_end_ptr)(void* env);

/// Pointer to callback function for adhoc tx
typedef void (*cb_adhoc_tx_cfm_ptr)(void* env, uint32_t status);

/// Structure containing the parameters of the @ref MM_RAW_SEND_STRAT_REQ message.
struct mm_raw_send_start_req
{
    /// List of waiting raw send req
    struct co_list_hdr list_hdr;
    /// Raw send wait RX response timer
    struct mm_timer_tag wait_rx;
    /// Channel information
    struct mac_chan_op chan;
    /// Index of the VIF
    int vif_idx;
    /// Pakcet buffer
    void *pkt;
    /// Packet len
    uint32_t len;
    /// Packet buffer valid for queued ad-hoc raw send
    bool pkt_valid;
    /// Packet buffer copied for queued ad-hoc raw send
    uint8_t pkt_buf[NX_TXFRAME_LEN];
    /// Duration in ms, default 20ms
    uint32_t duration;
    /// Raw Send wait end callback
    cb_raw_send_wait_end_ptr cb;
    /// event env
    void *env;
    /// whether need wait resp
    int need_rx;
    /// Ad-hoc mode
    bool adhoc;
    /// Receiver address valid
    bool ra_valid;
    /// Receiver address
    struct mac_addr ra;
    /// Transmitter address valid
    bool ta_valid;
    /// Transmitter address
    struct mac_addr ta;
    /// Tx rate
    uint8_t rate;
    /// RTS thrshold
    uint16_t rts_thrshold;
    /// Tx retry limit
    uint8_t retry_limit;
    /// Tx power
    int8_t tx_power;
    /// cb func for adhoc tx
    cb_adhoc_tx_cfm_ptr cb_cfm;

};

//THD STATINFO fields
//----------------------------------------------------------------------------------------
/// Number of RTS frame retries offset
#define NUM_RTS_RETRIES_OFT                0
/// Number of RTS frame retries mask
#define NUM_RTS_RETRIES_MSK               (0xFF << NUM_RTS_RETRIES_OFT)
/// Number of MPDU frame retries offset
#define NUM_MPDU_RETRIES_OFT               8
/// Number of MPDU frame retries mask
#define NUM_MPDU_RETRIES_MSK              (0xFF << NUM_MPDU_RETRIES_OFT)
/// Retry limit reached: frame unsuccessful
#define RETRY_LIMIT_REACHED_BIT            CO_BIT(16)
/// Frame lifetime expired: frame unsuccessful
#define LIFETIME_EXPIRED_BIT               CO_BIT(17)
/// BA frame not received - valid only for MPDUs part of AMPDU
#define BA_FRAME_RECEIVED_BIT              CO_BIT(18)
/// Frame was transmitted in a HE TB PPDU - Set by SW
#define HE_TB_TX_BIT                       CO_BIT(22)
/// Frame successful by TX DMA: Ack received successfully
#define FRAME_SUCCESSFUL_TX_BIT            CO_BIT(23)
/// Last MPDU of an A-MPDU
#define A_MPDU_LAST                        (0x0F << 26)
/// Transmission bandwidth offset
#define BW_TX_OFT                          24
/// Transmission bandwidth mask
#define BW_TX_MSK                          (0x3 << BW_TX_OFT)
/// Transmission bandwidth - 20MHz
#define BW_20MHZ_TX                        (0x0 << BW_TX_OFT)
/// Transmission bandwidth - 40MHz
#define BW_40MHZ_TX                        (0x1 << BW_TX_OFT)
/// Transmission bandwidth - 80MHz
#define BW_80MHZ_TX                        (0x2 << BW_TX_OFT)
/// Transmission bandwidth - 160MHz
#define BW_160MHZ_TX                       (0x3 << BW_TX_OFT)
/// Descriptor done bit: Set by HW for TX DMA
#define DESC_DONE_TX_BIT                   CO_BIT(31)
/// Descriptor done bit: Set by SW for TX DMA
#define DESC_DONE_SW_TX_BIT                CO_BIT(30)

void tx_desc_init_for_fhost(struct fhost_tx_desc_tag *desc, int seg_cnt, uint32_t *seg_addr, uint16_t *seg_len);

#include "export_enum.h"
#endif
