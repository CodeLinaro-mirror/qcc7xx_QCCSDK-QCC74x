#include "qcc74x_core.h"
#include "qcc74x_mtimer.h"

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_WL80211
#define FHOST_RX_BUF_CNT 20// fixme
#else
#include "fhost.h"
#define FHOST_RX_BUF_CNT            (NX_MAX_BA_RX * CFG_REORD_BUF + 2)
#endif

#ifdef CONFIG_SDIO3_SOFT_RST_INT_USER
#include <nh_sdio3.h>
#endif
#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"
#include "transportsdio.h"

#include <nethub_types.h>

#define DBG_TAG "WIFI_STA"
#include "log.h"

#define ETHSTA_DBGLOG  (1)

#define DEBUG_SDIO_LOG_ENABLE    (1)// 1-enable log. 0-no log
#define DEBUG_BYPASS_WIFI_TX     (0)// 1-not send to wifi fw. 0-normal
#define DEBUG_BYPASS_WIFI_RX     (0)// 1-not send to wifi fw. 0-normal
#define DEBUG_DUMP_WIFITX_ENABLE (0)// 1-dump wifi tx data. 0-not dump wifi tx data
#define DEBUG_DUMP_WIFIRX_ENABLE (0)// 1-dump wifi rx data. 0-not dump wifi rx data
/* Debug output for example program */
#define EXAMPLE_PRINTF printf
#if DEBUG_SDIO_LOG_ENABLE
#define EXAMPLE_DEBUG(fmt, ...) //LOG_I("[transport debug] " fmt, ##__VA_ARGS__)
#define EXAMPLE_INFO(fmt, ...)  //LOG_I("[transportsdio] " fmt, ##__VA_ARGS__)
#define EXAMPLE_ERR(fmt, ...)   //LOG_E("[transportsdio] " fmt, ##__VA_ARGS__)
#else
#define EXAMPLE_DEBUG(fmt, ...)
#define EXAMPLE_INFO(fmt, ...)
#define EXAMPLE_ERR(fmt, ...)
#endif

/*****************************************************************************
 * Global Variables
 *****************************************************************************/

/* External NETHUB statistics variable declaration */
extern nethub_statistics_t g_nethub_stats;

/** WiFi MAC address */
//static uint8_t wifi_mac_addr[6] = { 0x22, 0x22, 0x22, 0x22, 0x34, 0x56 };

/** Frame buffer memory attribute (aligned and non-cached) */
#define FRAME_BUFFER_ATTR __ALIGNED(32) ATTR_NOCACHE_NOINIT_RAM_SECTION
/** Download frame buffer pool */
static FRAME_BUFFER_ATTR uint8_t wifi_frame_buff[WIFI_FRAME_CNT][FRAME_BUFF_TOTALSIZE];

/** Global private structure pointer */
static wifi_wifista_priv_t *g_wifi_priv = NULL;


/* Payload offset mapping functions for SDIO upld direction */
struct payload_offset_entry {
    uint8_t *payload_addr;    /* payload address, NULL means unused */
    uint32_t offset;          /* corresponding offset value */
};

static struct payload_offset_entry offset_whitelist[FHOST_RX_BUF_CNT];

/** Free queue message structure */
typedef struct {
    frame_elem_t *frame_elem;
} free_msg_t;

/*****************************************************************************
 * Forward Declarations
 *****************************************************************************/

static void wifi_proc_task(void *arg);
static void frame_free_task(void *arg);

static int msg_dnld_recv_done_cb(frame_elem_t *frame_elem, void *arg);
static int msg_upld_send_done_cb(frame_elem_t *frame_elem, void *arg, bool success);
static int transportsdio_hwreset(frame_elem_t *frame_elem, void *arg);

/* WiFi TX completion callback */
static void wifi_tx_done_cb(frame_elem_t *frame_elem);

/**
 * @brief Wake up the WiFi processing task
 * @param[in] priv Pointer to WiFi private structure
 * @retval 0 Success
 * @retval <0 Error
 */
static int wifi_proc_task_wakeup(struct wifi_wifista_priv *priv)
{
    if (priv->wifi_proc_task == NULL) {
        EXAMPLE_ERR("wifi_proc_task is NULL, please init msg_ctrl first!\r\n");
        return -1;
    }

    /* Wake up message processing task */
    if (xPortIsInsideInterrupt()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(priv->wifi_proc_task, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotifyGive(priv->wifi_proc_task);
    }

    return 0;
}

/**
 * @brief Callback after frame buffer is freed, used to wake up the processing task
 * @param[in] frame_elem Pointer to frame element
 * @param[in] arg Pointer to WiFi private structure
 */
static void wifi_free_after_cb(frame_elem_t *frame_elem, void *arg)
{
    wifi_proc_task_wakeup((struct wifi_wifista_priv *)arg);
}

/**
 * @brief WiFi TX complete callback, directly releases frame
 * @param[in] priv Pointer to WiFi private structure
 * @param[in] frame_elem Pointer to frame element
 * @param[in] success TX success flag
 */
static void wifi_tx_done_cb(frame_elem_t *frame_elem)
{
    struct wifi_wifista_priv *priv = g_wifi_priv;
    if (!frame_elem) {
        return;
    }

    //EXAMPLE_PRINTF("elem:%p, buf:%p\r\n", frame_elem, frame_elem->buff_addr);
    /* Free frame from message */
    frame_queue_free_elem(frame_elem);

    nh_rx_credit_reclaim(&priv->flow_ctrl);

    /* Flow control threshold reached, wake up thread to update flow control */
    if (nh_rx_credit_need_update(&priv->flow_ctrl)) {
        wifi_proc_task_wakeup(priv);
    }
}

/**
 * @brief Send frame via WiFi interface
 * @param[in] priv Pointer to WiFi private structure
 * @param[in] frame_elem Pointer to frame element
 * @retval 0 Success
 * @retval <0 Error
 */
static int wifi_send_frame(struct wifi_wifista_priv *priv, frame_elem_t *frame_elem)
{
    int ret = -1;
    struct nh_wifi_msg_packt *msg_tx_packt;

    if (!priv || !frame_elem) {
        return -1;
    }

    msg_tx_packt = (struct nh_wifi_msg_packt *)frame_elem->buff_addr;

    if (msg_tx_packt->msg_packt.len >= 14) {
        static int s_dnld_cnt = 0;
        s_dnld_cnt++;
    #if DEBUG_DUMP_WIFITX_ENABLE
        EXAMPLE_INFO("Dnld[%d]<%d>  real_cb:%p, real_ctx:%p, real_arg:%p ----->  %02X:%02X:%02X:%02X:%02X:%02X  %02X:%02X:%02X:%02X:%02X:%02X   %02X:%02X .... %02X:%02X\r\n",
            msg_tx_packt->msg_packt.len, s_dnld_cnt, wifi_tx_done_cb, priv, frame_elem,
            msg_tx_packt->data[0], msg_tx_packt->data[1], msg_tx_packt->data[2], msg_tx_packt->data[3],
            msg_tx_packt->data[4], msg_tx_packt->data[5], msg_tx_packt->data[6], msg_tx_packt->data[7],
            msg_tx_packt->data[8], msg_tx_packt->data[9], msg_tx_packt->data[10], msg_tx_packt->data[11],
            msg_tx_packt->data[12], msg_tx_packt->data[13],
            msg_tx_packt->data[msg_tx_packt->msg_packt.len - 2], msg_tx_packt->data[msg_tx_packt->msg_packt.len - 1]);
    #endif
    }

#if 1
    if (priv->wifi_send_frame) {
        ret = priv->wifi_send_frame(priv,
                                   (void *)((uintptr_t)frame_elem->buff_addr + sizeof(struct nh_wifi_msg_packt)),
                                   msg_tx_packt->msg_packt.len + sizeof(nh_msg_packt_t) - sizeof(struct nh_wifi_msg_packt),
                                   wifi_tx_done_cb, frame_elem);
    }
#else
    wifi_tx_done_cb(priv, frame_elem);
#endif

    return ret;
}

/**
 * @brief Receive frame via WiFi interface
 * @param[in] priv Pointer to WiFi private structure
 * @param[in] frame_elem Pointer to frame element
 * @retval 0 Success
 * @retval <0 Error
 */
static int wifi_recv_frame(struct wifi_wifista_priv *priv, frame_elem_t *frame_elem)
{
    int ret = 0;

    if (!priv || !frame_elem) {
        return -1;
    }

    if (priv->wifi_recv_frame) {
        ret = priv->wifi_recv_frame(priv,
                                   (void *)((uintptr_t)frame_elem->buff_addr + sizeof(struct nh_wifi_msg_packt)),
                                   WIFI_RX_BUFF_SIZE);
    }

    return ret;
}

/**
 * @brief MSG download receive done callback, puts received message into queue for processing
 * @param[in] frame_elem Pointer to frame element
 * @param[in] arg Pointer to WiFi private structure
 * @retval 0 Success
 * @retval <0 Error
 */
static int msg_dnld_recv_done_cb(frame_elem_t *frame_elem, void *arg)
{
    int ret;
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)arg;

    if (!priv || !frame_elem) {
        return -1;
    }

    ret = frame_queue_send(priv->msg_dnld_queue, &frame_elem, 0);
    #if CONFIG_NETHUB_DUMP_DNLDDATA
    EXAMPLE_PRINTF("send frame_elem:%p, buf:%p, ret:%d, queue_len:%d\n", frame_elem, frame_elem->buff_addr, ret, frame_queue_messages_waiting(priv->msg_dnld_queue));
    #endif
    if (ret < 0) {
        EXAMPLE_ERR("Failed to send frame to download queue\r\n");
        return -1;
    }

    /* Statistics: DNLD producer generates packets */
    //g_nethub_stats.dnld_producer_count++;

    wifi_proc_task_wakeup(priv);
    return 0;
}

