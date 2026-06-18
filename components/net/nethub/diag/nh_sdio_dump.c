#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include "mr_frame_buff_ctrl.h"
#include "mr_msg_ctrl.h"
#include "mr_msg_ctrl_stat.h"
#include "mr_netdev.h"
#ifdef CONFIG_MR_TTY
#include "mr_tty.h"
#include "tty_atcmd.h"
#endif

#include "msg_ctrl_sdio.h"
#include "netdev_wifi.h"

#include "shell.h"

#define DBG_TAG "MSG"
#include "log.h"

static const char *msg_ctrl_sdio_stream_name(uint8_t tag)
{
    switch (tag) {
        case MR_MSG_TAG_NONE:
            return "NONE";
        case MR_MSG_TAG_TEST_1:
            return "TEST_1";
        case MR_MSG_TAG_TEST_2:
            return "TEST_2";
        case MR_MSG_TAG_TEST_3:
            return "TEST_3";
        case MR_MSG_TAG_SYS:
            return "SYS";
        case MR_MSG_TAG_NETDEV_WIFI_STA:
            return "WIFI";
        case MR_MSG_TAG_NETDEV_WIFI_AP:
            return "WIFI_AP";
        case MR_MSG_TAG_NETDEV_ETH:
            return "ETH";
        case MR_MSG_TAG_TTY_CMD:
            return "TTY";
        case MR_MSG_TAG_TTY_USER_1:
            return "TTY_USER_1";
        case MR_MSG_TAG_TTY_USER_2:
            return "TTY_USER_2";
        case MR_MSG_TAG_TTY_USER_3:
            return "TTY_USER_3";
        case MR_MSG_TAG_NETLINK:
            return "NETLINK";
        case MR_MSG_TAG_HCI:
            return "BT_HCI";
        case MR_MSG_TAG_VIRTUALCHAN:
            return "VCHAN";
        default:
            return "?";
    }
}

static const char *msg_ctrl_sdio_netdev_state_name(int state)
{
    switch (state) {
        case MR_NETDEV_DSTA_ERROR:
            return "ERROR";
        case MR_NETDEV_DSTA_IDLE:
            return "IDLE";
        case MR_NETDEV_DSTA_HOST_READY:
            return "HOST_READY";
        case MR_NETDEV_DSTA_DEVICE_RUN:
            return "DEVICE_RUN";
        default:
            return "UNKNOWN";
    }
}

#ifdef CONFIG_MR_TTY
static const char *msg_ctrl_sdio_tty_state_name(int state)
{
    switch (state) {
        case MR_TTY_DSTA_ERROR:
            return "ERROR";
        case MR_TTY_DSTA_IDLE:
            return "IDLE";
        case MR_TTY_DSTA_HOST_READY:
            return "HOST_READY";
        case MR_TTY_DSTA_DEVICE_RUN:
            return "DEVICE_RUN";
        default:
            return "UNKNOWN";
    }
}
#endif

static unsigned long msg_ctrl_sdio_queue_used(QueueHandle_t queue)
{
    if (queue == NULL) {
        return 0;
    }

    return (unsigned long)uxQueueMessagesWaiting(queue);
}

static unsigned long msg_ctrl_sdio_queue_free(QueueHandle_t queue)
{
    if (queue == NULL) {
        return 0;
    }

    return (unsigned long)uxQueueSpacesAvailable(queue);
}

static void msg_ctrl_sdio_dump_queue(const char *name, QueueHandle_t queue)
{
    unsigned long used = msg_ctrl_sdio_queue_used(queue);
    unsigned long free = msg_ctrl_sdio_queue_free(queue);

    printf("  %s: used=%lu free=%lu total=%lu\n", name, used, free, used + free);
}

static void msg_ctrl_sdio_dump_frame_pool(const char *label, const mr_frame_queue_ctrl_t *ctrl)
{
    unsigned long free;
    unsigned long total;
    unsigned long inflight;

    if (ctrl == NULL) {
        printf("  %s: NULL\n", label);
        return;
    }

    free = msg_ctrl_sdio_queue_used(ctrl->pool_queue);
    total = ctrl->frame_elem_cnt;
    inflight = (total >= free) ? (total - free) : 0;

    printf("  %s(%s): free=%lu/%lu inflight=%lu\n",
           label,
           ctrl->name ? ctrl->name : "?",
           free,
           total,
           inflight);
}

