#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

#include "FreeRTOS.h"
#include "task.h"

#include "qcc74x_irq.h"
#include "qcc74x_mtimer.h"

#include "mr_frame_buff_ctrl.h"
#include "mr_netdev.h"

#include "msg_ctrl_sdio.h"
#include "netdev_wifi.h"
#include "nh_hub.h"

#define DBG_TAG "NETDEV_STA"
#include "log.h"

typedef struct {
    uint64_t bytes;
    uint32_t packets;
} rcp_test_stat_t;

typedef struct {
    uint8_t mode;
    uint32_t duration_ms;
    uint32_t packet_len;
    uint32_t start_ms;
    uint32_t end_ms;
    uint32_t last_report_ms;
    volatile bool running;
    volatile bool stop_requested;
    TaskHandle_t worker_task;
    rcp_test_stat_t tx;
    rcp_test_stat_t rx;
} rcp_test_ctx_t;

#define RCP_TEST_MODE_TX            (1U << 0)
#define RCP_TEST_MODE_RX            (1U << 1)
#define RCP_TEST_REPORT_MS          1000U
#define RCP_TEST_DEFAULT_DURATION_S 999999U
#define RCP_TEST_DEFAULT_MS         (RCP_TEST_DEFAULT_DURATION_S * 1000U)
#define RCP_TEST_RX_IDLE_MS         10U
#define RCP_TEST_PROTO_SIZE         ((uint32_t)sizeof(mr_netdev_msg_t))
#define RCP_TEST_TAG_SIZE           ((uint32_t)sizeof(g_rcp_test_magic))
#define RCP_TEST_MIN_PACKET_LEN     (RCP_TEST_PROTO_SIZE + RCP_TEST_TAG_SIZE)
#define RCP_TEST_MAX_PACKET_LEN     1536U
#define RCP_TEST_DEFAULT_PACKET_LEN RCP_TEST_MAX_PACKET_LEN
#define RCP_TEST_TASK_STACK         2048U
#define RCP_TEST_TASK_PRIO          11U
#define RCP_TEST_TX_ERR_NOT_READY   (-2)
#define RCP_TEST_TX_ERR_NO_MEM      (-3)
#define RCP_TEST_TX_ERR_SMALL_BUF   (-4)

static const uint8_t g_rcp_test_magic[8] = { 'S', 'D', 'I', 'O', 'P', 'E', 'R', 'F' };
static rcp_test_ctx_t g_rcp_test;

static uint32_t rcp_test_now_ms(void)
{
    return (uint32_t)qcc74x_mtimer_get_time_ms();
}

static unsigned long long rcp_test_bytes_to_mbps_x100(uint64_t bytes, uint32_t elapsed_ms)
{
    if (elapsed_ms == 0U) {
        return 0U;
    }

    return (bytes * 8ULL * 100ULL) / ((uint64_t)elapsed_ms * 1000ULL);
}

static bool rcp_test_has_tx(uint8_t mode)
{
    return (mode & RCP_TEST_MODE_TX) != 0U;
}

static bool rcp_test_has_rx(uint8_t mode)
{
    return (mode & RCP_TEST_MODE_RX) != 0U;
}

static void rcp_test_reset(void)
{
    memset(&g_rcp_test, 0, sizeof(g_rcp_test));
}

static bool rcp_test_parse_packet(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len < RCP_TEST_TAG_SIZE) {
        return false;
    }

    return memcmp(data, g_rcp_test_magic, RCP_TEST_TAG_SIZE) == 0;
}

static void rcp_test_stat_add(rcp_test_stat_t *stat, uint32_t bytes)
{
    uintptr_t flags;

    if (stat == NULL) {
        return;
    }

    flags = qcc74x_irq_save();
    stat->bytes += bytes;
    stat->packets++;
    qcc74x_irq_restore(flags);
}

static void rcp_test_snapshot(rcp_test_stat_t *tx, rcp_test_stat_t *rx)
{
    uintptr_t flags = qcc74x_irq_save();

    if (tx != NULL) {
        *tx = g_rcp_test.tx;
    }
    if (rx != NULL) {
        *rx = g_rcp_test.rx;
    }

    qcc74x_irq_restore(flags);
}

static void rcp_test_print_dir(const char *label, uint64_t bytes,
                               uint32_t packets, uint32_t elapsed_ms)
{
    unsigned long long mbps_x100 = rcp_test_bytes_to_mbps_x100(bytes, elapsed_ms);

    printf("%s %lu.%02lu Mbps %lu pkts\r\n",
           label,
           (unsigned long)(mbps_x100 / 100ULL),
           (unsigned long)(mbps_x100 % 100ULL),
           (unsigned long)packets);
}