/**
 * @brief MSG upload send done callback, releases data back to pool
 * @param[in] frame_elem Pointer to frame element
 * @param[in] arg Pointer to WiFi private structure
 * @retval 0 Success
 * @retval <0 Error
 */
#if 0
static int msg_upld2_send_done_cb(frame_elem_t *frame_elem, void *arg)
{
    int ret;
    skb_free_cb_t skb_free = NULL;
    void *skb;

    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)arg;
    if (!frame_elem) {
        return -1;
    }
    if (!priv) {
        EXAMPLE_ERR("\x1b[33m%s:%d\x1b[0m priv is NULL.\r\n", __func__, __LINE__);
        return -1;
    }

    if (frame_elem->type == 0x11) {
        skb_free = frame_elem->skb_free;
        skb = frame_elem->skb;

        if (skb_free) {
            skb_free(skb);
        }
    } else {
        ret = frame_queue_send(priv->msg_dnld_queue, &frame_elem, 0);
        if (ret < 0) {
            EXAMPLE_ERR("Failed to send frame to download queue\r\n");
            return -1;
        }

        wifi_proc_task_wakeup(priv);
    }
    return 0;
}
#endif
static int msg_upld_send_done_customfree(frame_elem_t *frame_elem)
{
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)g_wifi_priv;
    free_msg_t free_msg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (!priv || !frame_elem) {
        return -1;
    }

    /* In interrupt context, send message to frame_free_queue, let frame_free_task free frame */
    free_msg.frame_elem = frame_elem;

    if (xPortIsInsideInterrupt()) {
        /* Use ISR version in interrupt context */
        if (xQueueSendFromISR(priv->frame_free_queue, &free_msg, &xHigherPriorityTaskWoken) != pdTRUE) {
            return -1;
        }
        /* Perform task switch if needed */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        /* Send directly in non-interrupt context */
        if (xQueueSend(priv->frame_free_queue, &free_msg, 0) != pdTRUE) {
            return -1;
        }
    }

    return 0;
}

static int msg_upld_send_done_cb(frame_elem_t *frame_elem, void *arg, bool success)
{
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)arg;
    struct nh_wifi_msg_packt *msg_packt;

    EXAMPLE_DEBUG("%s, frame_elem:%p, frame_elem->skb_free:%p\r\n",
        __FUNCTION__, frame_elem, frame_elem->skb_free);

    if (!frame_elem) {
        return -1;
    }

    msg_packt = (struct nh_wifi_msg_packt *)frame_elem->buff_addr;
    if (!success && priv && msg_packt->flag == nh_WIFI_FLAG_UPLD_DATA) {
        nh_tx_credit_put(&priv->flow_ctrl);
    }

    if (frame_elem && (frame_elem->skb_free == NULL)) {
        return frame_queue_free_elem(frame_elem);
    }
    return msg_upld_send_done_customfree(frame_elem);
}

static int transportsdio_hwreset(frame_elem_t *frame_elem, void *arg)
{
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)arg;

    (void)frame_elem;

    if (priv == NULL) {
        return -1;
    }

    priv->wifi_status = nh_WIFI_DSTA_IDLE;
    nh_credit_reset(&priv->flow_ctrl);

    return 0;
}

/**
 * @brief Send MSG upload control packet
 * @param[in] priv Pointer to WiFi private structure
 * @param[in] wifi_flag WiFi flag to send
 * @retval 0 Success
 * @retval <0 Error
 */
static int msg_upld_send_cmd_packet(struct wifi_wifista_priv *priv, uint8_t wifi_flag, uint8_t tofront, int credit)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_wifi_msg_packt *msg_tx_packt;

    ret = frame_queue_alloc_elem(priv->rx_frame_ctrl, &frame_elem, 1000);
    if (ret < 0) {
        EXAMPLE_ERR("Failed to allocate RX frame element\r\n");
        return -1;
    }

    frame_elem->skb_free = NULL;
    frame_elem->skb = NULL;
    EXAMPLE_DEBUG("===%s, frame_elem:%p\r\n", __FUNCTION__, frame_elem);

    msg_tx_packt = (struct nh_wifi_msg_packt *)frame_elem->buff_addr;

    if (wifi_flag == nh_WIFI_FLAG_DEVICE_READY) {
        /* Fill MAC address */
        frame_elem->data_size = sizeof(struct nh_wifi_msg_packt) + sizeof(priv->mac_addr);
        memcpy(msg_tx_packt->data, priv->mac_addr, sizeof(priv->mac_addr));
        EXAMPLE_INFO("Sending DEVICE_READY with MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
              priv->mac_addr[0], priv->mac_addr[1], priv->mac_addr[2],
              priv->mac_addr[3], priv->mac_addr[4], priv->mac_addr[5]);
    } else {
        frame_elem->data_size = sizeof(struct nh_wifi_msg_packt);
    }

    msg_tx_packt->msg_packt.len = frame_elem->data_size - sizeof(nh_msg_packt_t);
    msg_tx_packt->msg_packt.tag = priv->msg_tag;
    msg_tx_packt->msg_packt.sub_tag = 0;
    msg_tx_packt->flag = wifi_flag;

    if (credit >= 0) {
        msg_tx_packt->credit_update_flag = true;
        msg_tx_packt->credit_limit_cnt = (uint8_t)credit;
    } else {
        msg_tx_packt->credit_update_flag = false;
        msg_tx_packt->credit_limit_cnt = 0;
    }

    ret = nh_msg_ctrl_send(priv->msg_ctrl, frame_elem, tofront);

    if (ret < 0) {
        EXAMPLE_ERR("Failed to send DEVICE_READY message\r\n");
        frame_queue_free_elem(frame_elem);
        return -1;
    }

    return 0;
}

/**
 * @brief WiFi daemon thread
 * @details Main tasks:
 *   1. Process received RX frames and send to host
 *   2. Process TX frames to be sent via WiFi
 *   3. Monitor WiFi connection state changes
 * @param[in] arg Pointer to WiFi private structure
 */
