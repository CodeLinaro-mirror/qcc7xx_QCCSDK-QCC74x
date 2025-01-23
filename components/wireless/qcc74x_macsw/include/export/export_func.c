#include <stdbool.h>
#include "ip/lmac/src/vif/vif_mgmt.h"
#include "ip/umac/src/rc/rc.h"
#include "ip/lmac/src/tx/tx_swdesc.h"
#include "ip/lmac/src/rx/rxl/rxl_hwdesc.h"
#include "ip/lmac/src/hal/hal_desc.h"
#include "ip/lmac/src/hal/hal_machw.h"
#include "ip/lmac/src/mm/mm.h"
#include "ip/umac/src/me/me.h"
#include "plf_build_reg/reg_mac_core.h"
#include "plf/refip/src/driver/time/nx_time.h"
#include "modules/statistics/src/co_stats.h"


void *vif_info_get_vif(int index)
{
    return &vif_info_tab[index];
}

uint8_t mac_vif_get_sta_ap_id(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return vif->u.sta.ap_id;
}

uint16_t mac_vif_get_bcn_int(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return vif->u.ap.bcn_int;
}

void mac_vif_get_channel(void *macif, struct mac_chan_op* chan)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    if (vif->chan_ctxt) {
        *chan = vif->chan_ctxt->channel;
    }
}

void mac_vif_get_sta_status(void *macif, struct mac_addr *bssid, uint16_t *aid, int8_t *rssi)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    if ((vif->type == VIF_STA) &&
        (vif->u.sta.ap_id != INVALID_STA_IDX)) {
        struct sta_info_tag *sta = &sta_info_tab[vif->u.sta.ap_id];
        *bssid = sta->mac_addr;
        *rssi = vif->u.sta.rssi;
        *aid = sta->aid;
    }
}

struct co_list *mac_vif_get_sta_list(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return &vif->sta_list;
}

int mac_vif_get_key_info_len(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return CO_ARRAY_SIZE(vif->key_info);
}

struct key_info_tag *mac_vif_get_key(void *macif, int key_idx)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return &vif->key_info[key_idx];
}

uint8_t mac_vif_get_index(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return vif->index;
}

enum mac_vif_type mac_vif_get_type(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return vif->type;
}

bool mac_vif_get_active(void *macif)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)macif;
    return vif->active;
}

uint8_t sta_get_staid(void *p)
{
    struct sta_info_tag *sta = (struct sta_info_tag *)p;
    return sta->staid;
}

void *sta_get_list_next(void *p)
{
    struct sta_info_tag *sta = (struct sta_info_tag *)p;
    return sta->list_hdr.next;
}

void *sta_getp_mac_addr(void *p)
{
    struct sta_info_tag *sta = (struct sta_info_tag *)p;
    return &sta->mac_addr;
}

uint32_t sta_idx_get_last_active(uint8_t sta_idx)
{
    return sta_mgmt_last_active(sta_idx);
}
void *sta_idx_getp_mac_addr(uint8_t sta_idx)
{
    return &(sta_info_tab[sta_idx].mac_addr);
}

#if (RW_MESH_EN)
uint8_t sta_idx_get_mlink_idx(uint8_t sta_idx)
{
    return sta_info_tab[sta_idx].mlink_idx;
}
#endif

uint8_t sta_idx_get_key_idx(uint8_t sta_idx)
{
    return sta_info_tab[sta_idx].sta_sec_info.key_info.key_idx;
}

void *sta_idx_getp_key_info(uint8_t sta_idx)
{
    return &(sta_info_tab[sta_idx].sta_sec_info.key_info);
}

bool sta_idx_get_valid(uint8_t sta_idx)
{
    return sta_info_tab[sta_idx].valid;
}

uint8_t sta_idx_get_inst_nbr(uint8_t sta_idx)
{
    return sta_info_tab[sta_idx].inst_nbr;
}

uint8_t sta_idx_get_uapsd_queues(uint8_t sta_idx)
{
    return sta_info_tab[sta_idx].info.uapsd_queues;
}

uint8_t export_rc_get_format_mod(uint32_t rate_config)
{
    return rc_get_format_mod(rate_config);
}

