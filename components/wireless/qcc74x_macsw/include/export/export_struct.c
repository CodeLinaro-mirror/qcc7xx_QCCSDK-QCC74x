#include <stdint.h>
#include <stdbool.h>
#include "export/rwnx_config.h"
#include "ip/lmac/src/mm/mm.h"
#include "ip/lmac/src/mm/mm_task.h"
#include "ip/umac/src/rxu/rxu_task.h"
#include "ip/umac/src/scanu/scanu_task.h"
#include "ip/lmac/src/scan/scan_task.h"
#include "ip/umac/src/sm/sm_task.h"
#include "ip/umac/src/me/me_task.h"
#include "ip/umac/src/apm/apm_task.h"
#if RW_MESH_EN
#include "modules/mesh/src/mesh_task.h"
#endif
#include "ip/umac/src/ftm/ftm_task.h"
#include "modules/dbg/src/dbg_task.h"
#if NX_TWT
#include "ip/umac/src/twt/twt_task.h"
#endif


#define struct_func_len(name)                                                   \
    int name##_len()                                                            \
    {                                                                           \
        return sizeof(struct name);                                             \
    }                                                                           \

#define struct_func_get_def(name, type, param)                                  \
    type name##_get_##param(void *pa)                                           \
    {                                                                           \
        struct name *p = (struct name *)pa;                                     \
        return p->param;                                                        \
    }                                                                           \

#define struct_func_getp_def(name, type, param)                                 \
    type *name##_getp_##param(void *pa)                                         \
    {                                                                           \
        struct name *p = (struct name *)pa;                                     \
        return &p->param;                                                       \
    }                                                                           \

#define struct_func_set_def(name, type, param)                                  \
    void name##_set_##param(void *pa, type val)                                 \
    {                                                                           \
        struct name *p = (struct name *)pa;                                     \
        p->param = val;                                                         \
    }                                                                           \

struct_func_get_def(rxu_mgt_ind, uint16_t, length)
    struct_func_get_def(rxu_mgt_ind, uint8_t, inst_nbr)
    struct_func_get_def(rxu_mgt_ind, uint32_t*, payload)
    struct_func_get_def(rxu_mgt_ind, uint16_t, center_freq)
    struct_func_get_def(rxu_mgt_ind, uint8_t, band)
    struct_func_get_def(rxu_mgt_ind, int8_t, rssi)

    struct_func_get_def(scanu_start_cfm, uint8_t, vif_idx)
    struct_func_get_def(scanu_start_cfm, uint8_t, status)
    struct_func_get_def(scanu_start_cfm, uint8_t, result_cnt)

    struct_func_get_def(sm_connect_ind, uint8_t, vif_idx)
    struct_func_get_def(sm_connect_ind, uint16_t, status_code)
    struct_func_get_def(sm_connect_ind, uint16_t, ieeetypes_code)
    struct_func_getp_def(sm_connect_ind, void, bssid)
    struct_func_getp_def(sm_connect_ind, void, ssid)
    struct_func_get_def(sm_connect_ind, uint16_t, assoc_req_ie_len)
    struct_func_get_def(sm_connect_ind, uint16_t, assoc_rsp_ie_len)
    struct_func_get_def(sm_connect_ind, uint8_t, security)
    uint16_t sm_connect_ind_get_chan_prim20_freq(void* param)
{
    return ((struct sm_connect_ind *)param)->chan.prim20_freq;
}

struct_func_get_def(scanu_country_code_ind, char*, country_code)

struct_func_get_def(sm_connect_ind, uint8_t, ap_idx)
    struct_func_get_def(sm_connect_ind, uint8_t, ch_idx)
    struct_func_get_def(sm_connect_ind, uint32_t*, assoc_ie_buf)
    struct_func_get_def(sm_connect_ind, uint8_t, acm)
    struct_func_get_def(sm_connect_ind, uint16_t, aid)
    struct_func_get_def(sm_connect_ind, struct mac_chan_op, chan)
    struct_func_get_def(sm_connect_ind, bool, qos)
    struct_func_get_def(sm_connect_ind, uint8_t, bss_mode)

    struct_func_get_def(sm_disconnect_ind, uint16_t, status_code)
    struct_func_get_def(sm_disconnect_ind, uint16_t, reason_code)
    struct_func_get_def(sm_disconnect_ind, uint8_t, vif_idx)

