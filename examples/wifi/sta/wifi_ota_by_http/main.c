/****************************************************************************
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "export/qcc74x_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"

#include "qcc74x_irq.h"
#include "qcc74x_uart.h"
#include "qcc74x_flash.h"

#include "qcc743_glb.h"
#include "rfparam_adapter.h"

#include "board.h"
#include "shell.h"
#include "partition.h"

#include "qcc74x_wdg.h"
#include "ring_buffer.h"

#define DBG_TAG "MAIN"
#include "log.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define WIFI_STACK_SIZE  (1536)
#define TASK_PRIORITY_FW (16)

#define AUTO_CONNECT_WIFI (0)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct qcc74x_device_s *gpio;
static struct qcc74x_device_s *uart0;

static TaskHandle_t wifi_fw_task;
#if AUTO_CONNECT_WIFI
struct qcc74x_device_s *wdg;
static TaskHandle_t ota_task_handle;
#endif

#if AUTO_CONNECT_WIFI
// clang-format off
/* config your wifi ssid and password */
static const uint8_t wifi_sta_connet[] = "wifi_sta_connect 21a 12344321\r";
/* config your ota server hostname and IP address */
static const uint8_t ota_hostadd[] = "dns_local_hostadd localhost_1 192.168.31.112\r";
/* config your ota server and port */
static const uint8_t wifi_ota_test[] = "wifi_ota_test https://localhost_1:5000/build/build_out/wifi_ota_qcc743.bin.ota\r";
/* reboot command */
// static const uint8_t send_buf1[] = "reboot\r";
// clang-format on

extern Ring_Buffer_Type shell_rb;
extern void shell_release_sem(void);

SemaphoreHandle_t sem_wifi_init_done;
SemaphoreHandle_t sem_wifi_connect_done;
SemaphoreHandle_t sem_wifi_disconnect;
#endif

extern void shell_init_with_task(struct qcc74x_device_s *shell);

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Functions
 ****************************************************************************/

int wifi_start_firmware_task(void)
{
    LOG_I("Starting wifi ...\r\n");

    /* enable wifi clock */

    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);

    /* Enable wifi irq */

    extern void interrupt0_handler(void);
    qcc74x_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    qcc74x_irq_enable(WIFI_IRQn);

    xTaskCreate(wifi_main, (char *)"fw", WIFI_STACK_SIZE, NULL, TASK_PRIORITY_FW, &wifi_fw_task);

    return 0;
}

void wifi_event_handler(uint32_t code)
{
    uint32_t ret = 0;
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    switch (code) {
        case CODE_WIFI_ON_INIT_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_INIT_DONE\r\n", __func__);
            wifi_mgmr_init();
        } break;
        case CODE_WIFI_ON_MGMR_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_MGMR_DONE\r\n", __func__);
#if AUTO_CONNECT_WIFI
            ret = xSemaphoreGiveFromISR(sem_wifi_init_done, &xHigherPriorityTaskWoken);
            if (ret == pdPASS) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
#endif
        } break;
        case CODE_WIFI_ON_SCAN_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_SCAN_DONE\r\n", __func__);
            wifi_mgmr_sta_scanlist();
        } break;
        case CODE_WIFI_ON_CONNECTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_CONNECTED\r\n", __func__);
            void mm_sec_keydump();
            mm_sec_keydump();
        } break;
        case CODE_WIFI_ON_GOT_IP: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP\r\n", __func__);
#if AUTO_CONNECT_WIFI
            ret = xSemaphoreGiveFromISR(sem_wifi_connect_done, &xHigherPriorityTaskWoken);
            if (ret == pdPASS) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
#endif
        } break;
        case CODE_WIFI_ON_DISCONNECT: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_DISCONNECT\r\n", __func__);
#if AUTO_CONNECT_WIFI
            ret = xSemaphoreGiveFromISR(sem_wifi_disconnect, &xHigherPriorityTaskWoken);
            if (ret == pdPASS) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
#endif
        } break;
        case CODE_WIFI_ON_AP_STARTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STARTED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STOPPED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STOPPED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STA_ADD: {
            LOG_I("[APP] [EVT] [AP] [ADD] %lld\r\n", xTaskGetTickCount());
        } break;
        case CODE_WIFI_ON_AP_STA_DEL: {
            LOG_I("[APP] [EVT] [AP] [DEL] %lld\r\n", xTaskGetTickCount());
        } break;
        default: {
            LOG_I("[APP] [EVT] Unknown code %u \r\n", code);
        }
    }
}