static void wifi_proc_task(void *arg)
{
    int ret;
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)arg;

    frame_elem_t *frame_elem;
    struct nh_wifi_msg_packt *msg_packt;

    uint32_t wifi_update_ms = 0;
    bool msg_dnld_recv_suspend = false;

    EXAMPLE_INFO("WiFi daemon thread started\r\n");

    /* Initial state */
    priv->wifi_status = nh_WIFI_DSTA_IDLE;
    priv->wifi_connected = true;// default link

    while (!priv->stop_requested) {
        /* Wait */
        if (msg_dnld_recv_suspend) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        }

        /* get msg recv frame*/
        ret = frame_queue_receive(priv->msg_dnld_queue, &frame_elem, 0);
        if (ret < 0) {
            msg_dnld_recv_suspend = true;
            frame_elem = NULL;
            msg_packt = NULL;
        } else {
            msg_dnld_recv_suspend = false;
            msg_packt = (struct nh_wifi_msg_packt *)frame_elem->buff_addr;
            #if DEBUG_DUMP_WIFITX_ENABLE
            EXAMPLE_PRINTF("recv dnld frame_elem:%p, buf:%p, ret:%d, queue_len:%d\n", frame_elem, frame_elem->buff_addr, ret, frame_queue_messages_waiting(priv->msg_dnld_queue));
            #endif
        }

        /* Process HOST_RESET/STOP in any state */
        if (msg_packt && (msg_packt->flag == nh_WIFI_FLAG_HOST_RESET ||
                            msg_packt->flag == nh_WIFI_FLAG_HOST_STOP)) {
            LOG_W("Received host %s command\r\n", msg_packt->flag == nh_WIFI_FLAG_HOST_RESET ? "RESET" : "STOP");

            priv->wifi_status = nh_WIFI_DSTA_IDLE;

            frame_queue_free_elem(frame_elem);
            nh_credit_reset(&priv->flow_ctrl);
            msg_packt = NULL;
            continue;
        }

        /* State machine */
        switch (priv->wifi_status) {
            case nh_WIFI_DSTA_IDLE:
                /* Wait for HOST ready */
                if (msg_packt == NULL || msg_packt->flag != nh_WIFI_FLAG_HOST_READY) {
                    break;
                }
                EXAMPLE_INFO("Received HOST_READY message\r\n");

            case nh_WIFI_DSTA_HOST_READY:
                /* Host ready, send DEVICE_READY */

                if (msg_packt) {
                    nh_credit_init(&priv->flow_ctrl, "ETH",
                               msg_packt->credit_limit_cnt,
                               WIFI_DNLD_CREDIT_UPDATE_THRESHOLD,
                               WIFI_DNLD_CREDIT_MAX,
                               WIFI_DNLD_CREDIT_UPDATE_THRESHOLD);
                }

                LOG_W("Host ready (credit=%d), sending DEVICE_READY\r\n",
                      priv->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt);
                ret = msg_upld_send_cmd_packet(priv, nh_WIFI_FLAG_DEVICE_READY, 0, -1);
                if (ret < 0) {
                    EXAMPLE_ERR("Failed to send DEVICE_READY message\r\n");
                    break;
                }

                priv->wifi_status = nh_WIFI_DSTA_DEVICE_READY;
                break;

            case nh_WIFI_DSTA_DEVICE_READY:
                /* Host ready, send DEVICE_READY after initialization */
                if (priv->wifi_connected == false) {
                    break;
                }

                EXAMPLE_INFO("WiFi connected, sending DEVICE_START\r\n");

                ret = msg_upld_send_cmd_packet(priv, nh_WIFI_FLAG_DEVICE_START, 0, WIFI_DNLD_CREDIT_MAX);
                if (ret < 0) {
                    EXAMPLE_ERR("Failed to send DEVICE_START message\r\n");
                    break;
                }

                nh_msg_ctrl_reset_stream_stat(priv->msg_ctrl, priv->msg_tag);
                priv->wifi_status = nh_WIFI_DSTA_DEVICE_RUN;
                nh_credit_enable(&priv->flow_ctrl, NH_CREDIT_EN_RX | NH_CREDIT_EN_TX);
                break;

            case nh_WIFI_DSTA_DEVICE_RUN:
                //EXAMPLE_INFO("Run here, nh_WIFI_DSTA_DEVICE_RUN %d\r\n", priv->wifi_connected);
                if (msg_packt && msg_packt->credit_update_flag) {
                    nh_tx_credit_update(&priv->flow_ctrl, msg_packt->credit_limit_cnt);
                }
                if (priv->wifi_connected == true) {
                    break;
                }
                EXAMPLE_INFO("WiFi disconnected, sending DEVICE_STOP\r\n");

                ret = msg_upld_send_cmd_packet(priv, nh_WIFI_FLAG_DEVICE_STOP, 0, -1);
                if (ret < 0) {
                    EXAMPLE_ERR("Failed to send DEVICE_STOP message\r\n");
                    break;
                }

                priv->wifi_status = nh_WIFI_DSTA_IDLE;
                break;

            default:
                break;
        }

        /* Send received msg WiFi packets via WiFi */
        if (msg_packt) {
            if (priv->wifi_status == nh_WIFI_DSTA_DEVICE_RUN && msg_packt->flag == nh_WIFI_FLAG_DNLD_DATA) {
                /* Process download data */
                ret = wifi_send_frame(priv, frame_elem);
                if (ret < 0) {
                    EXAMPLE_ERR("Failed to send frame via WiFi: %d\r\n", ret);
                    frame_queue_free_elem(frame_elem);
                }
                /* Statistics: DNLD producer generates packets */
                g_nethub_stats.dnld_producer_count++;
            } else {
                frame_queue_free_elem(frame_elem);
            }
        }

        /* Actively send flow control update, needed when no uplink data */
        if (priv->wifi_status == nh_WIFI_DSTA_DEVICE_RUN &&
            nh_rx_credit_need_update(&priv->flow_ctrl)) {
            ret = nh_rx_credit_update(&priv->flow_ctrl);
            if (ret >= 0) {
                ret = msg_upld_send_cmd_packet(priv, nh_WIFI_FLAG_CREDIT_UPDATE, 1, ret);
            }
            if (ret < 0) {
                EXAMPLE_ERR("Failed to send CREDIT_UPDATE message\r\n");
            }
        }
    }

    EXAMPLE_INFO("WiFi daemon thread exiting\r\n");
    vTaskDelete(NULL);
}

int app_customfree3(frame_elem_t *frame_elem)
{
    if (frame_elem->skb_free) {
#if DEBUG_DUMP_WIFIRX_ENABLE
        EXAMPLE_INFO("upld done ================== %s   cb=%p, p=%p\r\n",
            __FUNCTION__, frame_elem->skb_free,frame_elem->skb);
#endif
        /* Statistics: UPLD SDIO transfer complete */
        g_nethub_stats.upld_transfer_complete++;

        frame_elem->skb_free(frame_elem->skb);
    }
    return 0;
}

/**
 * @brief Frame free task - safely free frame elements outside interrupt context
 * @param[in] arg Pointer to WiFi private structure
 */
static void frame_free_task(void *arg)
{
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)arg;
    free_msg_t free_msg;

    EXAMPLE_INFO("Frame free task started\r\n");

    while (!priv->stop_requested) {
        /* Wait for free message */
        if (xQueueReceive(priv->frame_free_queue, &free_msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (free_msg.frame_elem) {
                /* Safely free frame element */
                // frame_queue_free_elem(free_msg.frame_elem);
                app_customfree3(free_msg.frame_elem);
            }
        }
    }

    EXAMPLE_INFO("Frame free task exiting\r\n");
    vTaskDelete(NULL);
}
// extern int platform_get_mac(uint8_t *mac);

/*****************************************************************************
 * Public API Implementation
 *****************************************************************************/