struct_func_get_def(sm_status_ind, uint8_t, vif_idx)
    uint32_t sm_status_ind_get_array_size(void* param)
{
    return sizeof(((struct sm_status_ind *)param)->info)/sizeof(((struct sm_status_ind *)param)->info[0]);
}
    uint32_t sm_status_ind_get_array_id(void* param, int idx)
{
    return ((struct sm_status_ind *)param)->info[idx].id;
}
    uint32_t sm_status_ind_get_array_val(void* param, int idx)
{
    return ((struct sm_status_ind *)param)->info[idx].val;
}
    struct_func_get_def(me_tkip_mic_failure_ind, uint8_t, vif_idx)
    struct_func_getp_def(me_tkip_mic_failure_ind, void, addr)
    struct_func_get_def(me_tkip_mic_failure_ind, bool, ga)

    struct_func_get_def(me_tx_credits_update_ind, uint8_t, sta_idx)
    struct_func_get_def(me_tx_credits_update_ind, uint8_t, tid)
    struct_func_get_def(me_tx_credits_update_ind, int8_t, credits)

    struct_func_get_def(sm_external_auth_required_ind, uint8_t, vif_idx)
    struct_func_get_def(sm_external_auth_required_ind, struct mac_addr, bssid)
    struct_func_get_def(sm_external_auth_required_ind, struct mac_ssid, ssid)
    struct_func_get_def(sm_external_auth_required_ind, uint32_t, akm)

    struct_func_get_def(mm_ps_change_ind, uint8_t, sta_idx)
    struct_func_get_def(mm_ps_change_ind, uint8_t, ps_state)

    struct_func_get_def(mm_traffic_req_ind, uint8_t, sta_idx)
    struct_func_get_def(mm_traffic_req_ind, uint8_t, pkt_cnt)
    struct_func_get_def(mm_traffic_req_ind, bool, uapsd)

    struct_func_get_def(apm_probe_client_ind, uint8_t, vif_idx)
    struct_func_get_def(apm_probe_client_ind, uint8_t, sta_idx)
    struct_func_get_def(apm_probe_client_ind, bool, client_present)

    struct_func_get_def(mm_remain_on_channel_exp_ind, uint8_t, vif_index)
    struct_func_get_def(mm_remain_on_channel_exp_ind, uint16_t, freq)

    struct_func_get_def(mm_p2p_vif_ps_change_ind, uint8_t, vif_index)
    struct_func_get_def(mm_p2p_vif_ps_change_ind, uint8_t, ps_state)
    struct_func_get_def(mm_flow_cntrl_change_ind, uint8_t, vif_index)
    struct_func_get_def(mm_flow_cntrl_change_ind, uint8_t, enable)

#if RW_MESH_EN
    struct_func_get_def(mesh_path_update_ind, uint8_t, vif_idx)
    struct_func_get_def(mesh_path_update_ind, uint8_t, path_idx)
    struct_func_get_def(mesh_path_update_ind, bool, delete)
    struct_func_get_def(mesh_path_update_ind, uint8_t, nhop_sta_idx)
#endif

    struct_func_set_def(apm_start_req, struct mac_rateset, basic_rates)
    struct_func_set_def(apm_start_req, struct mac_chan_op, chan)
    struct_func_getp_def(apm_start_req, void, chan)
    struct_func_set_def(apm_start_req, uint32_t, bcn_addr)
    struct_func_set_def(apm_start_req, uint16_t, bcn_len)
    struct_func_set_def(apm_start_req, uint16_t, bcn_int)
    struct_func_set_def(apm_start_req, uint16_t, tim_oft)
    struct_func_set_def(apm_start_req, uint8_t, tim_len)
    struct_func_set_def(apm_start_req, uint32_t, flags)
    struct_func_set_def(apm_start_req, uint16_t, ctrl_port_ethertype)
    struct_func_set_def(apm_start_req, uint8_t, vif_idx)
    struct_func_len(apm_start_req)

    struct_func_get_def(apm_start_cfm, uint8_t, bcmc_idx)
    struct_func_len(apm_start_cfm)

    struct_func_set_def(apm_stop_req, uint8_t, vif_idx)
    struct_func_len(apm_stop_req)

    struct_func_set_def(apm_probe_client_req, uint8_t, vif_idx)
    struct_func_set_def(apm_probe_client_req, uint8_t, sta_idx)
    struct_func_len(apm_probe_client_req)
    struct_func_get_def(apm_probe_client_cfm, uint8_t, status)
    struct_func_len(apm_probe_client_cfm)

