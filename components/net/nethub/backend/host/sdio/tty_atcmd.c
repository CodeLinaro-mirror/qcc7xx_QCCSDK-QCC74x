#include <stdint.h>

#include "board.h"
#include "FreeRTOS.h"
#include "task.h"

#include "mr_frame_buff_ctrl.h"
#include "mr_msg_ctrl.h"
#include "mr_tty.h"
#include "nethub_defs.h"
#include "tty_atcmd.h"

#define DBG_TAG "TTY_ATCMD"
#include "log.h"

#define FRAME_BUFFER_ATTR __ALIGNED(64) ATTR_NOCACHE_NOINIT_RAM_SECTION

#define TTY_ATCMD_FRAME_CNT      (4)
#define TTY_ATCMD_PAYLOAD_SIZE   (1024)
#define TTY_ATCMD_FRAME_SIZE     (FRAME_BUFF_HEADER_ROOM + sizeof(mr_tty_msg_t) + TTY_ATCMD_PAYLOAD_SIZE)
#define TTY_ATCMD_DNLD_CREDIT    (TTY_ATCMD_FRAME_CNT)
#define TTY_ATCMD_CREDIT_THRESH  (TTY_ATCMD_FRAME_CNT / 2)

static FRAME_BUFFER_ATTR uint8_t g_tty_atcmd_frame_buff[TTY_ATCMD_FRAME_CNT][TTY_ATCMD_FRAME_SIZE];
static mr_tty_priv_t *g_tty_atcmd_priv;
static nethub_ctrl_rx_cb_t g_tty_atcmd_dnld_cb;
static void *g_tty_atcmd_dnld_arg;

static int tty_atcmd_dnld_output(mr_tty_priv_t *priv, mr_tty_msg_t *tty_msg_pkt)
{
    uint16_t data_len;

    data_len = MR_TTY_MSG_PACKET_GET_DATA_SIZE(tty_msg_pkt);

    if (g_tty_atcmd_dnld_cb != NULL && data_len > 0) {
        g_tty_atcmd_dnld_cb(g_tty_atcmd_dnld_arg, tty_msg_pkt->data, data_len);
    }

    mr_tty_dnld_elem_free(priv, tty_msg_pkt);
    return 0;
}

int tty_atcmd_upld_send(uint8_t *data_buff, uint32_t data_size)
{
    uint32_t offset = 0;

    if (data_size == 0) {
        return NETHUB_OK;
    }

    if (g_tty_atcmd_priv == NULL || data_buff == NULL) {
        return NETHUB_ERR_INVALID_PARAM;
    }

    while (offset < data_size) {
        uint32_t remain = data_size - offset;
        uint16_t chunk_len = (remain > TTY_ATCMD_PAYLOAD_SIZE) ?
            TTY_ATCMD_PAYLOAD_SIZE : (uint16_t)remain;
        int ret = mr_tty_upld_data_send(g_tty_atcmd_priv, data_buff + offset, chunk_len);

        if (ret < 0) {
            return ret;
        }

        offset += chunk_len;
    }

    return NETHUB_OK;
}

int tty_atcmd_dnld_register(nethub_ctrl_rx_cb_t dnld_cb, void *cbpri_arg)
{
    g_tty_atcmd_dnld_cb = dnld_cb;
    g_tty_atcmd_dnld_arg = cbpri_arg;
    return NETHUB_OK;
}

int nh_tty_upld_send(uint8_t *data_buff, uint32_t data_size)
{
    return tty_atcmd_upld_send(data_buff, data_size);
}

int nh_tty_dnld_register(nethub_ctrl_rx_cb_t dnld_cb, void *cbpri_arg)
{
    return tty_atcmd_dnld_register(dnld_cb, cbpri_arg);
}

mr_tty_priv_t *tty_atcmd_priv_get(void)
{
    return g_tty_atcmd_priv;
}

int tty_atcmd_init(mr_msg_ctrl_priv_t *msg_ctrl)
{
    mr_tty_cfg_t tty_cfg = {
        .name = "tty_atcmd",
        .msg_ctrl = msg_ctrl,
        .msg_tag = MR_MSG_TAG_TTY_CMD,
        .upld_frame_size = TTY_ATCMD_FRAME_SIZE,
        .upld_frame_count = TTY_ATCMD_FRAME_CNT,
        .upld_frame_buff = g_tty_atcmd_frame_buff,
        .dnld_credit_max = TTY_ATCMD_DNLD_CREDIT,
        .upld_credit_update_threshold = TTY_ATCMD_CREDIT_THRESH,
        .dnld_output_cb = tty_atcmd_dnld_output,
        .upld_done_cb = NULL,
        .task_stack_size = 512,
        .task_priority = 15,
        .task_period_max_ms = portMAX_DELAY,
    };

    if (g_tty_atcmd_priv != NULL) {
        return NETHUB_OK;
    }

    g_tty_atcmd_priv = mr_tty_init(&tty_cfg);
    if (g_tty_atcmd_priv == NULL) {
        LOG_E("Failed to init tty_atcmd\r\n");
        return NETHUB_ERR_INTERNAL;
    }

    vTaskDelay(10);
    return NETHUB_OK;
}