/**
 * @brief Initialize WiFi device
 * @param[in] msg_ctrl Message control interface
 * @param[in] msg_tag Message tag for routing
 * @retval 0 Success
 * @retval <0 Error code
 */
int transportsdio_init(wifi_wifista_priv_t *ctx, nh_msg_ctrl_t *msg_ctrl, uint8_t msg_tag)
{
    int ret;
    struct wifi_wifista_priv *priv = (struct wifi_wifista_priv *)ctx;

    /* Basic field initialization */
    priv->msg_ctrl = msg_ctrl;
    priv->msg_tag = msg_tag;
    priv->wifi_status = nh_WIFI_DSTA_IDLE;
    priv->stop_requested = false;
    priv->wifi_connected = true;
    priv->wifi_send_frame = NULL;
    priv->wifi_recv_frame = NULL;

    /* MAC address is set by user or obtained through other mechanisms */
    EXAMPLE_INFO("Using MAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
            priv->mac_addr[0], priv->mac_addr[1], priv->mac_addr[2],
            priv->mac_addr[3], priv->mac_addr[4], priv->mac_addr[5]);
    /* Initialize private memory pool */
    frame_queue_ctrl_init_cfg_t rx_cfg = {
        .frame_buff = wifi_frame_buff,
        .frame_type = 0x02,
        .frame_elem_cnt = WIFI_FRAME_CNT,
        .frame_elem_size = FRAME_BUFF_TOTALSIZE,
        .name = "wifi_rx"
    };
    priv->rx_frame_ctrl = frame_queue_create(&rx_cfg);
    if (!priv->rx_frame_ctrl) {
        EXAMPLE_ERR("Failed to create RX frame buffer controller\r\n");
        return -1;
    }
    /* Register the callback for freeing frame_elem after processing */
    frame_queue_free_cb_register(priv->rx_frame_ctrl,
                                 NULL, NULL,
                                 wifi_free_after_cb, priv);

    /* Create msg dnld queue */
    priv->msg_dnld_queue = xQueueCreate(WIFI_MSG_QUEUE_DEPTH, sizeof(frame_elem_t *));

    /* Create frame free queue */
    priv->frame_free_queue = xQueueCreate(WIFI_MSG_QUEUE_DEPTH, sizeof(free_msg_t));

    /* Register MSG message callback */
    ret = nh_msg_cb_register(msg_ctrl, msg_tag,
        msg_dnld_recv_done_cb, priv,
        msg_upld_send_done_cb, priv,
        transportsdio_hwreset, priv);
    if (ret < 0) {
        EXAMPLE_ERR("Failed to register message callbacks: %d\r\n", ret);
        return -1;
    }

    /* Create daemon thread */
    ret = xTaskCreate(wifi_proc_task, "wifi_proc", 2048, priv, 10, &priv->wifi_proc_task);
    if (ret != pdPASS) {
        EXAMPLE_ERR("Failed to create daemon thread: %d\r\n", ret);
        return -1;
    }

    /* Create frame free task */
    ret = xTaskCreate(frame_free_task, "frame_free", 1024, priv, 9, &priv->frame_free_task);
    if (ret != pdPASS) {
        EXAMPLE_ERR("Failed to create frame free task: %d\r\n", ret);
        return -1;
    }

    /* Save global pointer */
    g_wifi_priv = priv;

    EXAMPLE_INFO("WiFi device initialized successfully, MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
          priv->mac_addr[0], priv->mac_addr[1], priv->mac_addr[2],
          priv->mac_addr[3], priv->mac_addr[4], priv->mac_addr[5]);

    return 0;
}

/**
 * @brief Set MAC address
 * @param[in] mac_addr MAC address (6 bytes)
 * @retval 0 Success
 * @retval -1 Parameter error or device not initialized
 */
int transportsdio_set_mac_addr(const uint8_t mac_addr[6])
{
    if (!g_wifi_priv || !mac_addr) {
        return -1;
    }

    memcpy(g_wifi_priv->mac_addr, mac_addr, 6);
    return 0;
}

/**
 * @brief Get MAC address
 * @param[out] mac_addr Output MAC address buffer (6 bytes)
 * @retval 0 Success
 * @retval -1 Parameter error or device not initialized
 */
int transportsdio_get_mac_addr(uint8_t mac_addr[6])
{
    if (!g_wifi_priv || !mac_addr) {
        return -1;
    }

    memcpy(mac_addr, g_wifi_priv->mac_addr, 6);
    return 0;
}

/**
 * @brief Deinitialize WiFi device
 * @retval 0 Success
 * @retval <0 Error code
 */
int transportsdio_deinit(void)
{
    if (g_wifi_priv) {
        g_wifi_priv->stop_requested = true;

        /* Wait for tasks to end */
        while (eTaskGetState(g_wifi_priv->wifi_proc_task) != eDeleted ||
               eTaskGetState(g_wifi_priv->frame_free_task) != eDeleted) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        /* Clean up resources */
        vQueueDelete(g_wifi_priv->msg_dnld_queue);
        vQueueDelete(g_wifi_priv->frame_free_queue);
        frame_queue_remove(g_wifi_priv->rx_frame_ctrl);
        nh_credit_deinit(&g_wifi_priv->flow_ctrl);

        free(g_wifi_priv);
        g_wifi_priv = NULL;
    }
    return 0;
}

/**
 * @brief Start WiFi device
 * @retval 0 Success
 * @retval <0 Error code
 */
int transportsdio_start(void)
{
    if (g_wifi_priv) {
        EXAMPLE_INFO("WiFi device started\r\n");
        return 0;
    }
    return -1;
}

/**
 * @brief Stop WiFi device
 * @retval 0 Success
 * @retval <0 Error code
 */
int transportsdio_stop(void)
{
    if (g_wifi_priv) {
        EXAMPLE_INFO("WiFi device stopped\r\n");
        return 0;
    }
    return -1;
}

/**
 * @brief Set WiFi send frame callback function
 * @param[in] send_func WiFi send frame function pointer
 */
void transportsdio_set_wifi_send_cb(int (*send_func)(struct wifi_wifista_priv *, void *, uint32_t, void (*free_cb)(void *arg), void *cb_arg))
{
    if (g_wifi_priv) {
        g_wifi_priv->wifi_send_frame = send_func;
    }
}

/**
 * @brief Set WiFi receive frame callback function
 * @param[in] recv_func WiFi receive frame function pointer
 */
void transportsdio_set_wifi_recv_cb(int (*recv_func)(struct wifi_wifista_priv *, void *, uint32_t))
{
    if (g_wifi_priv) {
        g_wifi_priv->wifi_recv_frame = recv_func;
    }
}

/**
 * @brief Initialize payload offset whitelist
 */
void transportsdio_init_payload_offset(void)
{
    memset(offset_whitelist, 0, sizeof(offset_whitelist));
}

/**
 * @brief Register payload address and corresponding offset to whitelist
 * @param[in] payload payload address
 * @param[in] offset corresponding offset value
 * @retval 0 Success
 * @retval -1 Parameter error
 * @retval -2 Whitelist full
 */
int transportsdio_register_payload_offset(uint8_t *payload, uint32_t offset)
{
    int i;

    EXAMPLE_DEBUG("%s, pld:%p, offset:%d\r\n", __func__, payload, offset);
    if (!payload) {
        return -1;
    }

    /* Check if payload already exists, if so update offset */
    for (i = 0; i < FHOST_RX_BUF_CNT; i++) {
        if (offset_whitelist[i].payload_addr == payload) {
            offset_whitelist[i].offset = offset;
            return 0;
        }
    }

    /* Find free slot */
    for (i = 0; i < FHOST_RX_BUF_CNT; i++) {
        if (offset_whitelist[i].payload_addr == NULL) {
            offset_whitelist[i].payload_addr = payload;
            offset_whitelist[i].offset = offset;
            return 0;
        }
    }

    EXAMPLE_ERR("%s:%d Whitelist full %p.\r\n", __func__, __LINE__, payload);
    return -2; /* Whitelist full */
}

