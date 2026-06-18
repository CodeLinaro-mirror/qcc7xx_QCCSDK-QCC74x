#include <stdint.h>
#include <string.h>

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "mr_msg_ctrl.h"
#include "mr_msg_ctrl_stat.h"
#include "mr_frame_buff_ctrl.h"
#include "mr_netdev.h"

#include "msg_ctrl_sdio.h"
#include "netdev_wifi.h"
#include "nh_hub.h"

#ifdef CONFIG_WL80211
#define FHOST_RX_BUF_CNT 20
#else
#include "fhost.h"
#endif

#include <wifi_mgmr.h>
#include <wifi_mgmr_ext.h>

#define DBG_TAG "NETDEV_STA"
#include "log.h"

#define DEBUG_STA_LOG_ENABLE (0)

#define FRAME_BUFFER_ATTR __ALIGNED(64) ATTR_WIFI_RAM_SECTION
#define NETDEV_WIFI_SUBTAG_STA 0
#define NETDEV_WIFI_SUBTAG_AP  1
#define NETDEV_WIFI_HOST_ETH_PAD_SIZE 2U

FRAME_BUFFER_ATTR uint8_t netdev_nethub_frame_buff[NETDEV_NETHUB_CNT][NETDEV_NETHUB_FRAME_SIZE];

mr_netdev_priv_t *g_netdev_wifi_priv = NULL;

static nethub_channel_t netdev_wifi_channel_from_subtag(uint8_t sub_tag)
{
    return (sub_tag == NETDEV_WIFI_SUBTAG_AP) ? NETHUB_CHANNEL_WIFI_AP : NETHUB_CHANNEL_WIFI_STA;
}

static void wifi_tx_done_cb(void *arg)
{
    int ret;
    mr_netdev_priv_t *priv = g_netdev_wifi_priv;
    msg_sdio_dnld_fmt_t *dnld_fmt;

    if (!priv || !arg) {
        LOG_E("%s: Invalid parameters\r\n", __func__);
        return;
    }

    dnld_fmt = MSG_SDIO_DNLD_FMT_CONTAINER_OF(((uint8_t *)arg), trans_desc);

    memcpy(dnld_fmt->frame_elem, dnld_fmt->frame_elem_copy, sizeof(dnld_fmt->frame_elem));
    memcpy(dnld_fmt->netdev_msg, dnld_fmt->netdev_msg_copy, sizeof(dnld_fmt->netdev_msg));

#if DEBUG_STA_LOG_ENABLE
    printf("    --- dnld done, pkt:%p\r\n", dnld_fmt->netdev_msg);
#endif

    ret = mr_netdev_dnld_elem_free(priv, (mr_netdev_msg_t *)(dnld_fmt->netdev_msg));
    if (ret < 0) {
        LOG_E("%s: Failed to free dnld elem\r\n", __func__);
    }
}

int netdev_wifi_upld(nethub_frame_t *frame, void *arg)
{
    int ret;
    mr_netdev_priv_t *priv = g_netdev_wifi_priv;
    mr_netdev_msg_t *netdev_msg_pkt;
    mr_frame_elem_t *frame_elem;
    nethub_frame_t *frame_record;

    NETHUB_UNUSED(arg);
    if (!priv || !frame) {
        LOG_E("%s: Invalid parameters\r\n", __func__);
        return -1;
    }

    netdev_msg_pkt = (mr_netdev_msg_t *)((uintptr_t)frame->data - sizeof(mr_netdev_msg_t));
    frame_elem = MR_NETDEV_MSG_PACKET_TO_FRAME_ELEM_ADDR(netdev_msg_pkt);

    frame_elem->frame_ctrl = NULL;
    frame_elem->frame_type = 0;
    frame_elem->frame_id = 0;
    frame_elem->buff_addr = (void *)netdev_msg_pkt;
    frame_elem->buff_size = 2048;

    netdev_msg_pkt->msg_pkt.tag = priv->netdev_cfg.msg_tag;
    netdev_msg_pkt->msg_pkt.sub_tag = frame->type_internal;
    netdev_msg_pkt->flag = MR_NETDEV_FLAG_UPLD_DATA;
    netdev_msg_pkt->credit_update_flag = false;
    netdev_msg_pkt->credit_limit_cnt = 0;

    MR_NETDEV_MSG_PACKET_SET_DATA_SIZE(netdev_msg_pkt, frame->len);

    frame_record = (nethub_frame_t *)((uintptr_t)frame_elem - sizeof(nethub_frame_t));
    memcpy(frame_record, frame, sizeof(nethub_frame_t));

#if DEBUG_STA_LOG_ENABLE
    printf("    ==> upld start %s, pkt=%p, len=%u\r\n", __func__, netdev_msg_pkt, frame->len);
#endif

    ret = mr_netdev_upld_elem_send(priv, netdev_msg_pkt);
    if (ret < 0) {
        LOG_E("%s, Failed to send pkt\r\n", __func__);
        goto exit;
    }

    return 0;

exit:
    if (frame->free_cb) {
        frame->free_cb(frame->cb_arg);
    }
    return 0;
}

