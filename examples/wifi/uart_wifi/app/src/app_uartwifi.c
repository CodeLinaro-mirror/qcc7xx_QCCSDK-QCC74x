
#include <stdio.h>

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "export/qcc74x_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"

#include "qcc74x_irq.h"
#include "qcc74x_uart.h"
#include "qcc743_glb.h"

#include "rfparam_adapter.h"

#define DBG_TAG "appuartwifi"
#include "log.h"

#define WIFI_STACK_SIZE  (1536)
#define TASK_PRIORITY_FW (16)

extern void app_atmoudle_init(void);

int app_uartwifi_init(void)
{
    /* RF param init */
    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }

    /* TCP/IP stack init */
    tcpip_init(NULL, NULL);

    /* enable wifi clock */
    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);

    /* Enable wifi irq */
    extern void interrupt0_handler(void);
    qcc74x_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    qcc74x_irq_enable(WIFI_IRQn);

    /* Enable easyflash(littlefs) */
    qcc74x_mtd_init();
    easyflash_init();

    /* AT moudle start */
    app_atmoudle_init();
  
    return 0;
}