/**
 * @brief Unregister payload address from whitelist
 * @param[in] payload payload address to remove
 * @retval 0 Success
 * @retval -1 Parameter error
 * @retval -2 Payload not found
 */
int transportsdio_unregister_payload_offset(uint8_t *payload)
{
    int i;

    if (!payload) {
        return -1;
    }

    /* Find payload and set to NULL */
    for (i = 0; i < FHOST_RX_BUF_CNT; i++) {
        if (offset_whitelist[i].payload_addr == payload) {
            offset_whitelist[i].payload_addr = NULL;
            offset_whitelist[i].offset = 0;
            return 0;
        }
    }

    return -2; /* Payload not found */
}

/**
 * @brief Get corresponding offset by payload address
 * @param[in] payload payload address
 * @return offset value, return FRAME_BUFF_PAYLOAD_OFFSET if not found
 */
uint32_t transportsdio_get_payload_offset(uint8_t *payload)
{
    int i;

    if (!payload) {
        EXAMPLE_ERR("%s:%d payload err %p.\r\n", __func__, __LINE__, payload);
        return FRAME_BUFF_PAYLOAD_OFFSET;
    }

    /* Linear search for matching payload address */
    for (i = 0; i < FHOST_RX_BUF_CNT; i++) {
        if (offset_whitelist[i].payload_addr == payload) {
            EXAMPLE_DEBUG("%s, pld:%p, offset:%d\r\n", __func__, payload, offset_whitelist[i].offset);
            return offset_whitelist[i].offset;
        }
    }

    EXAMPLE_DEBUG("%s, pld:%p, offset:%d\r\n", __func__, payload, FRAME_BUFF_PAYLOAD_OFFSET);
    /* Return default value if not found */
    return FRAME_BUFF_PAYLOAD_OFFSET;
}

/**
 * @brief User call interface: send WiFi data to host (upld)
 * @param[in] data Data buffer to send
 * @param[in] len Data length
 * @retval 0 Success
 * @retval -1 Parameter error
 * @retval -2 Device not initialized
 * @retval -3 Memory allocation failed
 * @retval -4 WiFi not connected
 */
int transportsdio_send_upld_skb(nh_skb_t *skb)
{
    frame_elem_t *frame_elem;
    struct nh_wifi_msg_packt *msg_tx_packt;
    int ret;
    uint8_t *payload = (uint8_t *)skb->data;

    EXAMPLE_DEBUG("%s payload:%p\r\n", __func__, payload);
    if (!g_wifi_priv) {
        EXAMPLE_ERR("WiFi device not initialized.\r\n", __func__, __LINE__);
        return -2;
    }

    if (!g_wifi_priv->wifi_connected) {
        EXAMPLE_ERR("WiFi not connected.\r\n", __func__, __LINE__);
        return -4;
    }

    ret = nh_tx_credit_get(&g_wifi_priv->flow_ctrl, portMAX_DELAY);
    if (ret < 0) {
        EXAMPLE_ERR("Failed to get upld credit.\r\n");
        return -4;
    }

    /* Assign based on head */
    transportsdio_register_payload_offset(payload, FRAME_BUFF_ELEM_SIZE + sizeof(struct nh_wifi_msg_packt));
    frame_elem   = (frame_elem_t *)(payload - FRAME_BUFF_ELEM_SIZE - sizeof(struct nh_wifi_msg_packt));
    msg_tx_packt = (struct nh_wifi_msg_packt *)(payload - sizeof(struct nh_wifi_msg_packt));
    frame_elem->buff_addr = (void *)msg_tx_packt;

    /* Set callback free */
    frame_elem->skb_free = (skb_free_cb_t)skb->free_cb;
    frame_elem->skb      = skb->cb_arg;

    /* Statistics: UPLD starts to be transferred to SDIO module */
    g_nethub_stats.upld_start_consume++;
#if DEBUG_DUMP_WIFIRX_ENABLE
    EXAMPLE_INFO("upld start ================== p:%p, elem:%p, elem->buff_addr:%p, pld:%p, cb=%p\r\n",
            frame_elem->skb,
            frame_elem,
            frame_elem->buff_addr,
            payload,
            frame_elem->skb_free
            );
#endif
    /* Copy user data */
    // memcpy(msg_tx_packt->data, skb->data, skb->len);
    frame_elem->data_size = sizeof(struct nh_wifi_msg_packt) + skb->len;
    frame_elem->buff_size = WIFI_RX_BUFF_SIZE;

    /* Set message header */
    msg_tx_packt->msg_packt.len = skb->len + sizeof(struct nh_wifi_msg_packt) - sizeof(nh_msg_packt_t);
    msg_tx_packt->msg_packt.tag = g_wifi_priv->msg_tag;
    msg_tx_packt->msg_packt.sub_tag = 0;
    msg_tx_packt->flag = nh_WIFI_FLAG_UPLD_DATA;

    /* Set flow control information */
    ret = nh_rx_credit_update(&g_wifi_priv->flow_ctrl);
    msg_tx_packt->credit_limit_cnt = (ret >= 0) ? (uint8_t)ret : 0;
    msg_tx_packt->credit_update_flag = (ret >= 0) ? true : false;

#if 1//ETHSTA_DBGLOG
    if (msg_tx_packt->msg_packt.len >= 14) {
        static int s_upld_cnt = 0;
        s_upld_cnt++;
        #if DEBUG_DUMP_WIFIRX_ENABLE
        EXAMPLE_INFO("Upld[%d] tag[%d] upld_cnt:<%d> <-----  %02X:%02X:%02X:%02X:%02X:%02X  %02X:%02X:%02X:%02X:%02X:%02X   %02X:%02X .... %02X:%02X\r\n",
            msg_tx_packt->msg_packt.len, msg_tx_packt->msg_packt.tag, s_upld_cnt,
            msg_tx_packt->data[0], msg_tx_packt->data[1], msg_tx_packt->data[2], msg_tx_packt->data[3],
            msg_tx_packt->data[4], msg_tx_packt->data[5], msg_tx_packt->data[6], msg_tx_packt->data[7],
            msg_tx_packt->data[8], msg_tx_packt->data[9], msg_tx_packt->data[10], msg_tx_packt->data[11],
            msg_tx_packt->data[12], msg_tx_packt->data[13],
            msg_tx_packt->data[msg_tx_packt->msg_packt.len - 2], msg_tx_packt->data[msg_tx_packt->msg_packt.len - 1]);
        #endif
    }
#endif
    /* Send data to host */
    //EXAMPLE_DEBUG("g_wifi_priv->msg_ctrl = %p\r\n", g_wifi_priv->msg_ctrl);

    ret = nh_msg_ctrl_send(g_wifi_priv->msg_ctrl, frame_elem, 0);
    if (ret < 0) {
        nh_tx_credit_put(&g_wifi_priv->flow_ctrl);
        EXAMPLE_ERR("\x1b[33m%s:%d\x1b[0m Failed to send upld data, ret:%d.\r\n", __func__, __LINE__, ret);
        msg_upld_send_done_customfree(frame_elem);
        return ret;
    }

    //EXAMPLE_INFO("Upld data sent: len=%lu\r\n", len);
    return 0;
}