#if NX_FTM_INITIATOR
    struct_func_get_def(ftm_done_ind, uint8_t, vif_idx)
    struct_func_get_def(ftm_done_ind, struct mac_ftm_results, results)
    struct_func_set_def(ftm_start_req, uint8_t, vif_idx)
    struct_func_set_def(ftm_start_req, uint8_t, ftm_per_burst)
    struct_func_set_def(ftm_start_req, uint8_t, nb_ftm_rsp)
    struct_func_len(ftm_start_req)
    struct_func_get_def(ftm_start_cfm, uint8_t, status)
    struct_func_len(ftm_start_cfm)
#endif

void *scanu_start_req_chan(void *req, int nb)
{
    return &((struct scanu_start_req *)req)->chan[nb];
}
struct_func_set_def(scanu_start_req, uint8_t, chan_cnt)
    struct_func_get_def(scanu_start_req, uint8_t, chan_cnt)
void scanu_start_req_set_chan_flags(void *req, int nb, short flag)
{
    ((struct scanu_start_req *)req)->chan[nb].flags |= (enum mac_chan_flags)flag;
}
void *scanu_start_req_ssid_array(void *req, int nb)
{
    return ((struct scanu_start_req *)req)->ssid[nb].array;
}
void scanu_start_req_set_ssid_length(void *req, int nb, uint8_t len)
{
    ((struct scanu_start_req *)req)->ssid[nb].length = len;
}
struct_func_set_def(scanu_start_req, uint8_t, ssid_cnt)
    struct_func_getp_def(scanu_start_req, void, bssid)
    struct_func_set_def(scanu_start_req, uint32_t, add_ies)
    struct_func_set_def(scanu_start_req, uint16_t, add_ie_len)
    struct_func_set_def(scanu_start_req, uint8_t, vif_idx)
    struct_func_set_def(scanu_start_req, bool, no_cck)
    struct_func_set_def(scanu_start_req, uint32_t, duration)
    struct_func_set_def(scanu_start_req, bool, scan_only)
    struct_func_set_def(scanu_start_req, uint32_t, flags)
    struct_func_len(scanu_start_req)

    struct_func_set_def(scanu_get_scan_result_req, uint8_t, idx)
    struct_func_len(scanu_get_scan_result_req)

    struct_func_set_def(sm_connect_req, uint32_t, mode)
    struct_func_set_def(sm_connect_req, struct mac_ssid, ssid)
    struct_func_getp_def(sm_connect_req, void, bssid)
    struct_func_set_def(sm_connect_req, struct mac_chan_def, chan)
    struct_func_set_def(sm_connect_req, uint32_t, flags)
    struct_func_set_def(sm_connect_req, uint16_t, ctrl_port_ethertype)
    struct_func_set_def(sm_connect_req, uint16_t, ie_len)
    struct_func_set_def(sm_connect_req, uint16_t, listen_interval)
    struct_func_set_def(sm_connect_req, bool, dont_wait_bcmc)
    struct_func_set_def(sm_connect_req, uint8_t, auth_type)
    struct_func_set_def(sm_connect_req, uint8_t, uapsd_queues)
    struct_func_set_def(sm_connect_req, uint8_t, vif_idx)
    struct_func_set_def(sm_connect_req, uint8_t, wpa)
    struct_func_len(sm_connect_req)
    struct_func_get_def(sm_connect_cfm, uint8_t, status)
    struct_func_len(sm_connect_cfm)

    struct_func_set_def(sm_disconnect_req, uint8_t, vif_idx)
    struct_func_set_def(sm_disconnect_req, uint16_t, reason_code)
    struct_func_set_def(sm_disconnect_req, uint16_t, status_code)
    struct_func_len(sm_disconnect_req)

    struct_func_set_def(sm_external_auth_required_rsp, uint8_t, vif_idx)
    struct_func_set_def(sm_external_auth_required_rsp, uint16_t, status)
    struct_func_len(sm_external_auth_required_rsp)

    struct_func_set_def(mm_raw_send_start_req, void *, pkt)
    struct_func_set_def(mm_raw_send_start_req, uint32_t, len)
    struct_func_set_def(mm_raw_send_start_req, int, vif_idx)
    struct_func_set_def(mm_raw_send_start_req, struct mac_chan_op, chan)
    struct_func_set_def(mm_raw_send_start_req, uint32_t, duration)
    struct_func_set_def(mm_raw_send_start_req, void*, cb)
    struct_func_set_def(mm_raw_send_start_req, void*, env)
    struct_func_set_def(mm_raw_send_start_req, int, need_rx)
    struct_func_set_def(mm_raw_send_start_req, bool, adhoc);
    struct_func_set_def(mm_raw_send_start_req, struct mac_addr *, ra);
    struct_func_set_def(mm_raw_send_start_req, struct mac_addr *, ta);
    struct_func_set_def(mm_raw_send_start_req, uint8_t, rate);
    struct_func_set_def(mm_raw_send_start_req, uint16_t, rts_thrshold);
    struct_func_set_def(mm_raw_send_start_req, uint8_t, retry_limit);
    struct_func_set_def(mm_raw_send_start_req, int8_t, tx_power);
    struct_func_set_def(mm_raw_send_start_req, void*, cb_cfm);
    struct_func_len(mm_raw_send_start_req)
    struct_func_get_def(mm_raw_send_start_cfm, uint8_t, status)
    struct_func_len(mm_raw_send_start_cfm)

    struct_func_set_def(mm_add_if_req, uint8_t, type)
    struct_func_set_def(mm_add_if_req, struct mac_addr, addr)
    struct_func_set_def(mm_add_if_req, bool, p2p)
    struct_func_len(mm_add_if_req)
    struct_func_get_def(mm_add_if_cfm, uint8_t, status)
    struct_func_get_def(mm_add_if_cfm, uint8_t, inst_nbr)
    struct_func_len(mm_add_if_cfm)

    struct_func_set_def(mm_remove_if_req, uint8_t, inst_nbr)
    struct_func_len(mm_remove_if_req)

    struct_func_set_def(mm_key_add_req, uint8_t, key_idx)
    struct_func_set_def(mm_key_add_req, uint8_t, sta_idx)
    struct_func_set_def(mm_key_add_req, struct mac_sec_key, key)
    struct_func_set_def(mm_key_add_req, uint8_t, cipher_suite)
    struct_func_set_def(mm_key_add_req, uint8_t, inst_nbr)
    struct_func_set_def(mm_key_add_req, uint8_t, spp)
    struct_func_set_def(mm_key_add_req, bool, pairwise)
    struct_func_len(mm_key_add_req)
    struct_func_get_def(mm_key_add_cfm, uint8_t, status)
    struct_func_len(mm_key_add_cfm)

    struct_func_set_def(mm_key_del_req, uint8_t, hw_key_idx)
    struct_func_len(mm_key_del_req)

    struct_func_set_def(me_set_control_port_req, uint8_t, sta_idx)
    struct_func_set_def(me_set_control_port_req, bool, control_port_open)
    struct_func_len(me_set_control_port_req)

    struct_func_get_def(me_set_control_port_cfm, bool, control_port_open)
    struct_func_len(me_set_control_port_cfm)

    struct_func_get_def(mm_version_cfm, uint32_t, version_lmac)
    struct_func_get_def(mm_version_cfm, uint32_t, version_machw_1)
    struct_func_get_def(mm_version_cfm, uint32_t, version_machw_2)
    struct_func_get_def(mm_version_cfm, uint32_t, version_phy_1)
    struct_func_get_def(mm_version_cfm, uint32_t, version_phy_2)
    struct_func_get_def(mm_version_cfm, uint32_t, features)
    struct_func_get_def(mm_version_cfm, uint16_t, max_sta_nb)
    struct_func_get_def(mm_version_cfm, uint8_t, max_vif_nb)
    struct_func_len(mm_version_cfm)

    struct_func_set_def(me_config_monitor_req, struct mac_chan_op, chan)
    struct_func_set_def(me_config_monitor_req, bool, chan_set)
    struct_func_set_def(me_config_monitor_req, bool, uf)
    struct_func_len(me_config_monitor_req)
    struct_func_len(me_config_monitor_cfm)

    struct_func_set_def(mm_set_edca_req, uint32_t, ac_param)
    struct_func_set_def(mm_set_edca_req, bool, uapsd)
    struct_func_set_def(mm_set_edca_req, uint8_t, hw_queue)
    struct_func_set_def(mm_set_edca_req, uint8_t, inst_nbr)
    struct_func_len(mm_set_edca_req)

    struct_func_set_def(mm_bcn_change_req, uint32_t, bcn_ptr)
    struct_func_set_def(mm_bcn_change_req, uint16_t, bcn_len)
    struct_func_set_def(mm_bcn_change_req, uint16_t, tim_oft)
    struct_func_set_def(mm_bcn_change_req, uint8_t, tim_len)
    struct_func_set_def(mm_bcn_change_req, uint8_t, inst_nbr)
    void mm_bcn_change_req_set_csa_oft(void *pa, uint8_t val, int i)
{
    struct mm_bcn_change_req *p = (struct mm_bcn_change_req *)pa;
    p->csa_oft[i] = val;
}
struct_func_len(mm_bcn_change_req)

    struct_func_set_def(mm_bcn_control_req, uint8_t, bcn_tx_mode)
    struct_func_set_def(mm_bcn_control_req, int, bcn_tx_timer)
    struct_func_set_def(mm_bcn_control_req, bool, bcn_tx_stop)
    struct_func_len(mm_bcn_control_req)

    struct_func_set_def(me_sta_add_req, struct mac_addr, mac_addr)
    struct_func_set_def(me_sta_add_req, struct mac_rateset, rate_set)
    struct_func_set_def(me_sta_add_req, struct mac_htcapability, ht_cap)
    struct_func_set_def(me_sta_add_req, struct mac_vhtcapability, vht_cap)
    struct_func_set_def(me_sta_add_req, struct mac_hecapability, he_cap)
    struct_func_set_def(me_sta_add_req, uint32_t, flags)
    struct_func_set_def(me_sta_add_req, uint16_t, aid)
    struct_func_set_def(me_sta_add_req, uint8_t, uapsd_queues)
    struct_func_set_def(me_sta_add_req, uint8_t, max_sp_len)
    struct_func_set_def(me_sta_add_req, uint8_t, opmode)
    struct_func_set_def(me_sta_add_req, uint8_t, vif_idx)
    struct_func_set_def(me_sta_add_req, bool, tdls_sta)
    struct_func_set_def(me_sta_add_req, bool, tdls_initiator)
    struct_func_set_def(me_sta_add_req, bool, tdls_chsw_allowed)
    struct_func_len(me_sta_add_req)
    struct_func_get_def(me_sta_add_cfm, uint8_t, sta_idx)
    struct_func_len(me_sta_add_cfm)

    struct_func_set_def(me_sta_del_req, uint8_t, sta_idx)
    struct_func_set_def(me_sta_del_req, bool, tdls_sta)
    struct_func_len(me_sta_del_req)

    struct_func_set_def(me_rc_set_rate_req, uint16_t, fixed_rate_cfg)
    struct_func_set_def(me_rc_set_rate_req, uint8_t, sta_idx)
    struct_func_len(me_rc_set_rate_req)

    #if NX_POWERSAVE
    struct_func_set_def(me_set_ps_mode_req, uint8_t, ps_mode)
    struct_func_set_def(me_set_ps_mode_req, uint8_t, ps_state)
    struct_func_len(me_set_ps_mode_req)
    #endif

    struct_func_set_def(mm_remain_on_channel_req, uint8_t, op_code)
    struct_func_set_def(mm_remain_on_channel_req, uint8_t, vif_index)
    struct_func_set_def(mm_remain_on_channel_req, struct mac_chan_op, chan)
    struct_func_set_def(mm_remain_on_channel_req, uint32_t, duration_ms)
    struct_func_len(mm_remain_on_channel_req)
    struct_func_get_def(mm_remain_on_channel_cfm, uint8_t, status)
    struct_func_len(mm_remain_on_channel_cfm)

    struct_func_set_def(mm_set_p2p_noa_req, uint8_t, vif_index)
    struct_func_set_def(mm_set_p2p_noa_req, uint8_t, noa_inst_nb)
    struct_func_set_def(mm_set_p2p_noa_req, uint8_t, count)
    struct_func_set_def(mm_set_p2p_noa_req, bool, dyn_noa)
    struct_func_set_def(mm_set_p2p_noa_req, uint32_t, duration_us)
    struct_func_set_def(mm_set_p2p_noa_req, uint32_t, interval_us)
    struct_func_set_def(mm_set_p2p_noa_req, uint32_t, start_offset)
    struct_func_len(mm_set_p2p_noa_req)
    struct_func_get_def(mm_set_p2p_noa_cfm, uint8_t, status)
    struct_func_len(mm_set_p2p_noa_cfm)

    #if RW_MESH_EN
    struct_func_set_def(mesh_start_req, struct mac_rateset, basic_rates)
    struct_func_set_def(mesh_start_req, struct mac_chan_op, chan)
    struct_func_set_def(mesh_start_req, uint8_t, dtim_period)
    struct_func_set_def(mesh_start_req, uint8_t, bcn_int)
    struct_func_set_def(mesh_start_req, uint8_t, vif_idx)
    struct_func_set_def(mesh_start_req, uint8_t, mesh_id_len)
    void mesh_start_req_set_mesh_id(void *pa, uint8_t val, int i)
{
    struct mesh_start_req *p = (struct mesh_start_req *)pa;
    p->mesh_id[i] = val;
}
struct_func_set_def(mesh_start_req, uint32_t, ie_addr)
    struct_func_set_def(mesh_start_req, uint8_t, ie_len)
    struct_func_set_def(mesh_start_req, bool, user_mpm)
    struct_func_set_def(mesh_start_req, bool, is_auth)
    struct_func_set_def(mesh_start_req, uint8_t, auth_id)
    struct_func_len(mesh_start_req)
    struct_func_get_def(mesh_start_cfm, uint8_t, status)
    struct_func_get_def(mesh_start_cfm, uint8_t, bcmc_idx)
    struct_func_len(mesh_start_cfm)

    struct_func_set_def(mesh_stop_req, uint8_t, vif_idx)
    struct_func_len(mesh_stop_req)
    struct_func_get_def(mesh_stop_cfm, uint8_t, status)
    struct_func_len(mesh_stop_cfm)

    struct_func_set_def(mesh_peer_update_ntf, uint8_t, vif_idx)
    struct_func_set_def(mesh_peer_update_ntf, uint8_t, sta_idx)
    struct_func_set_def(mesh_peer_update_ntf, uint8_t, state)
    struct_func_len(mesh_peer_update_ntf)
    #endif

    struct_func_set_def(me_config_req, struct mac_htcapability, ht_cap)
    struct_func_set_def(me_config_req, struct mac_vhtcapability, vht_cap)
    struct_func_set_def(me_config_req, struct mac_hecapability, he_cap)
    struct_func_set_def(me_config_req, uint16_t, tx_lft)
    struct_func_set_def(me_config_req, uint8_t, phy_bw_max)
    struct_func_set_def(me_config_req, bool, ht_supp)
    struct_func_set_def(me_config_req, bool, vht_supp)
    struct_func_set_def(me_config_req, bool, he_supp)
    struct_func_set_def(me_config_req, bool, he_ul_on)
    struct_func_set_def(me_config_req, bool, ps_on)
    struct_func_set_def(me_config_req, bool, ant_div_on)
    struct_func_set_def(me_config_req, bool, dpsm)
