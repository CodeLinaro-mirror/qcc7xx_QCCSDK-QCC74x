/**
 * @file transporthci.c
 * @brief HCI transport layer over SDIO implementation
 */

#include "qcc74x_core.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>

#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"
#include "transporthci.h"
#include "nethub_types.h"

#define DBG_TAG "HCI"
#include "log.h"

/* Debug configuration */
#if DEBUG_HCI_LOG_ENABLE
#define HCI_DEBUG   LOG_D
#else
#define HCI_DEBUG(fmt, ...)
#endif

#define HCI_ERR     LOG_E
#define HCI_INFO    LOG_I

/* HCI H4 packet types */
#define HCI_H4_CMD 0x01
#define HCI_H4_ACL 0x02
#define HCI_H4_SCO 0x03
#define HCI_H4_EVT 0x04

/*****************************************************************************
 * Global Variables
 *****************************************************************************/

/** Frame buffer memory attribute (aligned and non-cached) */
#define FRAME_BUFFER_ATTR __ALIGNED(32) ATTR_NOCACHE_NOINIT_RAM_SECTION
static FRAME_BUFFER_ATTR uint8_t hci_frame_buff[HCI_FRAME_CNT][FRAME_BUFF_TOTALSIZE];

/** Global private structure pointer */
static struct hci_priv *g_hci_priv = NULL;

/** External callback for received data */
extern void btble_sdio_rx_copy(uint8_t *data, uint32_t len);
extern void btble_sdio_uart_rx_event(void);

/*****************************************************************************
 * Forward Declarations
 *****************************************************************************/

static void hci_proc_task(void *arg);
static int hci_dnld_recv_done_cb(frame_elem_t *frame_elem, void *arg);
static int hci_upld_send_done_cb(frame_elem_t *frame_elem, void *arg, bool success);
static int hci_hw_reset_cb(frame_elem_t *unused, void *arg);
static int hci_send_cmd_packt(struct hci_priv *priv, uint8_t flag, int credit);

/*****************************************************************************
 * Internal Functions
 *****************************************************************************/

/**
 * @brief Wake up the HCI processing task
 */