int transportsdio_send_upld(const void *data, uint32_t len)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_wifi_msg_packt *msg_tx_packt;

    if (!data || len == 0 || len > WIFI_RX_BUFF_SIZE) {
        EXAMPLE_ERR("Invalid parameters: data=%p, len=%lu\r\n", data, len);
        return -1;
    }

    if (!g_wifi_priv) {
        EXAMPLE_ERR("WiFi device not initialized\r\n");
        return -2;
    }

    if (!g_wifi_priv->wifi_connected) {
        EXAMPLE_ERR("WiFi not connected\r\n");
        return -4;
    }

    ret = nh_tx_credit_get(&g_wifi_priv->flow_ctrl, portMAX_DELAY);
    if (ret < 0) {
        EXAMPLE_ERR("Failed to get upld credit\r\n");
        return -4;
    }

    ret = frame_queue_alloc_elem(g_wifi_priv->rx_frame_ctrl, &frame_elem, 1000);
    if (ret < 0) {
        nh_tx_credit_put(&g_wifi_priv->flow_ctrl);
        EXAMPLE_ERR("Failed to allocate frame element\r\n");
        return -3;
    }

    msg_tx_packt = (struct nh_wifi_msg_packt *)frame_elem->buff_addr;

    frame_elem->skb_free = NULL;
    frame_elem->skb = NULL;

    /* Copy user data */
    memcpy(msg_tx_packt->data, data, len);
    frame_elem->data_size = sizeof(struct nh_wifi_msg_packt) + len;

    /* Set message header */
    msg_tx_packt->msg_packt.len = len + sizeof(struct nh_wifi_msg_packt) - sizeof(nh_msg_packt_t);
    msg_tx_packt->msg_packt.tag = g_wifi_priv->msg_tag;
    msg_tx_packt->msg_packt.sub_tag = 0;
    msg_tx_packt->flag = nh_WIFI_FLAG_UPLD_DATA;

    /* Set flow control information */
    ret = nh_rx_credit_update(&g_wifi_priv->flow_ctrl);
    msg_tx_packt->credit_limit_cnt = (ret >= 0) ? (uint8_t)ret : 0;
    msg_tx_packt->credit_update_flag = (ret >= 0) ? true : false;

#if 0//ETHSTA_DBGLOG
    if (msg_tx_packt->msg_packt.len >= 14) {
        EXAMPLE_PRINTF("Upld[%d] <-----  %02X:%02X:%02X:%02X:%02X:%02X  %02X:%02X:%02X:%02X:%02X:%02X   %02X:%02X .... %02X:%02X\r\n",
            msg_tx_packt->msg_packt.len,
            msg_tx_packt->data[0], msg_tx_packt->data[1], msg_tx_packt->data[2], msg_tx_packt->data[3],
            msg_tx_packt->data[4], msg_tx_packt->data[5], msg_tx_packt->data[6], msg_tx_packt->data[7],
            msg_tx_packt->data[8], msg_tx_packt->data[9], msg_tx_packt->data[10], msg_tx_packt->data[11],
            msg_tx_packt->data[12], msg_tx_packt->data[13],
            msg_tx_packt->data[msg_tx_packt->msg_packt.len - 2], msg_tx_packt->data[msg_tx_packt->msg_packt.len - 1]);
    }
#endif
    /* Send data to host */
    ret = nh_msg_ctrl_send(g_wifi_priv->msg_ctrl, frame_elem, 0);
    if (ret < 0) {
        nh_tx_credit_put(&g_wifi_priv->flow_ctrl);
        EXAMPLE_ERR("Failed to send upld data: %d\r\n", ret);
        frame_queue_free_elem(frame_elem);
        return ret;
    }

    //EXAMPLE_INFO("Upld data sent: len=%lu\r\n", len);
    return 0;
}

#ifdef CONFIG_NETHUB_DEBUG
static void transportsdio_dump_queue_state(const char *name, QueueHandle_t queue, UBaseType_t depth)
{
    UBaseType_t used;
    UBaseType_t free_cnt;

    if (queue == NULL) {
        EXAMPLE_PRINTF("  %s: NULL\r\n", name);
        return;
    }

    used = uxQueueMessagesWaiting(queue);
    free_cnt = uxQueueSpacesAvailable(queue);
    if (depth == 0U) {
        depth = used + free_cnt;
    }

    EXAMPLE_PRINTF("  %s: %p (used: %lu/%lu, free: %lu)\r\n",
                   name,
                   queue,
                   (unsigned long)used,
                   (unsigned long)depth,
                   (unsigned long)free_cnt);
}

static void transportsdio_dump_frame_ctrl_state(const char *name, frame_queue_ctrl_t *ctrl)
{
    UBaseType_t pool_free = 0;
    UBaseType_t output_wait = 0;
    UBaseType_t input_wait = 0;
    UBaseType_t inflight = 0;

    if (ctrl == NULL) {
        EXAMPLE_PRINTF("  %s: NULL\r\n", name);
        return;
    }

    if (ctrl->pool_queue != NULL) {
        pool_free = uxQueueMessagesWaiting(ctrl->pool_queue);
        inflight = (ctrl->frame_elem_cnt >= pool_free) ? (ctrl->frame_elem_cnt - pool_free) : 0;
    }
    if (ctrl->output_queue != NULL) {
        output_wait = uxQueueMessagesWaiting(ctrl->output_queue);
    }
    if (ctrl->input_queue != NULL) {
        input_wait = uxQueueMessagesWaiting(ctrl->input_queue);
    }

    EXAMPLE_PRINTF("  %s: %p (name: %s, type: 0x%04X, elem: %u)\r\n",
                   name,
                   ctrl,
                   ctrl->name ? ctrl->name : "?",
                   ctrl->frame_type,
                   ctrl->frame_elem_cnt);
    EXAMPLE_PRINTF("    pool_queue: %p (free: %lu/%u, inflight: %lu)\r\n",
                   ctrl->pool_queue,
                   (unsigned long)pool_free,
                   ctrl->frame_elem_cnt,
                   (unsigned long)inflight);
    EXAMPLE_PRINTF("    output_queue: %p (wait: %lu)\r\n",
                   ctrl->output_queue,
                   (unsigned long)output_wait);
    EXAMPLE_PRINTF("    input_queue: %p (wait: %lu)\r\n",
                   ctrl->input_queue,
                   (unsigned long)input_wait);
}

static void transportsdio_dump_credit_state(const char *name, const nh_credit_t *credit, bool enabled, bool is_tx)
{
    uint8_t credit_delta;
    UBaseType_t sem_cnt = 0;

    if (credit == NULL) {
        EXAMPLE_PRINTF("  %s: NULL\r\n", name);
        return;
    }

    if (credit->wait_sem != NULL) {
        sem_cnt = uxQueueMessagesWaiting((QueueHandle_t)credit->wait_sem);
    }

    credit_delta = is_tx
        ? (uint8_t)(credit->credit_limit_cnt - credit->credit_consumed_cnt)
        : (uint8_t)(credit->credit_consumed_cnt - credit->credit_limit_cnt);

    EXAMPLE_PRINTF("  %s: %s\r\n", name, enabled ? "enabled" : "disabled");
    EXAMPLE_PRINTF("    init_credit: %u, threshold: %u, credit_limit_cnt: %u, credit_consumed_cnt: %u\r\n",
                   credit->init_credit,
                   credit->threshold,
                   credit->credit_limit_cnt,
                   credit->credit_consumed_cnt);
    EXAMPLE_PRINTF("    wait_sem: %p, wait_sem_cnt: %lu, %s: %u\r\n",
                   credit->wait_sem,
                   (unsigned long)sem_cnt,
                   is_tx ? "credit_left" : "pending_update",
                   credit_delta);
}

static const char *transportsdio_msg_tag_name(uint8_t tag)
{
    switch (tag) {
        case nh_MSG_TAG_SYS:
            return "SYS";
        case nh_MSG_TAG_WIFI:
            return "WIFI";
        case nh_MSG_TAG_ETH:
            return "ETH";
        case nh_MSG_TAG_TTY:
            return "TTY";
        case nh_MSG_TAG_NETLINK:
            return "NETLINK";
        case nh_MSG_TAG_HCI:
            return "HCI";
        default:
            return "?";
    }
}