static void rcp_test_print_interval(uint32_t start_ms, uint32_t end_ms,
                                    const rcp_test_stat_t *prev_tx,
                                    const rcp_test_stat_t *prev_rx,
                                    const rcp_test_stat_t *curr_tx,
                                    const rcp_test_stat_t *curr_rx)
{
    uint32_t elapsed_ms;

    if (end_ms <= start_ms) {
        return;
    }

    elapsed_ms = end_ms - start_ms;
    if (rcp_test_has_tx(g_rcp_test.mode)) {
        printf("[ %4lu.%03lu-%4lu.%03lu sec ] ",
               (unsigned long)(start_ms / 1000U),
               (unsigned long)(start_ms % 1000U),
               (unsigned long)(end_ms / 1000U),
               (unsigned long)(end_ms % 1000U));
        rcp_test_print_dir("TX",
                           curr_tx->bytes - prev_tx->bytes,
                           curr_tx->packets - prev_tx->packets,
                           elapsed_ms);
    }
    if (rcp_test_has_rx(g_rcp_test.mode)) {
        printf("[ %4lu.%03lu-%4lu.%03lu sec ] ",
               (unsigned long)(start_ms / 1000U),
               (unsigned long)(start_ms % 1000U),
               (unsigned long)(end_ms / 1000U),
               (unsigned long)(end_ms % 1000U));
        rcp_test_print_dir("RX",
                           curr_rx->bytes - prev_rx->bytes,
                           curr_rx->packets - prev_rx->packets,
                           elapsed_ms);
    }
}

static void rcp_test_print_summary(void)
{
    rcp_test_stat_t tx = { 0 };
    rcp_test_stat_t rx = { 0 };
    uint32_t elapsed_ms;

    rcp_test_snapshot(&tx, &rx);
    elapsed_ms = (g_rcp_test.end_ms > g_rcp_test.start_ms) ?
                 (g_rcp_test.end_ms - g_rcp_test.start_ms) : 0U;

    printf("SDIO ETH test complete\r\n");
    if (rcp_test_has_tx(g_rcp_test.mode)) {
        rcp_test_print_dir("TX avg", tx.bytes, tx.packets, elapsed_ms);
    }
    if (rcp_test_has_rx(g_rcp_test.mode)) {
        rcp_test_print_dir("RX avg", rx.bytes, rx.packets, elapsed_ms);
    }
}

static void rcp_test_tx_frame_release(mr_frame_elem_t *frame_elem)
{
    mr_netdev_priv_t *priv = g_netdev_wifi_priv;
    mr_msg_ctrl_priv_t *msg_ctrl = (priv != NULL) ? priv->netdev_cfg.msg_ctrl : NULL;

    if (frame_elem != NULL && msg_ctrl != NULL && msg_ctrl->dnld_queue_ctrl != NULL) {
        frame_elem->frame_ctrl = msg_ctrl->dnld_queue_ctrl;
        frame_elem->buff_addr = (void *)((uintptr_t)frame_elem + FRAME_BUFF_HEADER_ROOM);
        frame_elem->buff_size = msg_ctrl->dnld_queue_ctrl->frame_elem_size - FRAME_BUFF_HEADER_ROOM;
        (void)mr_msg_ctrl_dnld_free(msg_ctrl, frame_elem);
    }
}

static void rcp_test_tx_frame_free(void *arg)
{
    rcp_test_tx_frame_release((mr_frame_elem_t *)arg);
}

