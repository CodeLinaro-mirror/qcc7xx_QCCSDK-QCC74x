#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "qcc74x_mtimer.h"

#include "FreeRTOS.h"
#include "task.h"

#include "shell.h"
#include "transportsdio.h"
#include "nh_msg_ctrl.h"
#include "nh_sdio_drv.h"
#include "wifi_mgmr_ext.h"

#ifdef USE_SDIO2_DRIVER
#include "qcc74x_sdio2.h"
extern struct qcc74x_device_s *sdio2_hd;
extern volatile bool sdio2_ready_flag;
#elif defined(USE_SDIO3_DRIVER)
#include <nh_sdio3.h>
extern struct nh_device_s *sdio3_hd;
extern volatile bool sdio3_ready_flag;
#endif

#define TRANSPORTSDIO_PERF_INTERVAL_MS        1000U
#define TRANSPORTSDIO_PERF_DEFAULT_DURATION_MS 10000U
#define TRANSPORTSDIO_PERF_TX_DEFAULT_PAYLOAD 1500U
#define TRANSPORTSDIO_PERF_TX_TASK_STACK      2048U
#define TRANSPORTSDIO_PERF_TX_TASK_PRIO       11U
#define TRANSPORTSDIO_PERF_TX_STOP_WAIT_MS    3000U

typedef struct {
    uint32_t start_ms;
    uint32_t interval_start_ms;
    uint32_t last_ms;
    uint32_t packets;
    uint32_t interval_packets;
    uint64_t total_bytes;
    uint64_t interval_bytes;
} transportsdio_perf_stat_t;

typedef struct {
    volatile bool initialized;
    volatile bool rx_running;
    volatile bool tx_running;
    volatile bool tx_stop_requested;
    TaskHandle_t tx_task;
    nh_msg_ctrl_t *msg_ctrl;
    uint32_t tx_payload_len;
    uint32_t tx_sequence;
    uint32_t tx_send_failures;
    volatile int tx_last_result;
    transportsdio_perf_stat_t rx_stat;
    transportsdio_perf_stat_t tx_stat;
} transportsdio_perf_ctx_t;

static transportsdio_perf_ctx_t g_transportsdio_perf;

static uint32_t transportsdio_perf_now_ms(void)
{
    return (uint32_t)qcc74x_mtimer_get_time_ms();
}

static unsigned long long transportsdio_perf_bytes_to_mbps_x100(uint64_t bytes, uint32_t elapsed_ms)
{
    if (0U == elapsed_ms) {
        return 0U;
    }

    return (bytes * 8ULL * 100ULL) / ((uint64_t)elapsed_ms * 1000ULL);
}

static void transportsdio_perf_stat_reset(transportsdio_perf_stat_t *stat)
{
    if (NULL == stat) {
        return;
    }

    memset(stat, 0, sizeof(*stat));
}

static void transportsdio_perf_print_interval(const char *label,
                                              uint32_t start_ms,
                                              uint32_t end_ms,
                                              uint64_t bytes,
                                              uint32_t packets)
{
    unsigned long long mbps_x100 = transportsdio_perf_bytes_to_mbps_x100(bytes, end_ms - start_ms);

    printf("[ %4lu.%03lu-%4lu.%03lu sec ] %s %4llu.%02llu Mbps %4lu pkts\r\n",
           (unsigned long)(start_ms / 1000U),
           (unsigned long)(start_ms % 1000U),
           (unsigned long)(end_ms / 1000U),
           (unsigned long)(end_ms % 1000U),
           label,
           mbps_x100 / 100ULL,
           mbps_x100 % 100ULL,
           (unsigned long)packets);
}

static void transportsdio_perf_print_summary(const char *label, const transportsdio_perf_stat_t *stat)
{
    unsigned long long mbps_x100;
    uint32_t end_ms;

    if (NULL == stat) {
        printf("%s 0.00 Mbps 0 pkts\r\n", label);
        return;
    }
    if (0U == stat->start_ms) {
        printf("%s 0.00 Mbps 0 pkts\r\n", label);
        return;
    }

    end_ms = stat->last_ms;
    if (end_ms < stat->start_ms) {
        end_ms = stat->start_ms;
    }

    mbps_x100 = transportsdio_perf_bytes_to_mbps_x100(stat->total_bytes, end_ms - stat->start_ms);
    printf("%s %llu.%02llu Mbps %lu pkts\r\n",
           label,
           mbps_x100 / 100ULL,
           mbps_x100 % 100ULL,
           (unsigned long)stat->packets);
}