static int hci_proc_task_wakeup(struct hci_priv *priv)
{
    if (priv->hci_proc_task == NULL) {
        HCI_ERR("hci_proc_task is NULL\r\n");
        return -1;
    }

    /* Wake up message processing task */
    if (xPortIsInsideInterrupt()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(priv->hci_proc_task, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotifyGive(priv->hci_proc_task);
    }

    return 0;
}

/**
 * @brief MSG download receive done callback
 */
static int hci_dnld_recv_done_cb(frame_elem_t *frame_elem, void *arg)
{
    int ret;
    struct hci_priv *priv = (struct hci_priv *)arg;

    if (!priv || !frame_elem) {
        HCI_ERR("Invalid args in recv_done_cb\r\n");
        return -1;
    }

    ret = frame_queue_send(priv->msg_dnld_queue, &frame_elem, 0);
    if (ret < 0) {
        HCI_ERR("Failed to send frame to download queue\r\n");
        frame_queue_free_elem(frame_elem);
        return -1;
    }

    hci_proc_task_wakeup(priv);
    return 0;
}

/**
 * @brief MSG upload send done callback
 */
static int hci_upld_send_done_cb(frame_elem_t *frame_elem, void *arg, bool success)
{
    struct hci_priv *priv = (struct hci_priv *)arg;
    struct nh_hci_msg_packt *msg_packt;

    if (!frame_elem) {
        return -1;
    }

    msg_packt = (struct nh_hci_msg_packt *)frame_elem->buff_addr;
    if (!success && msg_packt->flag == NH_HCI_FLAG_UPLD_DATA) {
        nh_tx_credit_put(&priv->flow_ctrl);
    }

    return frame_queue_free_elem(frame_elem);
}

/**
 * @brief SDIO hardware reset callback
 * @details Called by MSG layer when SDIO bus resets; flags the task to re-enter IDLE.
 */
static int hci_hw_reset_cb(frame_elem_t *unused, void *arg)
{
    struct hci_priv *priv = (struct hci_priv *)arg;

    (void)unused;
    nh_credit_reset(&priv->flow_ctrl);
    priv->reset_flag = true;
    hci_proc_task_wakeup(priv);

    HCI_INFO("HW reset received\r\n");
    return 0;
}

/**
 * @brief HCI daemon thread
 * @details Main tasks:
 *   1. Process received HCI frames from host
 *   2. Send HCI frames to host
 * @param[in] arg Pointer to HCI private structure
 */
static void hci_proc_task(void *arg)
{
    int ret;
    struct hci_priv *priv = (struct hci_priv *)arg;
    frame_elem_t *frame_elem;
    struct nh_hci_msg_packt *msg_packt;
    bool msg_dnld_recv_suspend = false;

    LOG_I("HCI daemon thread started\r\n");

    /* Initial state — wait for host handshake */
    priv->hci_status = nh_HCI_STA_IDLE;
    priv->hci_ready = false;

    while (!priv->stop_requested) {
        /* Wait for notification — only when queue was empty last iteration */
        if (msg_dnld_recv_suspend) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        }

        /* Handle SDIO hardware reset */
        if (priv->reset_flag) {
            HCI_INFO("Processing HW reset, entering IDLE\r\n");
            priv->reset_flag = false;
            priv->hci_ready = false;
            priv->hci_status = nh_HCI_STA_IDLE;
            nh_credit_reset(&priv->flow_ctrl);
        }

        /* get msg recv frame */
        ret = frame_queue_receive(priv->msg_dnld_queue, &frame_elem, 0);
        if (ret < 0) {
            msg_dnld_recv_suspend = true;
            frame_elem = NULL;
            msg_packt = NULL;
        } else {
            msg_dnld_recv_suspend = false;
            msg_packt = (struct nh_hci_msg_packt *)frame_elem->buff_addr;
        }

        /* Process reset/stop commands and reset the state machine */
        if (msg_packt && (msg_packt->flag == NH_HCI_FLAG_HOST_RESET ||
                          msg_packt->flag == NH_HCI_FLAG_HOST_STOP)) {
            HCI_INFO("Host %s, entering IDLE\r\n",
                     msg_packt->flag == NH_HCI_FLAG_HOST_RESET ? "RESET" : "STOP");
            priv->hci_ready = false;
            priv->hci_status = nh_HCI_STA_IDLE;
            nh_credit_reset(&priv->flow_ctrl);
            frame_queue_free_elem(frame_elem);
            continue;
        }

        /* State machine */
        switch (priv->hci_status) {
            case nh_HCI_STA_IDLE:
                if (msg_packt == NULL || msg_packt->flag != NH_HCI_FLAG_HOST_READY) {
                    break;
                }
                HCI_INFO("Host READY (host_rx_wnd=%u)\r\n", msg_packt->credit_limit_cnt);
                nh_credit_init(&priv->flow_ctrl, "HCI",
                               msg_packt->credit_limit_cnt,
                               HCI_DNLD_CREDIT_UPDATE_THRESHOLD,
                               HCI_DNLD_CREDIT_MAX,
                               HCI_DNLD_CREDIT_UPDATE_THRESHOLD);

                HCI_INFO("Host ready (credit=%u), sending DEVICE_READY\r\n",
                         priv->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt);
                ret = hci_send_cmd_packt(priv, NH_HCI_FLAG_DEVICE_READY, -1);
                if (ret < 0) {
                    HCI_ERR("Failed to send DEVICE_READY\r\n");
                    break;
                }
                priv->hci_status = nh_HCI_STA_DEVICE_READY;
                break;

            case nh_HCI_STA_DEVICE_READY:
                ret = hci_send_cmd_packt(priv, NH_HCI_FLAG_DEVICE_START, HCI_DNLD_CREDIT_MAX);
                if (ret < 0) {
                    HCI_ERR("Failed to send DEVICE_START\r\n");
                    break;
                }
                nh_msg_ctrl_reset_stream_stat(priv->msg_ctrl, priv->msg_tag);
                priv->hci_status = nh_HCI_STA_RUN;
                priv->hci_ready = true;
                nh_credit_enable(&priv->flow_ctrl, NH_CREDIT_EN_TX | NH_CREDIT_EN_RX);
                HCI_INFO("HCI handshake complete, entering RUN\r\n");
                break;

            case nh_HCI_STA_RUN:
                if (msg_packt == NULL) {
                    break;
                }

#if HCI_DNLD_CREDIT_MAX
                if (msg_packt->credit_update_flag) {
                    nh_tx_credit_update(&priv->flow_ctrl, msg_packt->credit_limit_cnt);
                }
#endif
                break;

            default:
                HCI_ERR("Invalid HCI status: %d\r\n", priv->hci_status);
                priv->hci_status = nh_HCI_STA_IDLE;
                if (msg_packt) {
                    frame_queue_free_elem(frame_elem);
                }
                break;
        }

        /* Process received data packet */
        if (msg_packt) {
            if (priv->hci_status == nh_HCI_STA_RUN &&
                msg_packt->flag == NH_HCI_FLAG_DNLD_DATA) {
                uint32_t msg_len = msg_packt->msg_packt.len;
                const uint32_t proto_meta_size = sizeof(struct nh_hci_msg_packt) - sizeof(struct nh_msg_packt);

                if (msg_len >= proto_meta_size) {
                    uint32_t payload_len = msg_len - proto_meta_size;

                    if (payload_len > 0) {
                        HCI_DEBUG("RX payload: len=%u\r\n", payload_len);
                        btble_sdio_rx_copy(msg_packt->data, payload_len);
                        btble_sdio_uart_rx_event();
                    }

#if HCI_DNLD_CREDIT_MAX
                    nh_rx_credit_reclaim(&priv->flow_ctrl);
                    if (nh_rx_credit_need_update(&priv->flow_ctrl)) {
                        hci_proc_task_wakeup(priv);
                    }
#endif
                }
            }
            frame_queue_free_elem(frame_elem);
        }

#if HCI_DNLD_CREDIT_MAX
        /* Send a proactive flow-control update */
        if (priv->hci_status == nh_HCI_STA_RUN && priv->hci_ready &&
            nh_rx_credit_need_update(&priv->flow_ctrl)) {
            ret = nh_rx_credit_update(&priv->flow_ctrl);
            if (ret >= 0) {
                ret = hci_send_cmd_packt(priv, NH_HCI_FLAG_CREDIT_UPDATE, ret);
            }
            if (ret < 0) {
                HCI_ERR("Failed to send credit update\r\n");
            }
        }
#endif
    }

    LOG_I("HCI daemon thread exiting\r\n");
    vTaskDelete(NULL);
}

