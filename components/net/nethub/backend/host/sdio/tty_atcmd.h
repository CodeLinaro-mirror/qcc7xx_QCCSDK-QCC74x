#ifndef TTY_ATCMD_H
#define TTY_ATCMD_H

#include <stdint.h>

#include "mr_msg_ctrl.h"
#include "mr_tty.h"
#include "nethub_defs.h"

int tty_atcmd_init(mr_msg_ctrl_priv_t *msg_ctrl);
int tty_atcmd_upld_send(uint8_t *data_buff, uint32_t data_size);
int tty_atcmd_dnld_register(nethub_ctrl_rx_cb_t dnld_cb, void *cbpri_arg);
mr_tty_priv_t *tty_atcmd_priv_get(void);

#endif