static int rcp_test_send_tx_packet(uint32_t payload_len)
{
    mr_netdev_priv_t *priv = g_netdev_wifi_priv;
    mr_msg_ctrl_priv_t *msg_ctrl;
    uint32_t packet_len = payload_len + RCP_TEST_PROTO_SIZE;
    mr_frame_elem_t *frame_elem = NULL;
    nethub_frame_t *frame_record;
    mr_netdev_msg_t *netdev_msg_pkt;
    int ret;

    if (priv == NULL || priv->netdev_status != MR_NETDEV_DSTA_DEVICE_RUN) {
        return RCP_TEST_TX_ERR_NOT_READY;
    }
    msg_ctrl = priv->netdev_cfg.msg_ctrl;
    if (msg_ctrl == NULL) {
        return RCP_TEST_TX_ERR_NOT_READY;
    }

    ret = mr_msg_ctrl_dnld_alloc(msg_ctrl, &frame_elem, portMAX_DELAY);
    if (ret < 0 || frame_elem == NULL) {
        return RCP_TEST_TX_ERR_NO_MEM;
    }

    if (frame_elem->buff_size < packet_len) {
        rcp_test_tx_frame_release(frame_elem);
        return RCP_TEST_TX_ERR_SMALL_BUF;
    }

    netdev_msg_pkt = MR_NETDEV_FRAME_ELEM_TO_MSG_PACKET_ADDR(frame_elem);
    frame_record = (nethub_frame_t *)((uintptr_t)frame_elem - sizeof(nethub_frame_t));

    frame_record->data = netdev_msg_pkt->data;
    frame_record->len = payload_len;
    frame_record->free_cb = rcp_test_tx_frame_free;
    frame_record->cb_arg = frame_elem;
    frame_record->next = NULL;

    frame_elem->frame_ctrl = NULL;
    frame_elem->buff_addr = (void *)netdev_msg_pkt;

    netdev_msg_pkt->msg_pkt.tag = priv->netdev_cfg.msg_tag;
    netdev_msg_pkt->msg_pkt.sub_tag = 0;
    netdev_msg_pkt->flag = MR_NETDEV_FLAG_UPLD_DATA;
    netdev_msg_pkt->credit_update_flag = false;
    netdev_msg_pkt->credit_limit_cnt = 0;
    MR_NETDEV_MSG_PACKET_SET_DATA_SIZE(netdev_msg_pkt, payload_len);

    memcpy(netdev_msg_pkt->data, g_rcp_test_magic, RCP_TEST_TAG_SIZE);

    ret = mr_netdev_upld_elem_send(priv, netdev_msg_pkt);
    if (ret < 0) {
        rcp_test_tx_frame_free(frame_elem);
        return ret;
    }

    return 0;
}

bool netdev_wifi_rcp_test_consume(mr_netdev_priv_t *priv, mr_netdev_msg_t *netdev_msg_pkt)
{
    uint32_t data_len;

    if (!g_rcp_test.running || !rcp_test_has_rx(g_rcp_test.mode)) {
        return false;
    }

    data_len = MR_NETDEV_MSG_PACKET_GET_DATA_SIZE(netdev_msg_pkt);
    if (rcp_test_parse_packet(netdev_msg_pkt->data, data_len)) {
        rcp_test_stat_add(&g_rcp_test.rx, data_len + RCP_TEST_PROTO_SIZE);
    }

    mr_netdev_dnld_elem_free(priv, netdev_msg_pkt);
    return true;
}

static void rcp_test_worker(void *arg)
{
    rcp_test_stat_t prev_tx = { 0 };
    rcp_test_stat_t prev_rx = { 0 };
    rcp_test_stat_t curr_tx = { 0 };
    rcp_test_stat_t curr_rx = { 0 };
    uint32_t packet_len = 0U;
    uint32_t payload_len = 0U;

    NETHUB_UNUSED(arg);

    if (rcp_test_has_tx(g_rcp_test.mode)) {
        packet_len = g_rcp_test.packet_len;
        payload_len = packet_len - RCP_TEST_PROTO_SIZE;
    }

    while (!g_rcp_test.stop_requested) {
        uint32_t now = rcp_test_now_ms();

        if (g_rcp_test.duration_ms != 0U &&
            (now - g_rcp_test.start_ms) >= g_rcp_test.duration_ms) {
            break;
        }

        if (rcp_test_has_tx(g_rcp_test.mode)) {
            int ret = rcp_test_send_tx_packet(payload_len);

            if (ret != 0) {
                printf("SDIO ETH test TX stopped, netdev_wifi_upld=%d\r\n", ret);
                break;
            } else {
                rcp_test_stat_add(&g_rcp_test.tx, packet_len);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(RCP_TEST_RX_IDLE_MS));
        }

        now = rcp_test_now_ms();
        if ((now - g_rcp_test.last_report_ms) < RCP_TEST_REPORT_MS) {
            continue;
        }

        rcp_test_snapshot(&curr_tx, &curr_rx);
        rcp_test_print_interval(g_rcp_test.last_report_ms,
                                now,
                                &prev_tx,
                                &prev_rx,
                                &curr_tx,
                                &curr_rx);
        prev_tx = curr_tx;
        prev_rx = curr_rx;
        g_rcp_test.last_report_ms = now;
    }

    g_rcp_test.end_ms = rcp_test_now_ms();
    rcp_test_snapshot(&curr_tx, &curr_rx);
    rcp_test_print_interval(g_rcp_test.last_report_ms,
                            g_rcp_test.end_ms,
                            &prev_tx,
                            &prev_rx,
                            &curr_tx,
                            &curr_rx);
    rcp_test_print_summary();
    g_rcp_test.running = false;
    g_rcp_test.stop_requested = false;
    g_rcp_test.worker_task = NULL;
    vTaskDelete(NULL);
}