static void transportsdio_dump_stream_packets(nh_msg_ctrl_t *msg_ctrl)
{
    int i;

    if (msg_ctrl == NULL) {
        EXAMPLE_PRINTF("Stream Packets:\r\n");
        EXAMPLE_PRINTF("  msg_ctrl: N/A\r\n");
        return;
    }

    EXAMPLE_PRINTF("Stream Packets:\r\n");
    for (i = 0; i < nh_MSG_TAG_MAX; i++) {
        const nh_msg_stream_stat_t *stat = &msg_ctrl->stream_stats[i];
        bool active = (msg_ctrl->nh_msg_dnld_recv_cb[i] != NULL) ||
                      (msg_ctrl->nh_msg_upld_send_cb[i] != NULL) ||
                      (msg_ctrl->nh_msg_hw_reset_cb[i] != NULL) ||
                      (stat->tx_data_packets != 0U) ||
                      (stat->tx_ctrl_packets != 0U) ||
                      (stat->rx_data_packets != 0U) ||
                      (stat->rx_ctrl_packets != 0U);

        if (!active) {
            continue;
        }

        EXAMPLE_PRINTF("tag %d (%s):\r\n",
                       i,
                       transportsdio_msg_tag_name(i));
        EXAMPLE_PRINTF("       data pkt tx=%lu rx=%lu, bytes tx=%lu rx=%lu\r\n",
                       (unsigned long)stat->tx_data_packets,
                       (unsigned long)stat->rx_data_packets,
                       (unsigned long)stat->tx_data_bytes,
                       (unsigned long)stat->rx_data_bytes);
        EXAMPLE_PRINTF("       ctrl pkt tx=%lu rx=%lu, bytes tx=%lu rx=%lu\r\n",
                       (unsigned long)stat->tx_ctrl_packets,
                       (unsigned long)stat->rx_ctrl_packets,
                       (unsigned long)stat->tx_ctrl_bytes,
                       (unsigned long)stat->rx_ctrl_bytes);
    }
}

void transportsdio_reset_stats(void)
{
    if (g_wifi_priv == NULL) {
        return;
    }

    if (g_wifi_priv->msg_ctrl != NULL) {
        nh_msg_ctrl_reset_stream_stats(g_wifi_priv->msg_ctrl);
    }

    memset(&g_nethub_stats, 0, sizeof(g_nethub_stats));
}

/**
 * @brief Dump g_wifi_priv structure parameters for debugging
 * @details Print important parameters including queue status, connection state,
 *          flow control status and other key information
 */
void transportsdio_dump_priv(void)
{
    wifi_wifista_priv_t *priv = g_wifi_priv;
    nh_msg_ctrl_t *msg_ctrl;
    nh_msg_ctrl_ops_t *sdio_ops;
    UBaseType_t wifi_upld_inflight = 0;
    UBaseType_t msg_dnld_pool_inflight = 0;
    int upld_sdio_pending = 0;
    int upld_free_pending = 0;
    int dnld_pending = 0;
    int sdio_ready = -1;
    int sdio_upld_busy = -1;
    int sdio_dnld_busy = -1;

    if (!g_wifi_priv) {
        EXAMPLE_PRINTF("g_wifi_priv is NULL - device not initialized\r\n");
        return;
    }

    msg_ctrl = priv->msg_ctrl;
    sdio_ops = (msg_ctrl != NULL) ? msg_ctrl->msg_sdio_ops : NULL;

    if (priv->rx_frame_ctrl != NULL && priv->rx_frame_ctrl->pool_queue != NULL &&
        priv->rx_frame_ctrl->frame_elem_cnt >= uxQueueMessagesWaiting(priv->rx_frame_ctrl->pool_queue)) {
        wifi_upld_inflight = priv->rx_frame_ctrl->frame_elem_cnt -
            uxQueueMessagesWaiting(priv->rx_frame_ctrl->pool_queue);
    }

    if (msg_ctrl != NULL && msg_ctrl->dnld_queue_ctrl != NULL &&
        msg_ctrl->dnld_queue_ctrl->pool_queue != NULL &&
        msg_ctrl->dnld_queue_ctrl->frame_elem_cnt >= uxQueueMessagesWaiting(msg_ctrl->dnld_queue_ctrl->pool_queue)) {
        msg_dnld_pool_inflight = msg_ctrl->dnld_queue_ctrl->frame_elem_cnt -
            uxQueueMessagesWaiting(msg_ctrl->dnld_queue_ctrl->pool_queue);
    }

    upld_sdio_pending = (int)g_nethub_stats.upld_start_consume - (int)g_nethub_stats.upld_transfer_complete;
    if (upld_sdio_pending < 0) {
        upld_sdio_pending = 0;
    }

    upld_free_pending = (int)g_nethub_stats.upld_transfer_complete - (int)g_nethub_stats.upld_free_count;
    if (upld_free_pending < 0) {
        upld_free_pending = 0;
    }

    dnld_pending = (int)g_nethub_stats.dnld_producer_count -
        (int)(g_nethub_stats.dnld_consumer_free_normal + g_nethub_stats.dnld_consumer_free_abnormal);
    if (dnld_pending < 0) {
        dnld_pending = 0;
    }

    if (sdio_ops != NULL) {
        if (sdio_ops->sdio_is_ready != NULL) {
            sdio_ready = sdio_ops->sdio_is_ready();
        }
        if (sdio_ops->sdio_upld_is_busy != NULL) {
            sdio_upld_busy = sdio_ops->sdio_upld_is_busy();
        }
        if (sdio_ops->sdio_dnld_is_busy != NULL) {
            sdio_dnld_busy = sdio_ops->sdio_dnld_is_busy();
        }
    }

    EXAMPLE_PRINTF("=== WiFi Private Structure Dump ===\r\n");

    /* Basic information */
    EXAMPLE_PRINTF("Basic Info:\r\n");
    EXAMPLE_PRINTF("  msg_ctrl: %p\r\n", msg_ctrl);
    EXAMPLE_PRINTF("  msg_tag: %d\r\n", priv->msg_tag);
    EXAMPLE_PRINTF("  wifi_status: %d\r\n", priv->wifi_status);
    EXAMPLE_PRINTF("  stop_requested: %s\r\n", priv->stop_requested ? "true" : "false");

    /* MAC address and connection status */
    EXAMPLE_PRINTF("MAC & Connection:\r\n");
    EXAMPLE_PRINTF("  mac_addr: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   priv->mac_addr[0], priv->mac_addr[1], priv->mac_addr[2],
                   priv->mac_addr[3], priv->mac_addr[4], priv->mac_addr[5]);
    EXAMPLE_PRINTF("  wifi_connected: %s\r\n", priv->wifi_connected ? "connected" : "disconnected");

    /* Flow control information */
    EXAMPLE_PRINTF("Flow Control:\r\n");
    EXAMPLE_PRINTF("  tag: %s, enable_mask: 0x%02X\r\n",
                   priv->flow_ctrl.tag ? priv->flow_ctrl.tag : "?",
                   priv->flow_ctrl.enable);
    transportsdio_dump_credit_state("tx_credit", &priv->flow_ctrl.credit[NH_CREDIT_DIR_TX],
                                    (priv->flow_ctrl.enable & NH_CREDIT_EN_TX) != 0U, true);
    transportsdio_dump_credit_state("rx_credit", &priv->flow_ctrl.credit[NH_CREDIT_DIR_RX],
                                    (priv->flow_ctrl.enable & NH_CREDIT_EN_RX) != 0U, false);
    transportsdio_dump_stream_packets(msg_ctrl);

    EXAMPLE_PRINTF("Flow Summary:\r\n");
    EXAMPLE_PRINTF("  wifi_upld_pool_inflight: %lu/%u\r\n",
                   (unsigned long)wifi_upld_inflight,
                   priv->rx_frame_ctrl ? priv->rx_frame_ctrl->frame_elem_cnt : 0U);
    EXAMPLE_PRINTF("  msg_dnld_pool_inflight: %lu/%u\r\n",
                   (unsigned long)msg_dnld_pool_inflight,
                   (msg_ctrl && msg_ctrl->dnld_queue_ctrl) ? msg_ctrl->dnld_queue_ctrl->frame_elem_cnt : 0U);
    EXAMPLE_PRINTF("  upld_sdio_pending: %d, upld_free_pending: %d, dnld_pending: %d\r\n",
                   upld_sdio_pending,
                   upld_free_pending,
                   dnld_pending);
    EXAMPLE_PRINTF("  stats: dnld_prod=%lu dnld_free_ok=%lu dnld_free_err=%lu upld_prod=%lu upld_start=%lu upld_done=%lu upld_free=%lu\r\n",
                   (unsigned long)g_nethub_stats.dnld_producer_count,
                   (unsigned long)g_nethub_stats.dnld_consumer_free_normal,
                   (unsigned long)g_nethub_stats.dnld_consumer_free_abnormal,
                   (unsigned long)g_nethub_stats.upld_producer_count,
                   (unsigned long)g_nethub_stats.upld_start_consume,
                   (unsigned long)g_nethub_stats.upld_transfer_complete,
                   (unsigned long)g_nethub_stats.upld_free_count);

    /* Task information */
    EXAMPLE_PRINTF("Tasks:\r\n");
    EXAMPLE_PRINTF("  wifi_proc_task: %p", priv->wifi_proc_task);
    if (priv->wifi_proc_task) {
        EXAMPLE_PRINTF(" (state: %d)",
              eTaskGetState(priv->wifi_proc_task));
#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
        EXAMPLE_PRINTF(", stack_high_water_mark: %u",
              uxTaskGetStackHighWaterMark(priv->wifi_proc_task));
#endif
    }
    EXAMPLE_PRINTF("\r\n");

    EXAMPLE_PRINTF("  frame_free_task: %p", priv->frame_free_task);
    if (priv->frame_free_task) {
        EXAMPLE_PRINTF(" (state: %d)",
              eTaskGetState(priv->frame_free_task));
#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
        EXAMPLE_PRINTF(", stack_high_water_mark: %u",
              uxTaskGetStackHighWaterMark(priv->frame_free_task));
#endif
    }
    EXAMPLE_PRINTF("\r\n");

    /* Queue information */
    EXAMPLE_PRINTF("Queues:\r\n");
    transportsdio_dump_queue_state("eth.msg_dnld_queue", priv->msg_dnld_queue, WIFI_MSG_QUEUE_DEPTH);
    transportsdio_dump_queue_state("eth.frame_free_queue", priv->frame_free_queue, WIFI_MSG_QUEUE_DEPTH);
    if (msg_ctrl != NULL) {
        transportsdio_dump_queue_state("msg_ctrl.upld_queue", msg_ctrl->upld_queue, nh_MSG_CTRL_UPLD_QUEUE_DEPTH);
        transportsdio_dump_queue_state("msg_ctrl.dnld_queue", msg_ctrl->dnld_queue, nh_MSG_CTRL_DNLD_FRAME_NUM);
    } else {
        EXAMPLE_PRINTF("  msg_ctrl.upld_queue: N/A\r\n");
        EXAMPLE_PRINTF("  msg_ctrl.dnld_queue: N/A\r\n");
    }

    /* Frame buffer information */
    EXAMPLE_PRINTF("Frame Buffer:\r\n");
    transportsdio_dump_frame_ctrl_state("eth.rx_frame_ctrl", priv->rx_frame_ctrl);
    if (msg_ctrl != NULL) {
        transportsdio_dump_frame_ctrl_state("msg_ctrl.dnld_queue_ctrl", msg_ctrl->dnld_queue_ctrl);
    } else {
        EXAMPLE_PRINTF("  msg_ctrl.dnld_queue_ctrl: N/A\r\n");
    }

    EXAMPLE_PRINTF("SDIO State:\r\n");
    EXAMPLE_PRINTF("  hw_reset_flag: %s\r\n",
                   (msg_ctrl != NULL && msg_ctrl->hw_reset_flag) ? "true" : "false");
    EXAMPLE_PRINTF("  sdio_ops: %p, sdio_name: %s\r\n",
                   sdio_ops,
                   (sdio_ops != NULL && sdio_ops->sdio_name != NULL) ? sdio_ops->sdio_name : "?");
    EXAMPLE_PRINTF("  sdio_is_ready: %d, sdio_upld_busy: %d, sdio_dnld_busy: %d\r\n",
                   sdio_ready,
                   sdio_upld_busy,
                   sdio_dnld_busy);

    /* Callback function information */
    EXAMPLE_PRINTF("Callbacks:\r\n");
    EXAMPLE_PRINTF("  wifi_send_frame: %p\r\n", priv->wifi_send_frame);
    EXAMPLE_PRINTF("  wifi_recv_frame: %p\r\n", priv->wifi_recv_frame);

    EXAMPLE_PRINTF("=== End of WiFi Private Structure Dump ===\r\n");
}
#endif

