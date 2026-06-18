/**
 * @file mr_hci.c
 * @brief HCI transport over message router
 */

#include <stdlib.h>
#include <string.h>

#include "qcc74x_core.h"
#include "board.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "mr_credit_flow.h"
#include "mr_frame_buff_ctrl.h"
#include "mr_hci.h"
#include "mr_msg_ctrl.h"
#include "mr_msg_ctrl_stat.h"

#define DBG_TAG "HCI"
#include "log.h"

#if DEBUG_HCI_LOG_ENABLE
#define HCI_DEBUG LOG_D
#else
#define HCI_DEBUG(fmt, ...)
#endif

#define HCI_ERR  LOG_E
#define HCI_INFO LOG_I

#define FRAME_BUFFER_ATTR __ALIGNED(32) ATTR_NOCACHE_NOINIT_RAM_SECTION

static FRAME_BUFFER_ATTR uint8_t hci_frame_buff[MR_HCI_FRAME_CNT][MR_HCI_RX_BUFF_SIZE + FRAME_BUFF_HEADER_ROOM];

static mr_hci_priv_t *g_hci_priv;
static mr_hci_priv_t g_hci_ctx;

extern void btble_sdio_rx_copy(uint8_t *data, uint32_t len);
extern void btble_sdio_uart_rx_event(void);

static int hci_send_cmd_packet(mr_hci_priv_t *priv, uint8_t hci_flag, int credit);