/**
 * @brief Send a control or credit packet to host
 * @param[in] priv HCI private structure
 * @param[in] flag Packet flag (NH_HCI_FLAG_DEVICE_READY, _DEVICE_START, _CREDIT_UPDATE, …)
 * @param[in] credit Credit value carried in credit_limit_cnt (0 if not applicable)
 * @retval 0 Success
 * @retval <0 Error
 */
static int hci_send_cmd_packt(struct hci_priv *priv, uint8_t flag, int credit)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_hci_msg_packt *msg_packt;

    ret = frame_queue_alloc_elem(priv->rx_frame_ctrl, &frame_elem, 10);
    if (ret < 0) {
        return -1;
    }

    msg_packt = (struct nh_hci_msg_packt *)frame_elem->buff_addr;
    msg_packt->msg_packt.tag = priv->msg_tag;
    msg_packt->msg_packt.sub_tag = 0;
    msg_packt->msg_packt.len = sizeof(struct nh_hci_msg_packt) - sizeof(struct nh_msg_packt);
    msg_packt->reserved[0] = 0;
    msg_packt->flag = flag;
    msg_packt->credit_update_flag = (credit >= 0) ? 1 : 0;
    msg_packt->credit_limit_cnt = (credit >= 0) ? (uint8_t)credit : 0;
    frame_elem->data_size = sizeof(struct nh_hci_msg_packt);

    ret = nh_msg_ctrl_send(priv->msg_ctrl, frame_elem, 1);
    if (ret < 0) {
        frame_queue_free_elem(frame_elem);
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * Public API Implementation
 *****************************************************************************/

/**
 * @brief Initialize HCI device
 */
int transporthci_init(struct hci_priv *ctx, nh_msg_ctrl_t *msg_ctrl, uint8_t msg_tag)
{
    int ret;
    struct hci_priv *priv = (struct hci_priv *)ctx;

    /* Basic field initialization */
    priv->msg_ctrl = msg_ctrl;
    priv->msg_tag = msg_tag;
    priv->hci_status = nh_HCI_STA_IDLE;
    priv->stop_requested = false;
    priv->reset_flag = false;
    priv->hci_ready = false;

    HCI_INFO("HCI device initializing... msg_tag=%u (HCI=%u, WIFI=%u)\r\n",
             msg_tag, nh_MSG_TAG_HCI, nh_MSG_TAG_WIFI);

    ret = nh_credit_init(&priv->flow_ctrl, "HCI",
                         0,
                         HCI_DNLD_CREDIT_UPDATE_THRESHOLD,
                         HCI_DNLD_CREDIT_MAX,
                         HCI_DNLD_CREDIT_UPDATE_THRESHOLD);
    if (ret < 0) {
        HCI_ERR("Failed to init HCI flow control\r\n");
        return -1;
    }

    /* Initialize private memory pool */
    frame_queue_ctrl_init_cfg_t rx_cfg = {
        .frame_buff = hci_frame_buff,
        .frame_type = 0x03,
        .frame_elem_cnt = HCI_FRAME_CNT,
        .frame_elem_size = FRAME_BUFF_TOTALSIZE,
        .name = "hci_rx"
    };
    priv->rx_frame_ctrl = frame_queue_create(&rx_cfg);
    if (!priv->rx_frame_ctrl) {
        HCI_ERR("Failed to create RX frame buffer controller\r\n");
        return -1;
    }

    /* Create msg dnld queue */
    priv->msg_dnld_queue = xQueueCreate(HCI_MSG_QUEUE_DEPTH, sizeof(frame_elem_t *));

    /* Register MSG message callback */
    ret = nh_msg_cb_register(msg_ctrl, msg_tag,
        hci_dnld_recv_done_cb, priv,
        hci_upld_send_done_cb, priv,
        hci_hw_reset_cb, priv);
    if (ret < 0) {
        HCI_ERR("Failed to register message callbacks: %d\r\n", ret);
        return -1;
    }

    /* Create daemon thread */
    ret = xTaskCreate(hci_proc_task, "hci_proc", 2048, priv, 10, &priv->hci_proc_task);
    if (ret != pdPASS) {
        HCI_ERR("Failed to create daemon thread: %d\r\n", ret);
        return -1;
    }

    /* Save global pointer */
    g_hci_priv = priv;

    HCI_INFO("HCI device initialized successfully\r\n");

    return 0;
}

/**
 * @brief Send HCI data to host (upld)
 */
int transporthci_send(const void *data, uint32_t len)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_hci_msg_packt *msg_packt;

    if (!data || len == 0 || len > HCI_RX_BUFF_SIZE) {
        HCI_ERR("Invalid parameters: data=%p, len=%lu\r\n", data, len);
        return -1;
    }

    if (!g_hci_priv) {
        HCI_ERR("HCI device not initialized\r\n");
        return -2;
    }

    if (!g_hci_priv->hci_ready) {
        HCI_ERR("HCI not ready\r\n");
        return -3;
    }

    ret = nh_tx_credit_get(&g_hci_priv->flow_ctrl, portMAX_DELAY);
    if (ret < 0) {
        HCI_ERR("Failed to get HCI upld credit\r\n");
        return -1;
    }

    ret = frame_queue_alloc_elem(g_hci_priv->rx_frame_ctrl, &frame_elem, 1000);
    if (ret < 0) {
        HCI_ERR("Failed to allocate frame element\r\n");
        nh_tx_credit_put(&g_hci_priv->flow_ctrl);
        return -3;
    }

    msg_packt = (struct nh_hci_msg_packt *)frame_elem->buff_addr;

    msg_packt->msg_packt.tag = g_hci_priv->msg_tag;
    msg_packt->msg_packt.sub_tag = 0;
    msg_packt->msg_packt.len = sizeof(struct nh_hci_msg_packt) - sizeof(struct nh_msg_packt) + len;
    msg_packt->reserved[0] = 0;
    msg_packt->flag = NH_HCI_FLAG_UPLD_DATA;
    frame_elem->data_size = sizeof(struct nh_hci_msg_packt) + len;
    memcpy(msg_packt->data, data, len);

    HCI_DEBUG("HCI TX: type=0x%02X, len=%u\r\n", ((uint8_t*)data)[0], len);

#if HCI_DNLD_CREDIT_MAX
    ret = nh_rx_credit_update(&g_hci_priv->flow_ctrl);
    msg_packt->credit_limit_cnt = (ret >= 0) ? (uint8_t)ret : 0;
    msg_packt->credit_update_flag = (ret >= 0) ? 1 : 0;
    ret = nh_msg_ctrl_send(g_hci_priv->msg_ctrl, frame_elem, 0);
#else
    msg_packt->credit_update_flag = 0;
    msg_packt->credit_limit_cnt = 0;
    ret = nh_msg_ctrl_send(g_hci_priv->msg_ctrl, frame_elem, 0);
#endif

    if (ret < 0) {
        HCI_ERR("Failed to send upld data: %d\r\n", ret);
        nh_tx_credit_put(&g_hci_priv->flow_ctrl);
        frame_queue_free_elem(frame_elem);
        return ret;
    }

    return 0;
}

