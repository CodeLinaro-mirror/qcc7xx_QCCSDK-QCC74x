
#include <stdio.h>

#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"

#include "rfparam_adapter.h"

#include <nethub.h>
#include <nhsdiowifi.h>

#include "qcc74x_irq.h"
#include "qcc74x_uart.h"
#include "qcc743_glb.h"
#define DBG_TAG "appuser"
#include "log.h"
#if CONFIG_NXSDIO_HCI
#include "btble_lib_api.h"
#include "btble_sdio_uart.h"
#endif
#define WIFI_STACK_SIZE  (1536)
#define TASK_PRIORITY_FW (16)

extern void app_atmodule_init(void);

int app_user_init(void)
{
    /* RF param init */
    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }

    /* TCP/IP stack init */
    tcpip_init(NULL, NULL);

    /* enable wifi clock */
    if (GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM) != 0) {
        LOG_I("Failed to un-gate WiFi clocks!\r\n");
        return -1;
    }
    if (GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI) != 0) {
        LOG_I("Failed to reset WiFi MCU!\r\n");
        return -1;
    }

    /* Enable wifi irq */
    extern void interrupt0_handler(void);
    if (qcc74x_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL) != 0) {
        LOG_I("Failed to attach WiFi IRQ!\r\n");
        return -1;
    }
    qcc74x_irq_enable(WIFI_IRQn);

    /* Enable easyflash(littlefs) */
    if (qcc74x_mtd_init() != 0) {
        LOG_I("Failed to init MTD!\r\n");
        return -1;
    }
    if (easyflash_init() != 0) {
        LOG_I("Failed to init easyflash!\r\n");
        return -1;
    }
    /* nethub init */
    nethub_sdiowifi_init();

    #if CONFIG_NXSDIO_HCI
    btble_sdio_uart_init();
    btble_controller_init(configMAX_PRIORITIES - 1);
    nethub_sdiohci_init();
    #endif

    /* wifi start */
    app_atmodule_init();

    return 0;
}
