#include "usbd_core.h"
#include "qcc74x_mtimer.h"
#include "board.h"

extern void cdc_acm_init(void);
extern void cdc_acm_data_send_poll(void);

int main(void)
{
    board_init();

    cdc_acm_init();
    while (1) {
        cdc_acm_data_send_poll();
        qcc74x_mtimer_delay_us(2);
    }
}
