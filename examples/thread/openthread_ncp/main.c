
#include <qcc74x_sys.h>
#include <qcc74x_mtd.h>

#if defined(QCC743)
#include "rfparam_adapter.h"
#endif

#if defined (CONFIG_EASYFLASH4)
#include <easyflash.h>
#endif

#include <lmac154.h>
#include <openthread_port.h>
#include <openthread/ncp.h>

#include "board.h"

static struct qcc74x_device_s *uart0;
extern void __libc_init_array(void);
extern void board_ncp_init(void);

void otrInitUser(otInstance * instance)
{
    otAppNcpInit((otInstance * )instance);
}

void vApplicationTickHook( void )
{
#ifdef QCC743
    lmac154_monitor();
#endif
}

int main(void)
{
    otRadio_opt_t opt;
    
#if !defined(QCC74x_undefL)
    qcc74x_sys_rstinfo_init();
#endif

    board_ncp_init();

    qcc74x_mtd_init();
#if defined (CONFIG_EASYFLASH4)
    easyflash_init();
#endif

    configASSERT((configMAX_PRIORITIES > 4));

#if defined(QCC743)
    /* Init rf */
    if (0 != rfparam_init(0, NULL, 0)) {
        printf("PHY RF init failed!\r\n");
        return 0;
    }
#endif
    
    __libc_init_array();

    uart0 = qcc74x_device_get_by_name("uart0");
    ot_uart_init(uart0);

    opt.byte = 0;

    opt.bf.isCoexEnable = false;
#if OPENTHREAD_FTD
    opt.bf.isFtd = true;
#endif

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    opt.bf.isLinkMetricEnable = true;
#endif
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    opt.bf.isCSLReceiverEnable = true;
#endif
#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
    opt.bf.isTimeSyncEnable = true;
#endif

    otrStart(opt);

    puts("[OS] Starting OS Scheduler...\r\n");
    vTaskStartScheduler();

    while (1) {
    }
}