static int netdev_dnld_output_cb(mr_netdev_priv_t *priv, mr_netdev_msg_t *netdev_msg_pkt)
{
    nethub_route_result_t result;
    msg_sdio_dnld_fmt_t *dnld_fmt;
    nethub_frame_t frame = { 0 };
    nethub_channel_t dst_type;
    uint16_t payload_len;

    if (!priv || !netdev_msg_pkt || netdev_msg_pkt->flag != MR_NETDEV_FLAG_DNLD_DATA) {
        LOG_E("%s: Invalid parameters\r\n", __func__);
        return -1;
    }

    if (netdev_wifi_rcp_test_consume(priv, netdev_msg_pkt)) {
        return 0;
    }

    dnld_fmt = MSG_SDIO_DNLD_FMT_CONTAINER_OF(netdev_msg_pkt, netdev_msg);

    memcpy(dnld_fmt->frame_elem_copy, dnld_fmt->frame_elem, sizeof(dnld_fmt->frame_elem));
    memcpy(dnld_fmt->netdev_msg_copy, dnld_fmt->netdev_msg, sizeof(dnld_fmt->netdev_msg));

    payload_len = MR_NETDEV_MSG_PACKET_GET_DATA_SIZE(netdev_msg_pkt);
    if (payload_len <= NETDEV_WIFI_HOST_ETH_PAD_SIZE) {
        LOG_E("%s: invalid padded Ethernet length %u\r\n", __func__, payload_len);
        return -1;
    }

    frame.data = dnld_fmt->eth_payload + NETDEV_WIFI_HOST_ETH_PAD_SIZE;
    frame.len = payload_len - NETDEV_WIFI_HOST_ETH_PAD_SIZE;
    frame.free_cb = wifi_tx_done_cb;
    frame.cb_arg = dnld_fmt->trans_desc;
    frame.next = NULL;

#if DEBUG_STA_LOG_ENABLE
    printf("<-- dnld start, pkt:%p, len:%u,\r\n", netdev_msg_pkt, frame.len);
#endif

    dst_type = netdev_wifi_channel_from_subtag(netdev_msg_pkt->msg_pkt.sub_tag);
    MR_DNLD_PROF_BEGIN(nethub_send_cycles);
    result = nethub_send_output(&frame, dst_type);
    MR_DNLD_PROF_END(MR_DNLD_PROF_NETHUB_SEND, nethub_send_cycles);
    if (result == NETHUB_ROUTE_STOP) {
        LOG_W("Data consumed, forwarding stopped\n");
    } else if (result != NETHUB_ROUTE_CONTINUE) {
        LOG_W("Forwarding result abnormal: %d\n", result);
    }

    return 0;
}

static int netdev_upld_done_cb(mr_netdev_priv_t *priv, mr_netdev_msg_t *netdev_msg_pkt)
{
    mr_frame_elem_t *frame_elem;
    nethub_frame_t *frame_record;

    if (!priv || !netdev_msg_pkt) {
        LOG_E("%s: Invalid parameters\r\n", __func__);
        return -1;
    }

    frame_elem = MR_NETDEV_MSG_PACKET_TO_FRAME_ELEM_ADDR(netdev_msg_pkt);
    frame_record = (nethub_frame_t *)((uintptr_t)frame_elem - sizeof(nethub_frame_t));

    if (frame_record->free_cb) {
        frame_record->free_cb(frame_record->cb_arg);
    } else {
        LOG_E("No frame free callback, memory leak may occur\r\n");
    }

#if DEBUG_STA_LOG_ENABLE
    printf("    === upld done %s, pkt=%p\r\n", __func__, netdev_msg_pkt);
#endif

    return 0;
}

static int netdev_task_cb(mr_netdev_priv_t *priv, uint32_t *notified_value)
{
    NETHUB_UNUSED(priv);
    NETHUB_UNUSED(notified_value);

    return 0;
}

int netdev_wifi_init(mr_msg_ctrl_priv_t *msg_ctrl)
{
    mr_netdev_cfg_t netdev_cfg = {
        .name = "netdev_wifi",
        .msg_ctrl = msg_ctrl,
        .msg_tag = MR_MSG_TAG_NETDEV_WIFI_STA,
        .upld_frame_size = NETDEV_NETHUB_FRAME_SIZE,
        .upld_frame_count = NETDEV_NETHUB_CNT,
        .upld_frame_buff = netdev_nethub_frame_buff,
        .dnld_credit_max = msg_ctrl->cfg.dnld_frame_count,
        .upld_credit_update_threshold = 6,
        .dnld_output_cb = netdev_dnld_output_cb,
        .upld_done_cb = netdev_upld_done_cb,
        .task_priority = 27,
        .task_stack_size = 1024,
        .task_period_max_ms = portMAX_DELAY,
        .netdev_task_cb = netdev_task_cb,
        .link_up_flag = true,
        .mac = { 0x22, 0x22, 0x22, 0x88, 0x88, 0x88 },
        .ip = { 0, 0, 0, 0 },
    };

    wifi_mgmr_sta_mac_get(netdev_cfg.mac);

    g_netdev_wifi_priv = mr_netdev_init(&netdev_cfg);
    if (!g_netdev_wifi_priv) {
        LOG_E("Failed to init netdev emac\r\n");
        return -1;
    }

    return 0;
}