static void msg_ctrl_sdio_dump_stream_pool(const char *label, const mr_frame_queue_ctrl_t *ctrl)
{
    unsigned long free;
    unsigned long total;
    unsigned long inflight;

    if (ctrl == NULL) {
        printf("    %-9s: NULL\n", label);
        return;
    }

    free = msg_ctrl_sdio_queue_used(ctrl->pool_queue);
    total = ctrl->frame_elem_cnt;
    inflight = (total >= free) ? (total - free) : 0;

    printf("    %-9s: %s free=%lu/%lu inflight=%lu\n",
           label,
           ctrl->name ? ctrl->name : "?",
           free,
           total,
           inflight);
}

static void msg_ctrl_sdio_dump_tx_head(QueueHandle_t queue)
{
    mr_frame_elem_t *frame_elem = NULL;
    mr_msg_t *msg_pkt;

    if (queue == NULL || xQueuePeek(queue, &frame_elem, 0) != pdTRUE ||
        frame_elem == NULL || frame_elem->buff_addr == NULL) {
        printf("    head: none\n");
        return;
    }

    msg_pkt = (mr_msg_t *)frame_elem->buff_addr;
    printf("    head: stream=%u(%s) raw_len=%u\n",
           msg_pkt->tag,
           msg_ctrl_sdio_stream_name(msg_pkt->tag),
           frame_elem->data_size);
}

static unsigned int msg_ctrl_sdio_queue_head_len(QueueHandle_t queue)
{
    mr_frame_elem_t *frame_elem = NULL;

    if (queue == NULL || xQueuePeek(queue, &frame_elem, 0) != pdTRUE ||
        frame_elem == NULL) {
        return 0;
    }

    return frame_elem->data_size;
}

static void msg_ctrl_sdio_dump_credit_state(const char *name, const mr_credit_t *credit,
                                            bool enabled, bool is_tx)
{
    uint8_t delta;
    unsigned long sem_count = 0;

    if (credit == NULL) {
        printf("    %-9s: NULL\n", name);
        return;
    }

    delta = is_tx ?
        (uint8_t)(credit->credit_limit_cnt - credit->credit_consumed_cnt) :
        (uint8_t)(credit->credit_consumed_cnt - credit->credit_limit_cnt);

    if (credit->wait_sem != NULL) {
        sem_count = (unsigned long)uxSemaphoreGetCount((SemaphoreHandle_t)credit->wait_sem);
    }

    printf("    %-9s: %s limit=%u used=%u %s=%u sem=%lu th=%u\n",
           name,
           enabled ? "on " : "off",
           credit->credit_limit_cnt,
           credit->credit_consumed_cnt,
           is_tx ? "left" : "pending",
           delta,
           sem_count,
           credit->threshold);
}

static void msg_ctrl_sdio_dump_stream_stats(const mr_msg_stream_stat_t *stat)
{
    printf("    data     : pkt tx/rx=%lu/%lu bytes tx/rx=%lu/%lu\n",
           (unsigned long)stat->tx_data_packets,
           (unsigned long)stat->rx_data_packets,
           (unsigned long)stat->tx_data_bytes,
           (unsigned long)stat->rx_data_bytes);
    printf("    ctrl     : pkt tx/rx=%lu/%lu bytes tx/rx=%lu/%lu\n",
           (unsigned long)stat->tx_ctrl_packets,
           (unsigned long)stat->rx_ctrl_packets,
           (unsigned long)stat->tx_ctrl_bytes,
           (unsigned long)stat->rx_ctrl_bytes);
}

static bool msg_ctrl_sdio_handshake_pending(void)
{
    if (g_netdev_wifi_priv != NULL &&
        g_netdev_wifi_priv->netdev_status != MR_NETDEV_DSTA_DEVICE_RUN) {
        return true;
    }

#ifdef CONFIG_MR_TTY
    mr_tty_priv_t *tty_priv = tty_atcmd_priv_get();

    if (tty_priv != NULL && tty_priv->tty_status != MR_TTY_DSTA_DEVICE_RUN) {
        return true;
    }
#endif

    return false;
}

