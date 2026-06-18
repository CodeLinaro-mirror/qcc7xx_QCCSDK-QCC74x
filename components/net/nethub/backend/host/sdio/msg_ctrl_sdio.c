/**
 * @file mr_msg_ctrl_sdio.c
 * @brief SDIO-specific message controller initialization and configuration
 */

#include "qcc74x_core.h"

#include "FreeRTOS.h"
#include "task.h"

#include "mr_frame_buff_ctrl.h"
#include "mr_msg_ctrl.h"

#include "mr_sdio_drv.h"
#include "mr_netdev.h"

#include "msg_ctrl_sdio.h"

#define DBG_TAG "MSG"
#include "log.h"

#define FRAME_BUFFER_ATTR __ALIGNED(64) ATTR_WIFI_RAM_SECTION

static FRAME_BUFFER_ATTR msg_sdio_dnld_fmt_t msg_dnld_frame_buff[MSG_DNLD_FRAME_CNT + 1];

mr_msg_ctrl_priv_t *g_msg_sdio_ctrl = NULL;

static const mr_lp_gpio_cfg_t g_lp_inband_cfg = {
    .wake_notify_pin = GPIO_PIN_15,
    .ready_pin = GPIO_PIN_10,
    .wake_out_active_level = 0,
    .resume_cfg = GPIO_FUNC_SDU | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1,
    .notify_pin_cfg = GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1,
    .ready_pin_cfg = GPIO_INPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1,
    .ready_irq_trig = GPIO_INT_TRIG_MODE_ASYNC_RISING_EDGE,
};

static const mr_lp_gpio_cfg_t g_lp_oob_cfg = {
    .wake_notify_pin = GPIO_PIN_15,
    .ready_pin = GPIO_PIN_28,
    .wake_out_active_level = 0,
    .resume_cfg = GPIO_FUNC_SDU | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1,
    .notify_pin_cfg = GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1,
    .ready_pin_cfg = GPIO_INPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1,
    .ready_irq_trig = GPIO_INT_TRIG_MODE_ASYNC_FALLING_EDGE,
};

int msg_ctrl_sdio_init(void)
{
    mr_msg_ctrl_priv_t *msg_ctrl;

    if (g_msg_sdio_ctrl != NULL) {
        LOG_E("msg_ctrl already initialized\r\n");
        return -1;
    }

    mr_msg_ctrl_cfg_t msg_ctrl_cfg = {
        .name = "msg_ctrl_sdio",
        .dev_ops = &msg_ctrl_sdio_ops,
        .dnld_frame_size = sizeof(msg_sdio_dnld_fmt_t),
        .dnld_frame_count = MSG_DNLD_FRAME_CNT,
        .dnld_frame_buff = msg_dnld_frame_buff[0].frame_elem,
        .dnld_frame_type = 0x01,
        .upld_queue_depth = 64,
        .task_priority = 27,
        .task_stack_size = 1024,
        .task_period_max_ms = portMAX_DELAY,
        .msg_task_cb = NULL,
#if CONFIG_SDIO_OOB
        .lp_cfg = &g_lp_oob_cfg,
#else
        .lp_cfg = &g_lp_inband_cfg,
#endif
    };

    msg_ctrl = mr_msg_ctrl_init(&msg_ctrl_cfg);
    if (msg_ctrl == NULL) {
        LOG_E("Failed to initialize message controller\r\n");
        return -1;
    }

    g_msg_sdio_ctrl = msg_ctrl;

    LOG_I("DNLD Frame Buffer Info:\r\n");
    LOG_I("  - Buffer address: 0x%p, size: %u bytes\r\n",
          msg_dnld_frame_buff, sizeof(msg_dnld_frame_buff));
    LOG_I("  - Max buffer count: %u\r\n", MSG_DNLD_FRAME_CNT);
    LOG_I("  - Single frame size: %u bytes\r\n", sizeof(msg_sdio_dnld_fmt_t));

    return 0;
}