void me_config_req_set_amsdu_tx(void *pa, short val)
{
    struct me_config_req *p = (struct me_config_req *)pa;
    p->amsdu_tx = (enum amsdu_tx)val;
}
    struct_func_len(me_config_req)

    struct_func_set_def(mm_start_req, struct phy_cfg_tag, phy_cfg)
    struct_func_set_def(mm_start_req, uint32_t, uapsd_timeout)
    struct_func_set_def(mm_start_req, uint16_t, lp_clk_accuracy)
void mm_start_req_set_tx_timeout(void *pa, uint16_t array[], int len)
{
    struct mm_start_req *p = (struct mm_start_req *)pa;
    for(int i = 0; i < len; i++)
        p->tx_timeout[i] = array[i];
}
    struct_func_set_def(mm_start_req, uint8_t, coex_mode);
    struct_func_len(mm_start_req)

    struct_func_set_def(me_traffic_ind_req, uint8_t, sta_idx)
    struct_func_set_def(me_traffic_ind_req, uint8_t, tx_avail)
    struct_func_set_def(me_traffic_ind_req, bool, uapsd)
    struct_func_len(me_traffic_ind_req)

    struct_func_get_def(dbg_get_sys_stat_cfm, uint32_t, cpu_sleep_time)
    struct_func_get_def(dbg_get_sys_stat_cfm, uint32_t, doze_time)
    struct_func_get_def(dbg_get_sys_stat_cfm, uint32_t, stats_time)
    struct_func_len(dbg_get_sys_stat_cfm)

    struct_func_set_def(me_get_edca_req, uint8_t, hw_queue)
    struct_func_len(me_get_edca_req)

    struct_func_get_def(me_get_edca_cfm, uint32_t, ac_param)
    struct_func_len(me_get_edca_cfm)

    struct_func_get_def(me_get_stats_cfm, struct ieee80211_stats, sta_stats)
    struct_func_get_def(me_get_stats_cfm, struct ieee80211_stats, ap_stats)
    struct_func_get_def(me_get_stats_cfm, struct ieee80211_stats, adhoc_stats)
    struct_func_len(me_get_stats_cfm)

    struct_func_get_def(me_get_remaining_tx_cfm, uint8_t, tx0_cnt)
    struct_func_get_def(me_get_remaining_tx_cfm, uint8_t, tx1_cnt)
    struct_func_get_def(me_get_remaining_tx_cfm, uint8_t, tx2_cnt)
    struct_func_get_def(me_get_remaining_tx_cfm, uint8_t, tx3_cnt)
    struct_func_len(me_get_remaining_tx_cfm)

    struct_func_get_def(me_get_coex_status_cfm, uint8_t, status)
    struct_func_len(me_get_coex_status_cfm)

    struct_func_get_def(me_get_coex_duty_cfm, uint8_t, duty_ms)
    struct_func_len(me_get_coex_duty_cfm)