static void transportsdio_perf_stat_update(transportsdio_perf_stat_t *stat, const char *label, uint32_t bytes)
{
    uint32_t now_ms;

    if (NULL == stat) {
        return;
    }
    if (0U == bytes) {
        return;
    }

    now_ms = transportsdio_perf_now_ms();
    if (0U == stat->start_ms) {
        stat->start_ms = now_ms;
        stat->interval_start_ms = now_ms;
    }

    stat->last_ms = now_ms;
    stat->interval_bytes += bytes;
    stat->total_bytes += bytes;
    stat->packets += 1U;
    stat->interval_packets += 1U;

    if ((now_ms - stat->interval_start_ms) < TRANSPORTSDIO_PERF_INTERVAL_MS) {
        return;
    }

    transportsdio_perf_print_interval(label,
                                      stat->interval_start_ms,
                                      now_ms,
                                      stat->interval_bytes,
                                      stat->interval_packets);

    stat->interval_start_ms = now_ms;
    stat->interval_bytes = 0U;
    stat->interval_packets = 0U;
}

static void transportsdio_perf_fill_payload(uint8_t *payload, uint32_t payload_len, uint32_t sequence)
{
    if (NULL == payload) {
        return;
    }
    if (0U == payload_len) {
        return;
    }

    memset(payload, (uint8_t)(sequence & 0xFFU), payload_len);
    if (payload_len >= sizeof(sequence)) {
        memcpy(payload, &sequence, sizeof(sequence));
    } else {
        memcpy(payload, &sequence, payload_len);
    }
}

static int transportsdio_perf_transport_ready(void)
{
    uint8_t mac_addr[6];

    if (false == g_transportsdio_perf.initialized) {
        return 0;
    }

    if (0 == transportsdio_get_mac_addr(mac_addr)) {
        return 1;
    }

    return 0;
}

static int transportsdio_perf_transport_busy(void)
{
    uint8_t mac_addr[6];

    if (g_transportsdio_perf.initialized) {
        return 0;
    }

    if (0 == transportsdio_get_mac_addr(mac_addr)) {
        return 1;
    }

    return 0;
}

static void transportsdio_perf_sdio_hw_deinit(void)
{
#ifdef USE_SDIO2_DRIVER
    if (NULL != sdio2_hd) {
        qcc74x_sdio2_deinit(sdio2_hd);
        sdio2_hd = NULL;
        sdio2_ready_flag = false;
    }
#elif defined(USE_SDIO3_DRIVER)
    if (NULL != sdio3_hd) {
        nh_sdio3_deinit(sdio3_hd);
        sdio3_hd = NULL;
        sdio3_ready_flag = false;
    }
#endif
}

static int transportsdio_perf_wifi_send_cb(struct wifi_wifista_priv *ctx,
                                           void *buf,
                                           uint32_t buf_len,
                                           void (*free_cb)(void *arg),
                                           void *cb_arg)
{
    (void)ctx;
    (void)buf;

    if (free_cb) {
        free_cb(cb_arg);
    }

    if (g_transportsdio_perf.rx_running) {
        transportsdio_perf_stat_update(&g_transportsdio_perf.rx_stat, "SDIO RX", buf_len);
    }

    return 0;
}