static void msg_ctrl_sdio_dump_netdev_stream(mr_msg_ctrl_priv_t *msg_ctrl, mr_netdev_priv_t *priv)
{
    unsigned long tx_used;
    unsigned long tx_free;
    unsigned long dnld_used;
    unsigned long dnld_free;

    if (priv == NULL) {
        return;
    }

    tx_used = msg_ctrl_sdio_queue_used(msg_ctrl->upld_wait_queue);
    tx_free = msg_ctrl_sdio_queue_free(msg_ctrl->upld_wait_queue);
    dnld_used = msg_ctrl_sdio_queue_used(priv->msg_dnld_queue);
    dnld_free = msg_ctrl_sdio_queue_free(priv->msg_dnld_queue);

    printf("  %d %-7s state=%s(%d) ready=%u tag=%u\n",
           priv->netdev_cfg.msg_tag,
           msg_ctrl_sdio_stream_name(priv->netdev_cfg.msg_tag),
           msg_ctrl_sdio_netdev_state_name(priv->netdev_status),
           priv->netdev_status,
           priv->netdev_ready ? 1U : 0U,
           priv->netdev_cfg.msg_tag);
    printf("    queue    : tx=%lu/%lu free=%lu, rx=%lu/%lu free=%lu\n",
           tx_used,
           tx_used + tx_free,
           tx_free,
           dnld_used,
           dnld_used + dnld_free,
           dnld_free);
    msg_ctrl_sdio_dump_stream_pool("tx_pool", priv->upld_frame_ctrl);
    msg_ctrl_sdio_dump_stream_stats(&msg_ctrl->stream_stats[priv->netdev_cfg.msg_tag]);
    printf("    credit   : tag=%s mask=0x%02X\n",
           priv->flow_ctrl.tag ? priv->flow_ctrl.tag : "?",
           priv->flow_ctrl.enable);
    msg_ctrl_sdio_dump_credit_state("tx", &priv->flow_ctrl.credit[MR_CREDIT_DIR_TX],
                                    (priv->flow_ctrl.enable & MR_CREDIT_EN_TX) != 0U, true);
    msg_ctrl_sdio_dump_credit_state("rx", &priv->flow_ctrl.credit[MR_CREDIT_DIR_RX],
                                    (priv->flow_ctrl.enable & MR_CREDIT_EN_RX) != 0U, false);
}

#ifdef CONFIG_MR_TTY
static void msg_ctrl_sdio_dump_tty_stream(mr_msg_ctrl_priv_t *msg_ctrl, mr_tty_priv_t *priv)
{
    unsigned long tx_used;
    unsigned long tx_free;
    unsigned long dnld_used;
    unsigned long dnld_free;

    if (priv == NULL) {
        return;
    }

    tx_used = msg_ctrl_sdio_queue_used(msg_ctrl->upld_wait_queue);
    tx_free = msg_ctrl_sdio_queue_free(msg_ctrl->upld_wait_queue);
    dnld_used = msg_ctrl_sdio_queue_used(priv->msg_dnld_queue);
    dnld_free = msg_ctrl_sdio_queue_free(priv->msg_dnld_queue);

    printf("  %d %-7s state=%s(%d) ready=%u tag=%u\n",
           priv->tty_cfg.msg_tag,
           msg_ctrl_sdio_stream_name(priv->tty_cfg.msg_tag),
           msg_ctrl_sdio_tty_state_name(priv->tty_status),
           priv->tty_status,
           priv->tty_ready ? 1U : 0U,
           priv->tty_cfg.msg_tag);
    printf("    queue    : tx=%lu/%lu free=%lu, rx=%lu/%lu free=%lu\n",
           tx_used,
           tx_used + tx_free,
           tx_free,
           dnld_used,
           dnld_used + dnld_free,
           dnld_free);
    msg_ctrl_sdio_dump_stream_pool("tx_pool", priv->upld_frame_ctrl);
    msg_ctrl_sdio_dump_stream_stats(&msg_ctrl->stream_stats[priv->tty_cfg.msg_tag]);
    printf("    credit   : tag=%s mask=0x%02X\n",
           priv->flow_ctrl.tag ? priv->flow_ctrl.tag : "?",
           priv->flow_ctrl.enable);
    msg_ctrl_sdio_dump_credit_state("tx", &priv->flow_ctrl.credit[MR_CREDIT_DIR_TX],
                                    (priv->flow_ctrl.enable & MR_CREDIT_EN_TX) != 0U, true);
    msg_ctrl_sdio_dump_credit_state("rx", &priv->flow_ctrl.credit[MR_CREDIT_DIR_RX],
                                    (priv->flow_ctrl.enable & MR_CREDIT_EN_RX) != 0U, false);
}
#endif

void msg_ctrl_sdio_print_dnld_status(void)
{
    mr_frame_queue_ctrl_t *dnld_ctrl;
    int pool_waiting = 0;
    int output_waiting = 0;
    int input_waiting = 0;
    int used_count;

    if (g_msg_sdio_ctrl == NULL || g_msg_sdio_ctrl->dnld_queue_ctrl == NULL) {
        LOG_W("MSG controller not initialized\r\n");
        return;
    }

    dnld_ctrl = g_msg_sdio_ctrl->dnld_queue_ctrl;

    if (dnld_ctrl->pool_queue) {
        pool_waiting = mr_frame_queue_messages_waiting(dnld_ctrl->pool_queue);
    }
    if (dnld_ctrl->output_queue) {
        output_waiting = mr_frame_queue_messages_waiting(dnld_ctrl->output_queue);
    }
    if (dnld_ctrl->input_queue) {
        input_waiting = mr_frame_queue_messages_waiting(dnld_ctrl->input_queue);
    }

    used_count = MSG_DNLD_FRAME_CNT - pool_waiting;

    LOG_I("DNLD Frame Buffer Status (Host->Device):\r\n");
    LOG_I("  - Max buffer count: %u\r\n", MSG_DNLD_FRAME_CNT);
    LOG_I("  - Used buffer count: %u\r\n", used_count);
    LOG_I("  - Free buffer count: %u\r\n", pool_waiting);
    LOG_I("  - Output queue waiting: %u\r\n", output_waiting);
    LOG_I("  - Input queue waiting: %u\r\n", input_waiting);
    LOG_I("  - Usage rate: %u%%\r\n", (used_count * 100) / MSG_DNLD_FRAME_CNT);
}