#if AUTO_CONNECT_WIFI
void ota_task(void *param)
{
    if (xSemaphoreTake(sem_wifi_init_done, portMAX_DELAY) == pdTRUE) {
        Ring_Buffer_Write(&shell_rb, wifi_sta_connet, sizeof(wifi_sta_connet) - 1);
        shell_release_sem();
    }

    if (xSemaphoreTake(sem_wifi_connect_done, portMAX_DELAY) == pdTRUE) {
        Ring_Buffer_Write(&shell_rb, ota_hostadd, sizeof(ota_hostadd) - 1);
        Ring_Buffer_Write(&shell_rb, wifi_ota_test, sizeof(wifi_ota_test) - 1);
        shell_release_sem();
    }

    while (1) {
        if (xSemaphoreTake(sem_wifi_disconnect, portMAX_DELAY) == pdTRUE) {
            Ring_Buffer_Write(&shell_rb, wifi_sta_connet, sizeof(wifi_sta_connet) - 1);
            shell_release_sem();
        }
        vTaskDelay(10000);
    }
}
#endif

static void wifi_ota_dump_partition()
{
    pt_table_stuff_config pt_table_stuff[2];
    pt_table_id_type active_id;
    pt_table_stuff_config *pt_stuff;

    /* Set flash operation function, read via xip */
    pt_table_set_flash_operation(qcc74x_flash_erase, qcc74x_flash_write, qcc74x_flash_read);

    active_id = pt_table_get_active_partition_need_lock(pt_table_stuff);
    if (PT_TABLE_ID_INVALID == active_id) {
        printf("No valid PT\r\n");
        return;
    }
    printf("Active PT:%d,Age %d\r\n", active_id, pt_table_stuff[active_id].pt_table.age);

    pt_stuff = &pt_table_stuff[active_id];

    printf("======= PtTable_Config @%p=======\r\n", pt_stuff);
    printf("magicCode 0x%08X;", (unsigned int)(pt_stuff->pt_table.magicCode));
    printf(" version 0x%04X;", pt_stuff->pt_table.version);
    printf(" entryCnt %u;", pt_stuff->pt_table.entryCnt);
    printf(" age %lu;", pt_stuff->pt_table.age);
    printf(" crc32 0x%08X\r\n", (unsigned int)pt_stuff->pt_table.crc32);
    printf(" idx  type device active_index    name    address[0]   address[1]   length[0]    length[1]   age \r\n");
    for (uint32_t i = 0; i < pt_stuff->pt_table.entryCnt; i++) {
        printf("[%02d] ", i);
        printf("  %02u", (pt_stuff->pt_entries[i].type));
        printf("     %u", (pt_stuff->pt_entries[i].device));
        printf("        %u", (pt_stuff->pt_entries[i].active_index));
        printf("       %8s", (pt_stuff->pt_entries[i].name));
        printf("   0x%08lx", (pt_stuff->pt_entries[i].start_address[0]));
        printf("   0x%08lx", (pt_stuff->pt_entries[i].start_address[1]));
        printf("   0x%08lx", (pt_stuff->pt_entries[i].max_len[0]));
        printf("   0x%08lx", (pt_stuff->pt_entries[i].max_len[1]));
        printf("   %lu\r\n", (pt_stuff->pt_entries[i].age));
    }
}

int main(void)
{
    board_init();

#if AUTO_CONNECT_WIFI
    sem_wifi_init_done = xSemaphoreCreateBinary();
    sem_wifi_connect_done = xSemaphoreCreateBinary();
    sem_wifi_disconnect = xSemaphoreCreateBinary();
#endif

    uart0 = qcc74x_device_get_by_name("uart0");
    shell_init_with_task(uart0);

    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }

    LOG_I("PHY RF init success!\r\n");

    wifi_ota_dump_partition();

    tcpip_init(NULL, NULL);
    wifi_start_firmware_task();

#if AUTO_CONNECT_WIFI
    printf("Config Watchdog...\r\n");
    struct qcc74x_wdg_config_s wdg_cfg;
    wdg_cfg.clock_source = WDG_CLKSRC_32K;
    wdg_cfg.clock_div = 31;
    wdg_cfg.comp_val = 30000;
    wdg_cfg.mode = WDG_MODE_RESET;

    wdg = qcc74x_device_get_by_name("watchdog");
    qcc74x_wdg_init(wdg, &wdg_cfg);

    qcc74x_wdg_start(wdg);
    qcc74x_wdg_reset_countervalue(wdg);
    printf("Next delay 30s, wdg will reset it.\r\n");

    xTaskCreate(ota_task, "ota_task", 2048, NULL, 15, &ota_task_handle);
#endif

    vTaskStartScheduler();

    while (1) {
    }
}