static void rcp_test_dump_status(void)
{
    printf("SDIO ETH test: %s\r\n", g_rcp_test.running ? "running" : "stopped");
    if (!g_rcp_test.running) {
        return;
    }

    printf("  mode: %s%s\r\n",
           rcp_test_has_tx(g_rcp_test.mode) ? "tx" : "",
           (rcp_test_has_tx(g_rcp_test.mode) && rcp_test_has_rx(g_rcp_test.mode)) ? "+rx" :
           (rcp_test_has_rx(g_rcp_test.mode) ? "rx" : ""));
    if (g_rcp_test.duration_ms != 0U) {
        printf("  duration: %lu ms\r\n", (unsigned long)g_rcp_test.duration_ms);
    }
    if (rcp_test_has_tx(g_rcp_test.mode)) {
        printf("  packet: %lu bytes\r\n", (unsigned long)g_rcp_test.packet_len);
    }
}

static void rcp_test_help(void)
{
    printf("Usage:\r\n");
    printf("  rcp_test tx [duration_s] [packet_len]\r\n");
    printf("  rcp_test rx [duration_s]\r\n");
    printf("  rcp_test txrx [duration_s] [packet_len]\r\n");
    printf("  rcp_test stop\r\n");
    printf("  rcp_test status\r\n");
    printf("Notes:\r\n");
    printf("  default duration for all modes: %lu s\r\n",
           (unsigned long)RCP_TEST_DEFAULT_DURATION_S);
    printf("  default packet: %lu bytes (whole test packet)\r\n",
           (unsigned long)RCP_TEST_DEFAULT_PACKET_LEN);
}