static void msg_ctrl_sdio_dump_status(void)
{
    mr_msg_ctrl_priv_t *msg_ctrl = g_msg_sdio_ctrl;
    mr_frame_queue_ctrl_t *dnld_ctrl;
    unsigned long tx_used = 0;
    unsigned long tx_free = 0;
    const char *bus_name = "?";

    printf("=== Device nh Status ===\n");

    if (msg_ctrl == NULL) {
        printf("[bus]\n");
        printf("  id=? flags=0x00 rx=0 tx=0 handshake=0 recover=0 rx_len=0 mtu=0\n");
        printf("[tx]\n");
        printf("  data_queue: used=0 free=0 total=0\n");
        printf("    head: none\n");
        printf("[pools]\n");
        printf("  msg_ctrl: NULL\n");
        printf("[streams]\n");
        printf("=== End ===\n");
        return;
    }

    dnld_ctrl = msg_ctrl->dnld_queue_ctrl;
    if (msg_ctrl->cfg.dev_ops != NULL && msg_ctrl->cfg.dev_ops->name != NULL) {
        bus_name = msg_ctrl->cfg.dev_ops->name;
    }

    if (msg_ctrl->upld_wait_queue != NULL) {
        tx_used = msg_ctrl_sdio_queue_used(msg_ctrl->upld_wait_queue);
        tx_free = msg_ctrl_sdio_queue_free(msg_ctrl->upld_wait_queue);
    }

    printf("[bus]\n");
    printf("  id=%s flags=0x%02X rx=%u tx=%u handshake=%u recover=%u rx_len=%u mtu=%u\n",
           bus_name,
           0U,
           msg_ctrl_sdio_queue_used(msg_ctrl->dnld_done_queue) != 0U ? 1U : 0U,
           tx_used != 0U ? 1U : 0U,
           msg_ctrl_sdio_handshake_pending() ? 1U : 0U,
           0U,
           msg_ctrl_sdio_queue_head_len(msg_ctrl->dnld_done_queue),
           dnld_ctrl != NULL ? (unsigned int)(dnld_ctrl->frame_elem_size - FRAME_BUFF_HEADER_ROOM) : 0U);

    printf("[tx]\n");
    printf("  data_queue: used=%lu free=%lu total=%lu\n", tx_used, tx_free, tx_used + tx_free);
    msg_ctrl_sdio_dump_tx_head(msg_ctrl->upld_wait_queue);
    msg_ctrl_sdio_dump_queue("upld_done_queue", msg_ctrl->upld_done_queue);

    printf("[pools]\n");
    msg_ctrl_sdio_dump_frame_pool("dnld", dnld_ctrl);
    msg_ctrl_sdio_dump_queue("dnld_done_queue", msg_ctrl->dnld_done_queue);
    if (dnld_ctrl != NULL) {
        msg_ctrl_sdio_dump_queue("dnld_pool_queue", dnld_ctrl->pool_queue);
        msg_ctrl_sdio_dump_queue("dnld_output_queue", dnld_ctrl->output_queue);
        msg_ctrl_sdio_dump_queue("dnld_input_queue", dnld_ctrl->input_queue);
    }

    printf("[streams]\n");
    msg_ctrl_sdio_dump_netdev_stream(msg_ctrl, g_netdev_wifi_priv);
#ifdef CONFIG_MR_TTY
    msg_ctrl_sdio_dump_tty_stream(msg_ctrl, tty_atcmd_priv_get());
#endif

    printf("=== End ===\n");
}

static int cmd_nh_dump(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == 'r') {
        mr_dnld_profile_reset();
        printf("nh dnld profile reset\n");
        return 0;
    }

    msg_ctrl_sdio_dump_status();
    mr_dnld_profile_dump();
    return 0;
}
SHELL_CMD_EXPORT_ALIAS(cmd_nh_dump, nh_dump, show device nethub status);
