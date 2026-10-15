#include "qcc74x_mtimer.h"
#include "qcc74x_wdg.h"
#include "board.h"

struct qcc74x_device_s *wdg;
static volatile uint8_t wdg_int_arrived = 0;

void wdg_isr(int irq, void *arg)
{
    qcc74x_wdg_compint_clear(wdg);
    wdg_int_arrived = 1;
}

int main(void)
{
    board_init();
    printf("Watchdog interrupt test\r\n");

    struct qcc74x_wdg_config_s wdg_cfg;
    wdg_cfg.clock_source = WDG_CLKSRC_32K;
    wdg_cfg.clock_div = 31;
    wdg_cfg.comp_val = 1000 * 5 - 1; /* 5s */
    wdg_cfg.mode = WDG_MODE_INTERRUPT;

    wdg = qcc74x_device_get_by_name("watchdog");
    qcc74x_wdg_init(wdg, &wdg_cfg);
    qcc74x_irq_attach(wdg->irq_num, wdg_isr, wdg);
    qcc74x_irq_enable(wdg->irq_num);

    wdg_int_arrived = 0;
    qcc74x_wdg_start(wdg);

    /* delay 3s and reset counter value. */
    qcc74x_mtimer_delay_ms(3000);
    qcc74x_wdg_reset_countervalue(wdg);
    /* delay 3s and wdg interrupt should not trigger. */
    qcc74x_mtimer_delay_ms(3000);
    if (wdg_int_arrived) {
        printf("Error! Delay 3s, wdg interrupt should not arrive\r\n");
        qcc74x_wdg_stop(wdg);
    } else {
        printf("Delay 3s, wdg interrupt not arrive, pass\r\n");
    }

    /* delay 3s will trigger wdg interrupt */
    qcc74x_mtimer_delay_ms(3000);
    qcc74x_wdg_reset_countervalue(wdg);
    if (wdg_int_arrived) {
        printf("Delay 3s, wdg interrupt arrived, pass\r\n");
    } else {
        printf("Error! Delay 3s, wdg interrupt not arrived, count = %d\r\n",
        qcc74x_wdg_get_countervalue(wdg));
    }
    qcc74x_wdg_stop(wdg);

    while (1) {
        qcc74x_mtimer_delay_ms(1500);
    }
}
