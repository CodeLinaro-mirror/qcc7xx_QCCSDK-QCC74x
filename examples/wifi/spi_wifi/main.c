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
#include "mem.h"

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "export/qcc74x_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"

#include "qcc74x_irq.h"
#include "qcc74x_uart.h"
#include "qcc74x_common.h"

#include "qcc743_glb.h"
#include "rfparam_adapter.h"

#include "app_spiwifi.h"
#include "app_pm.h"

#if CONFIG_THREAD
#include "lmac154.h"
#include "openthread_port.h"
#include "openthread/ncp.h"
#endif

#include "board.h"
#include "shell.h"

#include "assert.h"

#include "qcc74x_mtd.h"
#include "coredump.h"

#define INIT_STACK_SIZE    (2048)
#define TASK_PRIORITY_INIT (16)

#ifdef CONFIG_SHELL
static struct qcc74x_device_s *uart0;
extern void shell_init_with_task(struct qcc74x_device_s *shell);
#endif
#if CONFIG_THREAD
extern void __libc_init_array(void);
#endif

/* Add forward declaration if needed */
void board_spi0_gpio_3pin_init(void);

void app_init_entry(void *param)
{
    app_spiwifi_init();

#if CONFIG_THREAD
    otRadio_opt_t opt;

    opt.byte = 0;
    opt.bf.isCoexEnable = true;
    opt.bf.isLinkMetricEnable = true;
    opt.bf.isCSLReceiverEnable = true;
    opt.bf.isTimeSyncEnable = true;

    otrStart(opt);
#endif

    vTaskDelete(NULL);
}

#if CONFIG_THREAD
void vApplicationTickHook( void )
{
    lmac154_monitor();
}

void otrInitUser(otInstance * instance)
{
    otAppNcpInit((otInstance * )instance);
}
#endif

int main(void)
{
    int ret;

    board_init();

    core_partition_init();
void at_minidump_init(uint32_t, size_t);
    at_minidump_init(core_partition_addr(),core_partition_size());

#ifdef CONFIG_SHELL
    uart0 = qcc74x_device_get_by_name("uart0");
    if (!uart0) {
        printf("[main] Error: uart0 device not found\r\n");
        return -1;
    }
    shell_init_with_task(uart0);
#endif

    // UART1 GPIO conflicts with SPI pins, only one can be used
    //board_uartx_gpio_init();
#if CONFIG_SPI_3PIN_MODE_ENABLE
    board_spi0_gpio_3pin_init(); /* Now declared above */
#else
    board_spi0_gpio_init();
#endif 
#if CONFIG_THREAD
    __libc_init_array();
#endif

    if (xTaskCreate(app_init_entry, (char *)"init", INIT_STACK_SIZE, NULL, TASK_PRIORITY_INIT, NULL) != pdPASS) {
        printf("[main] Error: xTaskCreate failed\r\n");
        return -1;
    }

    vTaskStartScheduler();

    while (1) {
    }
    return 0;
}

int shell_crash(int argc, char **argv)
{
    printf("shell crash\r\n");
    //asm ("ebreak");
    assert(0);
    return 0;
}
SHELL_CMD_EXPORT_ALIAS(shell_crash, crash, trigger crash.);