static int hci_proc_task_wakeup(mr_hci_priv_t *priv)
{
    if (priv->hci_proc_task == NULL) {
        HCI_ERR("hci_proc_task is NULL\r\n");
        return -1;
    }

    if (xPortIsInsideInterrupt()) {
        BaseType_t higher_priority_task_woken = pdFALSE;

        vTaskNotifyGiveFromISR(priv->hci_proc_task, &higher_priority_task_woken);
        if (higher_priority_task_woken) {
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    } else {
        xTaskNotifyGive(priv->hci_proc_task);
    }

    return 0;
}

static int hci_dnld_recv_done_cb(mr_frame_elem_t *frame_elem, void *arg)
{
    int ret;
    mr_hci_priv_t *priv = (mr_hci_priv_t *)arg;

    if (priv == NULL || frame_elem == NULL) {
        return -1;
    }

    ret = mr_frame_queue_send(priv->msg_dnld_queue, &frame_elem, 0);
    if (ret < 0) {
        HCI_ERR("Failed to send frame to download queue\r\n");
        mr_frame_queue_free_elem(frame_elem);
        return -1;
    }

    hci_proc_task_wakeup(priv);
    return 0;
}

static int hci_upld_send_done_cb(mr_frame_elem_t *frame_elem, void *arg, bool success)
{
    mr_hci_priv_t *priv = (mr_hci_priv_t *)arg;
    mr_hci_msg_t *msg_pkt;

    if (priv == NULL || frame_elem == NULL) {
        return -1;
    }

    msg_pkt = (mr_hci_msg_t *)frame_elem->buff_addr;
    if (!success && msg_pkt->flag == MR_HCI_FLAG_UPLD_DATA) {
        mr_tx_credit_put(&priv->flow_ctrl);
    }

    return mr_frame_queue_free_elem(frame_elem);
}

static int hci_hw_reset_cb(mr_frame_elem_t *unused, void *arg)
{
    mr_hci_priv_t *priv = (mr_hci_priv_t *)arg;

    (void)unused;

    if (priv == NULL) {
        return -1;
    }

    priv->hci_status = MR_HCI_STA_IDLE;
    priv->hci_ready = false;
    priv->reset_flag = true;
    mr_credit_reset(&priv->flow_ctrl);
    hci_proc_task_wakeup(priv);

    return 0;
}

static int hci_is_busy(void *arg)
{
    mr_hci_priv_t *priv = (mr_hci_priv_t *)arg;

    if (priv == NULL) {
        return 0;
    }

    if (priv->hci_status != MR_HCI_STA_RUN) {
        return 1;
    }

    if (priv->rx_frame_ctrl != NULL &&
        mr_frame_queue_messages_waiting(priv->rx_frame_ctrl->pool_queue) <
            priv->rx_frame_ctrl->frame_elem_cnt) {
        return 1;
    }

    if (priv->msg_dnld_queue != NULL &&
        mr_frame_queue_messages_waiting(priv->msg_dnld_queue) > 0) {
        return 1;
    }

    return 0;
}

static void hci_proc_task(void *arg)
{
    int ret;
    mr_hci_priv_t *priv = (mr_hci_priv_t *)arg;
    mr_frame_elem_t *frame_elem = NULL;
    mr_hci_msg_t *msg_pkt = NULL;
    bool msg_dnld_recv_suspend = false;

    HCI_INFO("HCI daemon thread started\r\n");

    priv->hci_status = MR_HCI_STA_IDLE;
    priv->hci_ready = false;

    while (!priv->stop_requested) {
        if (msg_dnld_recv_suspend) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        if (priv->reset_flag) {
            priv->reset_flag = false;
            priv->hci_ready = false;
            mr_credit_reset(&priv->flow_ctrl);
        }

        ret = mr_frame_queue_receive(priv->msg_dnld_queue, &frame_elem, 0);
        if (ret < 0) {
            msg_dnld_recv_suspend = true;
            frame_elem = NULL;
            msg_pkt = NULL;
        } else {
            msg_dnld_recv_suspend = false;
            msg_pkt = (mr_hci_msg_t *)frame_elem->buff_addr;
        }

        if (msg_pkt != NULL &&
            (msg_pkt->flag == MR_HCI_FLAG_HOST_RESET ||
             msg_pkt->flag == MR_HCI_FLAG_HOST_STOP)) {
            HCI_INFO("Host %s, entering IDLE\r\n",
                     msg_pkt->flag == MR_HCI_FLAG_HOST_RESET ? "RESET" : "STOP");
            priv->hci_ready = false;
            priv->hci_status = MR_HCI_STA_IDLE;
            mr_credit_reset(&priv->flow_ctrl);
            mr_frame_queue_free_elem(frame_elem);
            continue;
        }

        switch (priv->hci_status) {
            case MR_HCI_STA_IDLE:
                if (msg_pkt == NULL || msg_pkt->flag != MR_HCI_FLAG_HOST_READY) {
                    break;
                }

                ret = mr_credit_init(&priv->flow_ctrl, "HCI",
                                     msg_pkt->credit_limit_cnt,
                                     MR_HCI_DNLD_CREDIT_UPDATE_THRESHOLD,
                                     MR_HCI_DNLD_CREDIT_MAX,
                                     MR_HCI_DNLD_CREDIT_UPDATE_THRESHOLD);
                if (ret < 0) {
                    HCI_ERR("Failed to init HCI flow control\r\n");
                    break;
                }

                ret = hci_send_cmd_packet(priv, MR_HCI_FLAG_DEVICE_START, MR_HCI_DNLD_CREDIT_MAX);
                if (ret < 0) {
                    HCI_ERR("Failed to send DEVICE_START\r\n");
                    break;
                }
                MR_MSG_CTRL_RESET_STREAM_STAT(priv->msg_ctrl, priv->msg_tag);
                priv->hci_status = MR_HCI_STA_RUN;
                priv->hci_ready = true;
                mr_credit_enable(&priv->flow_ctrl, MR_CREDIT_EN_TX | MR_CREDIT_EN_RX);

                HCI_INFO("Host ready (credit=%u), sending DEVICE_START\r\n",
                         priv->flow_ctrl.credit[MR_CREDIT_DIR_TX].credit_limit_cnt);
                break;

            case MR_HCI_STA_RUN:
                if (msg_pkt != NULL && msg_pkt->credit_update_flag) {
                    mr_tx_credit_update(&priv->flow_ctrl, msg_pkt->credit_limit_cnt);
                }
                break;

            default:
                HCI_ERR("Invalid HCI status: %d\r\n", priv->hci_status);
                priv->hci_status = MR_HCI_STA_IDLE;
                break;
        }

        if (msg_pkt != NULL) {
            if (priv->hci_status == MR_HCI_STA_RUN &&
                msg_pkt->flag == MR_HCI_FLAG_DNLD_DATA) {
                uint32_t msg_len = msg_pkt->msg_pkt.len;
                uint32_t meta_size = sizeof(mr_hci_msg_t) - sizeof(struct mr_msg_pkt);

                if (msg_len >= meta_size) {
                    uint32_t payload_len = msg_len - meta_size;

                    if (payload_len > 0) {
                        HCI_DEBUG("RX payload: len=%lu\r\n", payload_len);
                        btble_sdio_rx_copy(msg_pkt->data, payload_len);
                        btble_sdio_uart_rx_event();
                    }

                    mr_rx_credit_reclaim(&priv->flow_ctrl);
                    if (mr_rx_credit_need_update(&priv->flow_ctrl)) {
                        hci_proc_task_wakeup(priv);
                    }
                }
            }
            mr_frame_queue_free_elem(frame_elem);
        }

        if (priv->hci_status == MR_HCI_STA_RUN &&
            priv->hci_ready &&
            mr_rx_credit_need_update(&priv->flow_ctrl)) {
            ret = mr_rx_credit_update(&priv->flow_ctrl);
            if (ret >= 0) {
                ret = hci_send_cmd_packet(priv, MR_HCI_FLAG_CREDIT_UPDATE, ret);
            }
            if (ret < 0) {
                HCI_ERR("Failed to send credit update\r\n");
            }
        }
    }

    HCI_INFO("HCI daemon thread exiting\r\n");
    vTaskDelete(NULL);
}

static int hci_send_cmd_packet(mr_hci_priv_t *priv, uint8_t hci_flag, int credit)
{
    int ret;
    mr_frame_elem_t *frame_elem;
    mr_hci_msg_t *msg_pkt;

    ret = mr_frame_queue_alloc_elem(priv->rx_frame_ctrl, &frame_elem, 10);
    if (ret < 0) {
        return -1;
    }

    msg_pkt = (mr_hci_msg_t *)frame_elem->buff_addr;
    msg_pkt->msg_pkt.tag = priv->msg_tag;
    msg_pkt->msg_pkt.sub_tag = 0;
    msg_pkt->msg_pkt.len = sizeof(mr_hci_msg_t) - sizeof(struct mr_msg_pkt);
    msg_pkt->reserved[0] = 0;
    msg_pkt->flag = hci_flag;
    msg_pkt->credit_update_flag = (credit >= 0) ? 1 : 0;
    msg_pkt->credit_limit_cnt = (credit >= 0) ? (uint8_t)credit : 0;
    frame_elem->data_size = sizeof(mr_hci_msg_t);

    ret = mr_msg_ctrl_upld_send(priv->msg_ctrl, frame_elem);
    if (ret < 0) {
        mr_frame_queue_free_elem(frame_elem);
        return -1;
    }

    return 0;
}

int mr_hci_init(mr_hci_priv_t *ctx, mr_msg_ctrl_priv_t *msg_ctrl, uint8_t msg_tag)
{
    int ret;
    mr_hci_priv_t *priv = ctx;
    mr_frame_queue_ctrl_init_cfg_t rx_cfg = {
        .frame_buff = hci_frame_buff,
        .frame_type = 0x03,
        .frame_elem_cnt = MR_HCI_FRAME_CNT,
        .frame_elem_size = MR_HCI_RX_BUFF_SIZE + FRAME_BUFF_HEADER_ROOM,
        .name = "hci_rx"
    };

    if (priv == NULL || msg_ctrl == NULL) {
        return -1;
    }

    memset(priv, 0, sizeof(*priv));
    priv->msg_ctrl = msg_ctrl;
    priv->msg_tag = msg_tag;
    priv->hci_status = MR_HCI_STA_IDLE;

    ret = mr_credit_init(&priv->flow_ctrl, "HCI",
                         0,
                         MR_HCI_DNLD_CREDIT_UPDATE_THRESHOLD,
                         MR_HCI_DNLD_CREDIT_MAX,
                         MR_HCI_DNLD_CREDIT_UPDATE_THRESHOLD);
    if (ret < 0) {
        HCI_ERR("Failed to init HCI flow control\r\n");
        return -1;
    }

    priv->rx_frame_ctrl = mr_frame_queue_create(&rx_cfg);
    if (priv->rx_frame_ctrl == NULL) {
        HCI_ERR("Failed to create RX frame buffer controller\r\n");
        return -1;
    }

    priv->msg_dnld_queue = xQueueCreate(MR_HCI_MSG_QUEUE_DEPTH, sizeof(mr_frame_elem_t *));
    if (priv->msg_dnld_queue == NULL) {
        HCI_ERR("Failed to create download queue\r\n");
        return -1;
    }

    ret = mr_msg_cb_register(msg_ctrl, msg_tag,
                             hci_dnld_recv_done_cb, priv,
                             hci_upld_send_done_cb, priv,
                             hci_hw_reset_cb, priv,
                             hci_is_busy);
    if (ret < 0) {
        HCI_ERR("Failed to register message callbacks: %d\r\n", ret);
        return -1;
    }

    ret = xTaskCreate(hci_proc_task, "hci_proc", 512, priv, 10, &priv->hci_proc_task);
    if (ret != pdPASS) {
        HCI_ERR("Failed to create daemon thread: %d\r\n", ret);
        return -1;
    }

    g_hci_priv = priv;

    HCI_INFO("HCI device initialized successfully\r\n");
    return 0;
}

int mr_hci_send(const void *data, uint32_t len)
{
    int ret;
    mr_frame_elem_t *frame_elem;
    mr_hci_msg_t *msg_pkt;

    if (data == NULL || len == 0 || len > MR_HCI_RX_BUFF_SIZE) {
        return -1;
    }

    if (g_hci_priv == NULL) {
        return -2;
    }

    if (!g_hci_priv->hci_ready) {
        return -3;
    }

    ret = mr_tx_credit_get(&g_hci_priv->flow_ctrl, portMAX_DELAY);
    if (ret < 0) {
        HCI_ERR("Failed to get HCI upload credit\r\n");
        return -1;
    }

    ret = mr_frame_queue_alloc_elem(g_hci_priv->rx_frame_ctrl, &frame_elem, 1000);
    if (ret < 0) {
        mr_tx_credit_put(&g_hci_priv->flow_ctrl);
        return -3;
    }

    msg_pkt = (mr_hci_msg_t *)frame_elem->buff_addr;
    msg_pkt->msg_pkt.tag = g_hci_priv->msg_tag;
    msg_pkt->msg_pkt.sub_tag = 0;
    msg_pkt->msg_pkt.len = sizeof(mr_hci_msg_t) - sizeof(struct mr_msg_pkt) + len;
    msg_pkt->reserved[0] = 0;
    msg_pkt->flag = MR_HCI_FLAG_UPLD_DATA;
    frame_elem->data_size = sizeof(mr_hci_msg_t) + len;
    memcpy(msg_pkt->data, data, len);

    ret = mr_rx_credit_update(&g_hci_priv->flow_ctrl);
    msg_pkt->credit_limit_cnt = (ret >= 0) ? (uint8_t)ret : 0;
    msg_pkt->credit_update_flag = (ret >= 0) ? 1 : 0;

    ret = mr_msg_ctrl_upld_send(g_hci_priv->msg_ctrl, frame_elem);
    if (ret < 0) {
        mr_tx_credit_put(&g_hci_priv->flow_ctrl);
        mr_frame_queue_free_elem(frame_elem);
        return ret;
    }

    return 0;
}

int mr_hci_send_skb(const void *data, uint32_t len, void (*free_cb)(void *arg), void *cb_arg)
{
    int ret = mr_hci_send(data, len);

    if (ret == 0 && free_cb != NULL) {
        free_cb(cb_arg);
    }

    return ret;
}

int mr_hci_channel_init(mr_msg_ctrl_priv_t *msg_ctrl)
{
    return mr_hci_init(&g_hci_ctx, msg_ctrl, MR_MSG_TAG_HCI);
}

int btble_sdio_hci_write(uint8_t *buf, uint32_t len)
{
    return mr_hci_send(buf, len);
}