#ifdef CONFIG_SDIO3_SOFT_RST_INT_USER
static void _sdio3_reinit(void)
{
    extern struct nh_device_s *sdio3_hd;
    struct nh_sdio3_config_s cfg = {
        .func_num = 1,                            /*!< function num: 1~2. */
        .ocr = 0xff0000,                                           /*!< OCR, [14:0]:res, [23:15]:2.7v~3.6v */
        .cap_flag = SDIO3_CAP_FLAG_SDR50 | SDIO3_CAP_FLAG_RD_WAIT, /*!< capability flag */
        .func1_dnld_size_max = (512*3),                /* dnld max size */
        .func2_dnld_size_max = (512*3),                /* dnld max size */
    };

    /* Initialize SDIO3 */
    EXAMPLE_PRINTF("nh_sdio3_init\r\n");
    nh_sdio3_init(sdio3_hd, &cfg);

    nh_sdio3_feature_control(sdio3_hd, SDIO3_CMD_INIT_READY, 0);
    nh_sdio3_feature_control(sdio3_hd, SDIO3_CMD_SET_FUNC_CARD_READY, 1);
}
void transportsdio3_reset_handler_isr(void *arg)
{
    extern uint32_t transportsdio_get_payload_offset(uint8_t *payload);
    extern volatile bool sdio3_ready_flag;
    extern struct nh_device_s *sdio3_hd;
    nh_msg_ctrl_t *msg_ctrl;
    frame_elem_t *frame_elem;
    int offset;

    printf("SDIO hotplug detected, this feature is not yet supported\r\n");
    //while(1);

    sdio3_ready_flag = false;
    nh_msg_host_reset_cb();

    {
        nh_sdio3_trans_desc_t trans_desc;

        while (nh_sdio3_dnld_pop(sdio3_hd, &trans_desc, 1) == 0) {
            msg_ctrl = (nh_msg_ctrl_t *)trans_desc.user_arg;
            frame_elem = (frame_elem_t *)((uintptr_t)trans_desc.buff - FRAME_BUFF_MSGSTRUCT_OFFSET);

            EXAMPLE_PRINTF("dnld pop:%d\r\n", FRAME_BUFF_MSGSTRUCT_OFFSET);
            frame_elem->data_size = 0;
            nh_msg_dnld_recv_done_cb(msg_ctrl, frame_elem, false);
        }
        while (nh_sdio3_upld_pop(sdio3_hd, &trans_desc, 1) == 0) {
            msg_ctrl = (nh_msg_ctrl_t *)trans_desc.user_arg;
            offset = transportsdio_get_payload_offset((uint8_t *)trans_desc.buff+FRAME_BUFF_MSGSTRUCT);
            EXAMPLE_PRINTF("upld pop offset:%d\r\n", offset);
            frame_elem = (frame_elem_t *)((uintptr_t)trans_desc.buff - offset + FRAME_BUFF_MSGSTRUCT);

            nh_msg_upld_send_done_cb(msg_ctrl, frame_elem, true);
        }
    }

    /* Reset SDIO3 queue control */
    nh_sdio3_deinit(sdio3_hd);

    /* Re-initialize SDIO3 */
    _sdio3_reinit();
}
#endif