static void transportsdio_perf_wait_ms(uint32_t wait_ms)
{
    uint32_t waited_ms = 0U;

    while (waited_ms < wait_ms) {
        uint32_t delay_ms = wait_ms - waited_ms;

        if (delay_ms > 10U) {
            delay_ms = 10U;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        waited_ms += delay_ms;
    }
}

int transportsdio_perf_init(void)
{
    int ret;
    nh_msg_ctrl_ops_t *sdio_ops = NULL;
    wifi_wifista_priv_t *ctx = NULL;

    if (g_transportsdio_perf.initialized) {
        return -1;
    }
    if (transportsdio_perf_transport_busy()) {
        return -7;
    }

#ifdef USE_SDIO2_DRIVER
    sdio_ops = sdio2_driver_init();
#elif defined(USE_SDIO3_DRIVER)
    sdio_ops = sdio3_driver_init();
#else
    return -2;
#endif

    if (NULL == sdio_ops) {
        return -3;
    }

    g_transportsdio_perf.msg_ctrl = nh_msg_ctrl_init(sdio_ops);
    if (NULL == g_transportsdio_perf.msg_ctrl) {
        transportsdio_perf_sdio_hw_deinit();
        return -4;
    }

    ctx = (wifi_wifista_priv_t *)malloc(sizeof(*ctx));
    if (NULL == ctx) {
        nh_msg_ctrl_deinit(g_transportsdio_perf.msg_ctrl);
        g_transportsdio_perf.msg_ctrl = NULL;
        transportsdio_perf_sdio_hw_deinit();
        return -5;
    }
    memset(ctx, 0, sizeof(*ctx));

    ret = wifi_mgmr_sta_mac_get(ctx->mac_addr);
    if (0 != ret) {
        ctx->mac_addr[0] = 0x02;
        ctx->mac_addr[5] = 0x01;
        printf("sdioperf: failed to get STA MAC, fallback to 02:00:00:00:00:01\r\n");
    }

    ret = transportsdio_init(ctx, g_transportsdio_perf.msg_ctrl, nh_MSG_TAG_ETH);
    if (0 != ret) {
        free(ctx);
        nh_msg_ctrl_deinit(g_transportsdio_perf.msg_ctrl);
        g_transportsdio_perf.msg_ctrl = NULL;
        transportsdio_perf_sdio_hw_deinit();
        return -6;
    }

    transportsdio_set_wifi_send_cb(transportsdio_perf_wifi_send_cb);

    g_transportsdio_perf.initialized = true;
    g_transportsdio_perf.rx_running = false;
    g_transportsdio_perf.tx_running = false;
    g_transportsdio_perf.tx_stop_requested = false;
    g_transportsdio_perf.tx_task = NULL;
    g_transportsdio_perf.tx_payload_len = 0U;
    g_transportsdio_perf.tx_sequence = 0U;
    g_transportsdio_perf.tx_send_failures = 0U;
    g_transportsdio_perf.tx_last_result = 0;
    transportsdio_perf_stat_reset(&g_transportsdio_perf.rx_stat);
    transportsdio_perf_stat_reset(&g_transportsdio_perf.tx_stat);

    printf("SDIO transport perf initialized\r\n");
    return 0;
}

static int transportsdio_perf_rx_stop(void);
static int transportsdio_perf_tx_stop(void);

int transportsdio_perf_deinit(void)
{
    int ret;

    if (false == g_transportsdio_perf.initialized) {
        return -1;
    }

    if (g_transportsdio_perf.tx_running || (NULL != g_transportsdio_perf.tx_task)) {
        ret = transportsdio_perf_tx_stop();
        if (0 != ret) {
            return -2;
        }
    }

    if (g_transportsdio_perf.rx_running) {
        transportsdio_perf_rx_stop();
    }

    transportsdio_deinit();

    if (NULL != g_transportsdio_perf.msg_ctrl) {
        nh_msg_ctrl_deinit(g_transportsdio_perf.msg_ctrl);
        g_transportsdio_perf.msg_ctrl = NULL;
    }

    transportsdio_perf_sdio_hw_deinit();

    g_transportsdio_perf.initialized = false;
    g_transportsdio_perf.rx_running = false;
    g_transportsdio_perf.tx_running = false;
    g_transportsdio_perf.tx_stop_requested = false;
    g_transportsdio_perf.tx_task = NULL;
    g_transportsdio_perf.tx_payload_len = 0U;
    g_transportsdio_perf.tx_sequence = 0U;
    g_transportsdio_perf.tx_send_failures = 0U;
    g_transportsdio_perf.tx_last_result = 0;
    transportsdio_perf_stat_reset(&g_transportsdio_perf.rx_stat);
    transportsdio_perf_stat_reset(&g_transportsdio_perf.tx_stat);

    printf("SDIO transport perf deinitialized\r\n");
    return 0;
}

static int transportsdio_perf_rx_start(void)
{
    if (0 == transportsdio_perf_transport_ready()) {
        return -1;
    }

    if (g_transportsdio_perf.rx_running) {
        return -2;
    }

    transportsdio_perf_stat_reset(&g_transportsdio_perf.rx_stat);
    g_transportsdio_perf.rx_running = true;
    printf("SDIO RX test started\r\n");
    return 0;
}

static int transportsdio_perf_rx_stop(void)
{
    if (false == g_transportsdio_perf.rx_running) {
        return -1;
    }

    g_transportsdio_perf.rx_running = false;
    g_transportsdio_perf.rx_stat.last_ms = transportsdio_perf_now_ms();
    transportsdio_perf_print_summary("SDIO RX", &g_transportsdio_perf.rx_stat);
    return 0;
}

static void transportsdio_perf_tx_task(void *arg)
{
    uint32_t payload_len = (uint32_t)(uintptr_t)arg;
    uint8_t *payload = NULL;

    payload = (uint8_t *)malloc(payload_len);
    if (NULL == payload) {
        g_transportsdio_perf.tx_last_result = -3;
        printf("sdioperf: failed to allocate TX payload buffer\r\n");
        goto exit;
    }

    transportsdio_perf_stat_reset(&g_transportsdio_perf.tx_stat);
    g_transportsdio_perf.tx_send_failures = 0U;
    g_transportsdio_perf.tx_last_result = 0;
    printf("SDIO TX test started: payload=%lu bytes\r\n", (unsigned long)payload_len);

    while (false == g_transportsdio_perf.tx_stop_requested) {
        int ret;

        transportsdio_perf_fill_payload(payload, payload_len, g_transportsdio_perf.tx_sequence);
        ret = transportsdio_send_upld(payload, payload_len);
        if (0 == ret) {
            g_transportsdio_perf.tx_sequence++;
            transportsdio_perf_stat_update(&g_transportsdio_perf.tx_stat, "SDIO TX", payload_len);
            continue;
        }

        g_transportsdio_perf.tx_send_failures++;
        if (-2 == ret || -4 == ret) {
            g_transportsdio_perf.tx_last_result = -5;
            printf("SDIO TX test stopped, transport unavailable: %d\r\n", ret);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    free(payload);
    g_transportsdio_perf.tx_stat.last_ms = transportsdio_perf_now_ms();
    transportsdio_perf_print_summary("SDIO TX", &g_transportsdio_perf.tx_stat);
    if (0U != g_transportsdio_perf.tx_send_failures) {
        printf("SDIO TX send_fail=%lu\r\n", (unsigned long)g_transportsdio_perf.tx_send_failures);
    }

exit:
    g_transportsdio_perf.tx_running = false;
    g_transportsdio_perf.tx_stop_requested = false;
    g_transportsdio_perf.tx_task = NULL;
    vTaskDelete(NULL);
}

static int transportsdio_perf_tx_start(uint32_t payload_len)
{
    int ret;

    if (0U == payload_len) {
        payload_len = TRANSPORTSDIO_PERF_TX_DEFAULT_PAYLOAD;
    }

    if (0 == transportsdio_perf_transport_ready()) {
        return -1;
    }
    if (payload_len > WIFI_RX_BUFF_SIZE) {
        return -1;
    }
    if (g_transportsdio_perf.tx_running) {
        return -2;
    }
    if (NULL != g_transportsdio_perf.tx_task) {
        return -2;
    }

    g_transportsdio_perf.tx_payload_len = payload_len;
    g_transportsdio_perf.tx_sequence = 0U;
    g_transportsdio_perf.tx_stop_requested = false;
    g_transportsdio_perf.tx_running = true;
    g_transportsdio_perf.tx_last_result = 0;
    ret = xTaskCreate(transportsdio_perf_tx_task,
                      "sdio_perf_tx",
                      TRANSPORTSDIO_PERF_TX_TASK_STACK,
                      (void *)(uintptr_t)payload_len,
                      TRANSPORTSDIO_PERF_TX_TASK_PRIO,
                      &g_transportsdio_perf.tx_task);
    if (pdPASS == ret) {
        return 0;
    }

    g_transportsdio_perf.tx_running = false;
    g_transportsdio_perf.tx_task = NULL;
    g_transportsdio_perf.tx_payload_len = 0U;
    g_transportsdio_perf.tx_last_result = -3;
    return -3;
}

static int transportsdio_perf_tx_stop(void)
{
    uint32_t waited_ms = 0U;

    if (!g_transportsdio_perf.tx_running && (NULL == g_transportsdio_perf.tx_task)) {
        return -1;
    }

    g_transportsdio_perf.tx_stop_requested = true;
    while (1) {
        if (NULL == g_transportsdio_perf.tx_task) {
            break;
        }
        if (waited_ms >= TRANSPORTSDIO_PERF_TX_STOP_WAIT_MS) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        waited_ms += 10U;
    }

    if (NULL == g_transportsdio_perf.tx_task) {
        return 0;
    }

    return -2;
}

int transportsdio_perf_test_rx(uint32_t duration_ms)
{
    int ret;

    if (0U == duration_ms) {
        duration_ms = TRANSPORTSDIO_PERF_DEFAULT_DURATION_MS;
    }

    ret = transportsdio_perf_rx_start();
    if (0 != ret) {
        return ret;
    }

    printf("SDIO RX duration=%lu ms\r\n", (unsigned long)duration_ms);
    //transportsdio_perf_wait_ms(duration_ms);

    return 0;
}

int transportsdio_perf_test_tx(uint32_t payload_len, uint32_t duration_ms)
{
    int ret;
    uint32_t start_ms;

    if (0U == duration_ms) {
        duration_ms = TRANSPORTSDIO_PERF_DEFAULT_DURATION_MS;
    }

    ret = transportsdio_perf_tx_start(payload_len);
    if (0 != ret) {
        return ret;
    }

    printf("SDIO TX duration=%lu ms\r\n", (unsigned long)duration_ms);

    return g_transportsdio_perf.tx_last_result;
}

void transportsdio_perf_dump_status(void)
{
    printf("SDIO transport: %s\r\n", g_transportsdio_perf.initialized ? "initialized" : "not initialized");
    printf("SDIO RX test: %s\r\n", g_transportsdio_perf.rx_running ? "running" : "stopped");
    printf("SDIO TX test: %s", g_transportsdio_perf.tx_running ? "running" : "stopped");
    if (g_transportsdio_perf.tx_running) {
        printf(", payload=%lu bytes", (unsigned long)g_transportsdio_perf.tx_payload_len);
    } else if (0U != g_transportsdio_perf.tx_payload_len) {
        printf(", last payload=%lu bytes", (unsigned long)g_transportsdio_perf.tx_payload_len);
    }
    printf("\r\n");
}

static void transportsdio_perf_help(void)
{
    printf("Usage:\r\n");
    printf("  sdioperf init\r\n");
    printf("  sdioperf deinit\r\n");
    printf("  sdioperf status\r\n");
    printf("  sdioperf rx [duration_s]\r\n");
    printf("  sdioperf tx [duration_s] [payload_len]\r\n");
    printf("Notes:\r\n");
    printf("  default duration: %lu s\r\n", (unsigned long)(TRANSPORTSDIO_PERF_DEFAULT_DURATION_MS / 1000U));
    printf("  default TX payload: %lu bytes\r\n", (unsigned long)TRANSPORTSDIO_PERF_TX_DEFAULT_PAYLOAD);
    printf("  sdioperf init requires transportsdio not already initialized by normal nethub flow\r\n");
    printf("Examples:\r\n");
    printf("  sdioperf init\r\n");
    printf("  sdioperf rx 10\r\n");
    printf("  sdioperf tx 10\r\n");
    printf("  sdioperf tx 10 512\r\n");
    printf("  sdioperf deinit\r\n");
}

static int transportsdio_perf_parse_u32(const char *arg, uint32_t *value)
{
    char *endptr = NULL;
    unsigned long parsed;

    if (NULL == arg) {
        return -1;
    }
    if (NULL == value) {
        return -1;
    }
    if ('\0' == arg[0]) {
        return -1;
    }

    parsed = strtoul(arg, &endptr, 0);
    if (parsed > UINT32_MAX) {
        return -1;
    }
    if ('\0' != endptr[0]) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int transportsdio_perf_parse_duration_ms(const char *arg, uint32_t *duration_ms)
{
    uint32_t duration_s;

    if (NULL == duration_ms) {
        return -1;
    }
    if (0 != transportsdio_perf_parse_u32(arg, &duration_s)) {
        return -1;
    }
    if (duration_s > (UINT32_MAX / 1000U)) {
        return -1;
    }

    *duration_ms = duration_s * 1000U;
    return 0;
}

int cmd_rcp_test(int argc, char **argv)
{
    int ret;

    if (argc < 2) {
        transportsdio_perf_help();
        return -1;
    }

    if (0 == strcmp(argv[1], "help")) {
        transportsdio_perf_help();
        return 0;
    }

    if (0 == strcmp(argv[1], "init")) {
        ret = transportsdio_perf_init();
        if (-1 == ret) {
            printf("sdioperf: transport already initialized\r\n");
        } else if (-2 == ret) {
            printf("sdioperf: no SDIO driver selected in build\r\n");
        } else if (-7 == ret) {
            printf("sdioperf: transportsdio is already initialized by another module\r\n");
        } else if (ret < 0) {
            printf("sdioperf: failed to initialize standalone SDIO transport, ret=%d\r\n", ret);
        }
        return ret;
    }

    if (0 == strcmp(argv[1], "deinit")) {
        ret = transportsdio_perf_deinit();
        if (-1 == ret) {
            printf("sdioperf: transport is not initialized\r\n");
        } else if (-2 == ret) {
            printf("sdioperf: failed to stop TX task before deinit\r\n");
        }
        return ret;
    }

    if (0 == strcmp(argv[1], "status")) {
        transportsdio_perf_dump_status();
        return 0;
    }

    if (0 == strcmp(argv[1], "rx")) {
        uint32_t duration_ms = -1;

        if (argc > 3) {
            transportsdio_perf_help();
            return -1;
        }
        if (argc >= 3) {
            if (0 != transportsdio_perf_parse_duration_ms(argv[2], &duration_ms)) {
                printf("Invalid duration: %s\r\n", argv[2]);
                return -1;
            }
        }

        ret = transportsdio_perf_test_rx(duration_ms);
        if (-1 == ret) {
            printf("sdioperf: standalone SDIO transport not initialized\r\n");
        } else if (-2 == ret) {
            printf("sdioperf: RX test is already running\r\n");
        }
        return ret;
    }

    if (0 == strcmp(argv[1], "tx")) {
        uint32_t duration_ms = 0U;
        uint32_t payload_len = TRANSPORTSDIO_PERF_TX_DEFAULT_PAYLOAD;

        if (argc >= 3) {
            if (0 != transportsdio_perf_parse_duration_ms(argv[2], &duration_ms)) {
                printf("Invalid duration: %s\r\n", argv[2]);
                return -1;
            }
        }
        if (argc >= 4) {
            if (0 != transportsdio_perf_parse_u32(argv[3], &payload_len)) {
                printf("Invalid payload length: %s\r\n", argv[3]);
                return -1;
            }
        }
        if (argc > 4) {
            transportsdio_perf_help();
            return -1;
        }

        ret = transportsdio_perf_test_tx(payload_len, duration_ms);
        if (-1 == ret) {
            printf("sdioperf: invalid payload length or standalone SDIO transport not initialized\r\n");
        } else if (-2 == ret) {
            printf("sdioperf: TX test is already running\r\n");
        } else if (-3 == ret) {
            printf("sdioperf: failed to create or allocate TX test resources\r\n");
        } else if (-5 == ret) {
            printf("sdioperf: TX transport became unavailable during test\r\n");
        }
        return ret;
    }

    transportsdio_perf_help();
    return -1;
}

int cmd_eth_dump(int argc, char **argv)
{
    if (argc > 2) {
        printf("Usage: eth_dump [clear]\r\n");
        return -1;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "clear") != 0) {
            printf("Usage: eth_dump [clear]\r\n");
            return -1;
        }

        transportsdio_reset_stats();
        printf("transport dump counters cleared\r\n");
    }

    transportsdio_dump_priv();
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(cmd_rcp_test, rcp_test, standalone SDIO ETH throughput test);
SHELL_CMD_EXPORT_ALIAS(cmd_eth_dump, eth_dump, standalone SDIO ETH dump);