#if NX_TWT
    struct_func_set_def(twt_setup_req, uint8_t, vif_idx);
    struct_func_set_def(twt_setup_req, uint8_t, setup_type);
void twt_setup_req_set_conf_flow_type(void *pa, uint8_t val) {
    struct twt_setup_req *p = (struct twt_setup_req *)pa;
    p->conf.flow_type = val; 
};
void twt_setup_req_set_conf_wake_int_exp(void *pa, uint8_t val) {
    struct twt_setup_req *p = (struct twt_setup_req *)pa;
    p->conf.wake_int_exp = val; 
};
void twt_setup_req_set_conf_wake_dur_uint(void *pa, bool val) {
    struct twt_setup_req *p = (struct twt_setup_req *)pa;
    p->conf.wake_dur_unit = val; 
};
void twt_setup_req_set_conf_min_twt_wake_dur(void *pa, uint8_t val) {
    struct twt_setup_req *p = (struct twt_setup_req *)pa;
    p->conf.min_twt_wake_dur = val; 
};
void twt_setup_req_set_conf_wake_int_mantissa(void *pa, uint16_t val) {
    struct twt_setup_req *p = (struct twt_setup_req *)pa;
    p->conf.wake_int_mantissa = val; 
};
    struct_func_len(twt_setup_req)

    struct_func_get_def(twt_setup_cfm, uint8_t, status);

    struct_func_set_def(twt_teardown_req, uint8_t, neg_type);
    struct_func_set_def(twt_teardown_req, uint8_t, all_twt);
    struct_func_set_def(twt_teardown_req, uint8_t, id);
    struct_func_set_def(twt_teardown_req, uint8_t, vif_idx);
    struct_func_len(twt_teardown_req)

    struct_func_get_def(twt_teardown_cfm, uint8_t, status);

    struct_func_set_def(twt_statusget_req, uint8_t, vif_idx);
    struct_func_len(twt_statusget_req)

    struct_func_get_def(twt_statusget_cfm, uint8_t, status);
    struct_func_get_def(twt_statusget_cfm, uint8_t, flows);
    struct_func_get_def(twt_statusget_cfm, uint8_t, twt_supported);
    struct_func_get_def(twt_statusget_cfm, uint8_t, dtim);
void twt_statusget_cfm_get_conf(void *pa,  struct twt_statusget_cfm *cfm, uint8_t flow_conf_max) {
    struct twt_status_info *p = (struct twt_status_info *)pa;
    struct twt_status_info conf[NX_TWT_FLOW_NB];
    uint8_t max = (NX_TWT_FLOW_NB > flow_conf_max ? NX_TWT_FLOW_NB : flow_conf_max);
    memcpy(p, (struct twt_status_info *)(cfm->conf), sizeof(struct twt_status_info)*max); 
};
    struct_func_len(twt_statusget_cfm)
#endif