/**
 * @brief Send HCI data to host with skb callback (upld)
 */
int transporthci_send_skb(const void *data, uint32_t len,
                           void (*free_cb)(void *arg), void *cb_arg)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_hci_msg_packt *msg_packt;

    if (!data || len == 0 || len > HCI_RX_BUFF_SIZE) {
        HCI_ERR("Invalid parameters: data=%p, len=%lu\r\n", data, len);
        return -1;
    }

    if (!g_hci_priv) {
        HCI_ERR("HCI device not initialized\r\n");
        return -2;
    }

    if (!g_hci_priv->hci_ready) {
        HCI_ERR("HCI not ready\r\n");
        return -3;
    }

    ret = nh_tx_credit_get(&g_hci_priv->flow_ctrl, portMAX_DELAY);
    if (ret < 0) {
        HCI_ERR("Failed to get HCI upld credit\r\n");
        return -1;
    }

    ret = frame_queue_alloc_elem(g_hci_priv->rx_frame_ctrl, &frame_elem, 1000);
    if (ret < 0) {
        HCI_ERR("Failed to allocate frame element\r\n");
        nh_tx_credit_put(&g_hci_priv->flow_ctrl);
        return -3;
    }

    msg_packt = (struct nh_hci_msg_packt *)frame_elem->buff_addr;

    msg_packt->msg_packt.tag = g_hci_priv->msg_tag;
    msg_packt->msg_packt.sub_tag = 0;
    msg_packt->msg_packt.len = sizeof(struct nh_hci_msg_packt) - sizeof(struct nh_msg_packt) + len;
    msg_packt->reserved[0] = 0;
    msg_packt->flag = NH_HCI_FLAG_UPLD_DATA;
    frame_elem->data_size = sizeof(struct nh_hci_msg_packt) + len;
    memcpy(msg_packt->data, data, len);

    /* Data is copied; release the original buffer immediately */
    if (free_cb) {
        free_cb(cb_arg);
    }

    HCI_DEBUG("HCI TX skb: type=0x%02X, len=%u\r\n", ((uint8_t*)data)[0], len);

#if HCI_DNLD_CREDIT_MAX
    ret = nh_rx_credit_update(&g_hci_priv->flow_ctrl);
    msg_packt->credit_limit_cnt = (ret >= 0) ? (uint8_t)ret : 0;
    msg_packt->credit_update_flag = (ret >= 0) ? 1 : 0;
    ret = nh_msg_ctrl_send(g_hci_priv->msg_ctrl, frame_elem, 0);
#else
    msg_packt->credit_update_flag = 0;
    msg_packt->credit_limit_cnt = 0;
    ret = nh_msg_ctrl_send(g_hci_priv->msg_ctrl, frame_elem, 0);
#endif

    if (ret < 0) {
        HCI_ERR("Failed to send upld data: %d\r\n", ret);
        nh_tx_credit_put(&g_hci_priv->flow_ctrl);
        frame_queue_free_elem(frame_elem);
        return ret;
    }

    return 0;
}

/*****************************************************************************
 * External API for btble_sdio_uart
 *****************************************************************************/

/**
 * @brief Write HCI data (called from btble_sdio_uart_write)
 */
int btble_sdio_hci_write(uint8_t *buf, uint32_t len)
{
    return transporthci_send(buf, len);
}
