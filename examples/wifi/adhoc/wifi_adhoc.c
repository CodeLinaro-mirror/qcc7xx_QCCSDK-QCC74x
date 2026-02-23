#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "shell.h"
#include "utils_getopt.h"
#include "qcc74x_mtimer.h"
#include "wifi_mgmr_ext.h"
#include "export/mac/mac_frame.h"
#include "export/mac/mac_types.h"
#include "export/common/co_list.h"
#include "export/export_macsw.h"

#define __MAYBE_UNUSED __attribute__((unused))
void adhoc_rx_cb(struct qcc74x_frame_info *info, void *arg)
{
    if (info->payload == NULL)
    {
        printf("Unsupported frame: length = %dr\r\n", info->length);
    }
    else
    {
        struct mac_hdr *hdr __MAYBE_UNUSED = (struct mac_hdr *)info->payload;
        printf("\r\nSN:%d length = %d rssi= %d freq = %u tods: %u fromds:%u \r\n", hdr->seq >> 4, info->length, info->rssi, info->freq, info->tods, info->fromds);
        if (info->eth_frame) {
            printf("eth_frame: ");
            for (int i = 0; i < info->eth_frame_length; i++) {
                printf("0x%x ", info->eth_frame[i]);
            }
            printf("\r\n");
        } else {
            printf("802.11_frame: ");
            for (int i = 0; i < info->length; i++) {
                printf("0x%x ", info->payload[i]);
            }
            printf("\r\n");
        }
    }
    return;
}

void adhoc_tx_cfm(void *env, uint32_t status)
{
    struct mm_raw_send_start_req *req = (struct mm_raw_send_start_req *)env;
    printf("tx status:%08lX, pkt addr: 0x%x acked: %s\r\n", status, req->pkt, (status & FRAME_SUCCESSFUL_TX_BIT) ? "Yes": "No");
    return;
}

#ifdef CONFIG_SHELL
#include <shell.h>
void cmd_adhoc_start(int argc, char **argv)
{
    wifi_mgmr_adhoc_start_params_t config;
    getopt_env_t getopt_env;
    int opt;

    memset(&config, '\0', sizeof(config));
    utils_getopt_init(&getopt_env, 0);
    while ((opt = utils_getopt(&getopt_env, argc, argv, "c:")) != -1) {
        switch (opt) {
        case 'c':
            config.channel = atoi(getopt_env.optarg);
            printf("set chan: %d\r\n", config.channel);
            break;

        case '?':
            printf("unknow option: %c\r\n", getopt_env.optopt);
            goto _ERROUT;
        }
    }
    config.cb = adhoc_rx_cb;
    config.cb_tx_cfm = adhoc_tx_cfm;
    wifi_mgmr_adhoc_start(&config);
    return;
_ERROUT:
    printf("[USAGE]: %s -c <channel num>\r\n", argv[0]);
}

void cmd_adhoc_stop(int argc, char **argv)
{
    wifi_mgmr_adhoc_stop();
}

uint8_t packet_adhoc_test[] = {
0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
0x08, 0x06, // arp probe
0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
0xa8, 0x17, 0x10, 0x3d, 0x3f, 0x25,
0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
struct mac_addr ra = {{0x3333,0x3333,0x3333}};

void cmd_wifi_adhoc_send(int argc, char **argv)
{

    wifi_mgmr_adhoc_pkt_params_t config;

    memset(&config, 0, sizeof(config));
    config.eth_frame = packet_adhoc_test;
    config.len = sizeof(packet_adhoc_test);
    config.ra = &ra;

    for (int i = 0; i < 10; i++) {
        if (wifi_mgmr_adhoc_pkt_send(&config)) {
            printf("Adhoc send failed\r\n");
        } else {
            printf("Adhoc send succeed\r\n");
        }
        //delay 1ms
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return;
}

SHELL_CMD_EXPORT_ALIAS(cmd_wifi_adhoc_send, wifi_adhoc_send, wifi adhoc send test);
SHELL_CMD_EXPORT_ALIAS(cmd_adhoc_start, wifi_adhoc_start, wifi ad-hoc start);
SHELL_CMD_EXPORT_ALIAS(cmd_adhoc_stop, wifi_adhoc_stop, wifi ad-hoc stop);
#endif