uint8_t export_rc_get_mcs_index(uint32_t rate_config)
{
    return rc_get_mcs_index(rate_config);
}

uint8_t export_rc_get_nss(uint32_t rate_config)
{
    return rc_get_nss(rate_config);
}

uint8_t export_rc_get_bw(uint32_t rate_config)
{
    return rc_get_bw(rate_config);
}

uint8_t export_rc_get_pre_type(uint32_t rate_config)
{
    return rc_get_pre_type(rate_config);
}

uint8_t export_rc_get_sgi(uint32_t rate_config)
{
    return rc_get_sgi(rate_config);
}
int8_t export_hal_desc_get_rssi(void *rx_vec_1, int8_t *rx_rssi)
{
    return hal_desc_get_rssi(rx_vec_1, rx_rssi);
}
uint16_t export_hal_desc_get_ht_length(void *rx_vec_1)
{
    return hal_desc_get_ht_length(rx_vec_1);
}

uint8_t export_MM_STA_TO_KEY(uint8_t sta_idx)
{
    return MM_STA_TO_KEY(sta_idx);
}

#if (RW_UMESH_EN)
uint8_t export_MM_MLINK_TO_MFP_KEY(uint8_t key_idx, uint8_t mlink_idx)
{
    return MM_MLINK_TO_MFP_KEY(key_idx, mlink_idx);
}

uint8_t export_MM_MLINK_TO_KEY(uint8_t key_idx, uint8_t mlink_idx)
{
    return MM_MLINK_TO_KEY(key_idx, mlink_idx);
}
#endif

uint8_t export_MM_VIF_TO_KEY(uint8_t key_idx, uint8_t vif_idx)
{
    return MM_VIF_TO_KEY(key_idx, vif_idx);
}

uint8_t export_MM_VIF_TO_MFP_KEY(uint8_t key_idx, uint8_t vif_idx)
{
    return MM_VIF_TO_MFP_KEY(key_idx, vif_idx);
}

#if NX_POWERSAVE
void me_env_set_ps_mode(int mode)
{
    me_env.ps_mode = mode;
}
#endif

bool inline_hal_machw_he_support(void)
{
    return hal_machw_he_support();
}
uint32_t inline_hal_machw_time(void)
{
    return hal_machw_time();
}
bool inline_hal_machw_time_cmp(uint32_t time1, uint32_t time2)
{
    return hal_machw_time_cmp(time1, time2);
}

bool inline_hal_machw_time_past(uint32_t time)
{
    return hal_machw_time_past(time);
}

int get_nx_txdesc_cnt(int i)
{
    return nx_txdesc_cnt[i];
}

uint8_t inline_nxmac_tkip_getf()
{
    return nxmac_tkip_getf();
}

uint8_t inline_nxmac_ccmp_getf()
{
    return nxmac_ccmp_getf();
}

uint8_t inline_nxmac_gcmp_getf()
{
    return nxmac_gcmp_getf();
}

int get_time_SINCE_EPOCH(uint32_t *sec, uint32_t *usec)
{
    return get_time(SINCE_EPOCH, sec, usec);
}

int get_time_SINCE_BOOT(uint32_t *sec, uint32_t *usec)
{
    return get_time(SINCE_BOOT, sec, usec);
}

uint32_t *export_get_rx_buffer1_addr(void)
{
    return rxl_get_rx_buffer1_addr();
}

uint8_t export_stats_get_tx_mcs()
{
    return rc_pkt_ss.tx_mcs;
}

uint8_t export_stats_get_rx_mcs()
{
    return rc_pkt_ss.rx_mcs;
}

char* stats_get_format(uint8_t format)
{
    switch(format) {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
                return "NON_HT";
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
                return "HT";
        case FORMATMOD_VHT:
                return "VHT";
        case FORMATMOD_HE_SU:
        case FORMATMOD_HE_MU:
        case FORMATMOD_HE_ER:
                return "HE";
        defaule:
                return "N/A";
    }
}
char* export_stats_get_tx_format()
{
    return stats_get_format(rc_pkt_ss.tx_format);
}

char* export_stats_get_rx_format()
{
    return stats_get_format(rc_pkt_ss.rx_format);
}