static int rcp_test_parse_u32(const char *arg, uint32_t *value)
{
    char *endptr = NULL;
    unsigned long parsed;

    if (arg == NULL || value == NULL || arg[0] == '\0') {
        return -1;
    }

    parsed = strtoul(arg, &endptr, 0);
    if (parsed > UINT32_MAX || endptr == NULL || endptr[0] != '\0') {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int rcp_test_parse_duration_ms(const char *arg, uint32_t *duration_ms)
{
    uint32_t duration_s;

    if (duration_ms == NULL || rcp_test_parse_u32(arg, &duration_s) != 0) {
        return -1;
    }
    if (duration_s > (UINT32_MAX / 1000U)) {
        return -1;
    }

    *duration_ms = duration_s * 1000U;
    return 0;
}

static int rcp_test_start(uint8_t mode, uint32_t duration_ms, uint32_t packet_len)
{
    mr_netdev_priv_t *priv = g_netdev_wifi_priv;
    int ret;

    if (!rcp_test_has_tx(mode) && !rcp_test_has_rx(mode)) {
        return -1;
    }
    if (g_rcp_test.running || g_rcp_test.worker_task != NULL) {
        return -2;
    }
    if (priv == NULL || priv->netdev_status != MR_NETDEV_DSTA_DEVICE_RUN) {
        return -3;
    }
    if (duration_ms == 0U) {
        duration_ms = RCP_TEST_DEFAULT_MS;
    }

    if (rcp_test_has_tx(mode)) {
        if (packet_len == 0U) {
            packet_len = RCP_TEST_DEFAULT_PACKET_LEN;
        }
        if (packet_len < RCP_TEST_MIN_PACKET_LEN ||
            packet_len > RCP_TEST_MAX_PACKET_LEN) {
            return -1;
        }
    }

    rcp_test_reset();
    g_rcp_test.mode = mode;
    g_rcp_test.duration_ms = duration_ms;
    g_rcp_test.packet_len = packet_len;
    g_rcp_test.start_ms = rcp_test_now_ms();
    g_rcp_test.last_report_ms = g_rcp_test.start_ms;
    g_rcp_test.running = true;

    ret = xTaskCreate(rcp_test_worker,
                      "sdio_test",
                      RCP_TEST_TASK_STACK,
                      NULL,
                      RCP_TEST_TASK_PRIO,
                      &g_rcp_test.worker_task);
    if (ret == pdPASS) {
        printf("SDIO ETH test started: mode=%s%s",
               rcp_test_has_tx(mode) ? "tx" : "",
               (rcp_test_has_tx(mode) && rcp_test_has_rx(mode)) ? "+rx" :
               (rcp_test_has_rx(mode) ? "rx" : ""));
        if (duration_ms != 0U) {
            printf(", duration=%lu ms", (unsigned long)duration_ms);
        }
        if (rcp_test_has_tx(mode)) {
            printf(", packet=%lu bytes", (unsigned long)packet_len);
        }
        printf("\r\n");
        return 0;
    }

    rcp_test_reset();
    return -4;
}

static int rcp_test_stop(void)
{
    if (!g_rcp_test.running) {
        return -1;
    }

    g_rcp_test.stop_requested = true;
    return 0;
}

static int cmd_netdev_info(int argc, const char *argv[])
{
    mr_netdev_priv_t *priv = g_netdev_wifi_priv;

    NETHUB_UNUSED(argc);
    NETHUB_UNUSED(argv);

    if (!priv) {
        LOG_E("netdev not initialized\r\n");
        return -1;
    }

    printf("\r\n========== NETDEV Private Data ==========\r\n");

    printf("\r\n【配置信息】\r\n");
    printf("  name: %s\r\n", priv->netdev_cfg.name ? priv->netdev_cfg.name : "NULL");
    printf("  msg_ctrl: %p\r\n", priv->netdev_cfg.msg_ctrl);
    printf("  msg_tag: 0x%02X\r\n", priv->netdev_cfg.msg_tag);

    printf("\r\n【上传缓冲区配置】\r\n");
    printf("  upld_frame_size: %u bytes\r\n", priv->netdev_cfg.upld_frame_size);
    printf("  upld_frame_count: %u\r\n", priv->netdev_cfg.upld_frame_count);
    printf("  upld_frame_buff: %p\r\n", priv->netdev_cfg.upld_frame_buff);
    printf("  upld_frame_type: %u\r\n", priv->netdev_cfg.upld_frame_type);

    printf("\r\n【流控配置】\r\n");
    printf("  dnld_credit_max: %u\r\n", priv->netdev_cfg.dnld_credit_max);
    printf("  upld_credit_update_threshold: %u\r\n", priv->netdev_cfg.upld_credit_update_threshold);

    printf("\r\n【回调函数】\r\n");
    printf("  dnld_output_cb: %p\r\n", priv->netdev_cfg.dnld_output_cb);
    printf("  upld_done_cb: %p\r\n", priv->netdev_cfg.upld_done_cb);
    printf("  netdev_event: %p\r\n", priv->netdev_cfg.netdev_event);
    printf("  netdev_reset_cb: %p\r\n", priv->netdev_cfg.netdev_reset_cb);

    printf("\r\n【任务配置】\r\n");
    printf("  task_stack_size: %d\r\n", priv->netdev_cfg.task_stack_size);
    printf("  task_priority: %d\r\n", priv->netdev_cfg.task_priority);
    printf("  task_period_max_ms: %u\r\n", priv->netdev_cfg.task_period_max_ms);
    printf("  netdev_task_cb: %p\r\n", priv->netdev_cfg.netdev_task_cb);

    printf("\r\n【网络信息】\r\n");
    printf("  link_up_flag: %s\r\n", priv->netdev_cfg.link_up_flag ? "UP" : "DOWN");
    printf("  mac: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           priv->netdev_cfg.mac[0], priv->netdev_cfg.mac[1], priv->netdev_cfg.mac[2],
           priv->netdev_cfg.mac[3], priv->netdev_cfg.mac[4], priv->netdev_cfg.mac[5]);
    printf("  ip: %u.%u.%u.%u\r\n",
           priv->netdev_cfg.ip[0], priv->netdev_cfg.ip[1],
           priv->netdev_cfg.ip[2], priv->netdev_cfg.ip[3]);

    printf("\r\n【运行时状态】\r\n");
    printf("  upld_frame_ctrl: %p\r\n", priv->upld_frame_ctrl);
    printf("  netdev_status: %d", priv->netdev_status);
    switch (priv->netdev_status) {
        case MR_NETDEV_DSTA_ERROR:
            printf(" (ERROR)\r\n");
            break;
        case MR_NETDEV_DSTA_IDLE:
            printf(" (IDLE)\r\n");
            break;
        case MR_NETDEV_DSTA_HOST_READY:
            printf(" (HOST_READY)\r\n");
            break;
        case MR_NETDEV_DSTA_DEVICE_RUN:
            printf(" (DEVICE_RUN)\r\n");
            break;
        default:
            printf(" (UNKNOWN)\r\n");
            break;
    }

    printf("\r\n【流控状态】\r\n");
    printf("  enable_mask: 0x%02X\r\n", priv->flow_ctrl.enable);
    printf("  tx_credit: init=%u limit=%u consumed=%u threshold=%u\r\n",
           priv->flow_ctrl.credit[MR_CREDIT_DIR_TX].init_credit,
           priv->flow_ctrl.credit[MR_CREDIT_DIR_TX].credit_limit_cnt,
           priv->flow_ctrl.credit[MR_CREDIT_DIR_TX].credit_consumed_cnt,
           priv->flow_ctrl.credit[MR_CREDIT_DIR_TX].threshold);
    printf("  rx_credit: init=%u advertised=%u available=%u threshold=%u\r\n",
           priv->flow_ctrl.credit[MR_CREDIT_DIR_RX].init_credit,
           priv->flow_ctrl.credit[MR_CREDIT_DIR_RX].credit_limit_cnt,
           priv->flow_ctrl.credit[MR_CREDIT_DIR_RX].credit_consumed_cnt,
           priv->flow_ctrl.credit[MR_CREDIT_DIR_RX].threshold);

    printf("\r\n【任务信息】\r\n");
    printf("  netdev_proc_task: %p\r\n", priv->netdev_proc_task);
    printf("  msg_dnld_queue: %p\r\n", priv->msg_dnld_queue);
    printf("  stop_requested: %s\r\n", priv->stop_requested ? "true" : "false");

    printf("\r\n【就绪状态】\r\n");
    printf("  netdev_ready: %s\r\n", priv->netdev_ready ? "true" : "false");

    printf("\r\n==========================================\r\n");

    msg_ctrl_sdio_print_dnld_status();

    return 0;
}
SHELL_CMD_EXPORT_ALIAS(cmd_netdev_info, netdev_info, show netdev private data);

int cmd_rcp_test(int argc, char **argv)
{
    uint8_t mode = 0U;
    uint32_t duration_ms = 0U;
    uint32_t packet_len = 0U;
    int ret;

    if (argc < 2) {
        rcp_test_help();
        return -1;
    }

    if (strcmp(argv[1], "help") == 0) {
        rcp_test_help();
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        rcp_test_dump_status();
        return 0;
    }
    if (strcmp(argv[1], "stop") == 0) {
        ret = rcp_test_stop();
        if (ret != 0) {
            printf("sdioperf: no test is running\r\n");
        }
        return ret;
    }

    if (strcmp(argv[1], "tx") == 0) {
        mode = RCP_TEST_MODE_TX;
    } else if (strcmp(argv[1], "rx") == 0) {
        mode = RCP_TEST_MODE_RX;
    } else if (strcmp(argv[1], "txrx") == 0) {
        mode = RCP_TEST_MODE_TX | RCP_TEST_MODE_RX;
    } else {
        rcp_test_help();
        return -1;
    }

    if (argc >= 3) {
        if (rcp_test_parse_duration_ms(argv[2], &duration_ms) != 0) {
            printf("Invalid duration: %s\r\n", argv[2]);
            return -1;
        }
    }
    if (rcp_test_has_tx(mode) && argc >= 4) {
        if (rcp_test_parse_u32(argv[3], &packet_len) != 0) {
            printf("Invalid packet length: %s\r\n", argv[3]);
            return -1;
        }
    }
    if ((rcp_test_has_tx(mode) && argc > 4) ||
        (!rcp_test_has_tx(mode) && argc > 3)) {
        rcp_test_help();
        return -1;
    }

    ret = rcp_test_start(mode, duration_ms, packet_len);
    if (ret == -1) {
        printf("sdioperf: invalid mode or packet length\r\n");
    } else if (ret == -2) {
        printf("sdioperf: test is already running\r\n");
    } else if (ret == -3) {
        printf("sdioperf: ETH transport is not ready\r\n");
    } else if (ret == -4) {
        printf("sdioperf: failed to create worker task\r\n");
    }

    return ret;
}
SHELL_CMD_EXPORT_ALIAS(cmd_rcp_test, rcp_test, SDIO ETH throughput test);
