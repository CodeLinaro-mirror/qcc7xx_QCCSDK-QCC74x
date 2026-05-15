/**
 * @file nh_tty.c
 * @brief Simplified TTY Device Implementation
 * @author mlwang
 * @date 2025-08-08
 * @details TTY implementation: application module based on msg_ctrl,
 *          - TTY data output: Use LOG_I to output received data
 *          - MSG transfer callbacks: msg_dnld_recv_cb/msg_upld_send_cb (handle message transfer completion)
 */

#include "qcc74x_core.h"

#include "board.h"
#include "shell.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"
#include "nh_tty.h"

#define DBG_TAG "TTY"
#include "log.h"

/*****************************************************************************
 * Global Variables
 *****************************************************************************/

/** Frame buffer memory attribute (aligned and non-cached) */
#define FRAME_BUFFER_ATTR __ALIGNED(32) ATTR_NOCACHE_NOINIT_RAM_SECTION
/** Download frame buffer pool */
static FRAME_BUFFER_ATTR uint8_t tty_frame_buff[TTY_FRAME_CNT][TTY_FRAME_SIZE];

/** Global TTY private structure pointer */
static struct nh_tty_priv *g_tty_priv = NULL;

/*****************************************************************************
 * Forward Declarations
 *****************************************************************************/

static void tty_proc_task(void *arg);

static int msg_dnld_recv_done_cb(frame_elem_t *frame_elem, void *arg);
static int msg_upld_send_done_cb(frame_elem_t *frame_elem, void *arg, bool success);

/**
 * @brief Wake up the TTY processing task
 * @param[in] priv Pointer to TTY private structure
 * @retval 0 Success
 * @retval <0 Error
 */
static int tty_proc_task_wakeup(struct nh_tty_priv *priv)
{
    if (priv->tty_proc_task == NULL) {
        LOG_E("tty_proc_task is NULL, please init msg_ctrl first!\r\n");
        return -1;
    }

    /* Wake up message processing task */
    if (xPortIsInsideInterrupt()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(priv->tty_proc_task, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotifyGive(priv->tty_proc_task);
    }

    return 0;
}

/**
 * @brief Callback after frame buffer is freed, used to wake up the processing task
 * @param[in] frame_elem Pointer to frame element
 * @param[in] arg Pointer to TTY private structure
 */
void tty_free_after_cb(frame_elem_t *frame_elem, void *arg)
{
    tty_proc_task_wakeup((struct nh_tty_priv *)arg);
}

/*****************************************************************************
 * TTY TX/RX data processing
 *****************************************************************************/

/**
 * @brief Process TTY data output using LOG_I
 * @param[in] priv Pointer to TTY private structure
 * @param[in] frame_elem Pointer to frame element
 * @param[in] success Processing success flag
 */
static void tty_dnld_data_output(struct nh_tty_priv *priv, frame_elem_t *frame_elem)
{
    struct nh_tty_msg_packt *msg_packt;
    uint16_t data_len;

    msg_packt = (struct nh_tty_msg_packt *)frame_elem->buff_addr;
    data_len = msg_packt->msg_packt.len - (sizeof(struct nh_tty_msg_packt) - sizeof(nh_msg_packt_t));

    if (data_len <= TTY_FRAME_RAWSIZE) {
        if (priv->dnld_cb) {
            priv->dnld_cb (priv->cbpri_arg, msg_packt->data, data_len);
        }
    } else {
        printf("%s, pkt err and drop it, data_len:%d, mtu:%d\r\n", __func__, data_len, TTY_FRAME_RAWSIZE);
    }

    /* Free the frame */
    frame_queue_free_elem(frame_elem);

#if TTY_DNLD_CREDIT_MAX
    nh_rx_credit_reclaim(&priv->flow_ctrl);

    if (nh_rx_credit_need_update(&priv->flow_ctrl)) {
        tty_proc_task_wakeup(priv);
    }
#endif
}

/*****************************************************************************
 * MSG transport callbacks (msg_*)
 *****************************************************************************/

/**
 * @brief MSG download receive done callback, puts received message into queue for processing
 * @param[in] frame_elem Pointer to frame element
 * @param[in] arg Pointer to TTY private structure
 * @retval 0 Success
 * @retval <0 Error
 */
static int msg_dnld_recv_done_cb(frame_elem_t *frame_elem, void *arg)
{
    int ret;
    struct nh_tty_priv *priv = (struct nh_tty_priv *)arg;

    if (!priv || !frame_elem) {
        return -1;
    }

    ret = frame_queue_send(priv->msg_dnld_queue, &frame_elem, 0);
    if (ret < 0) {
        LOG_E("Failed to send frame to download queue\r\n");
        return -1;
    }

    tty_proc_task_wakeup(priv);
    return 0;
}

/**
 * @brief MSG upload send done callback, releases data back to pool
 * @param[in] frame_elem Pointer to frame element
 * @param[in] arg Pointer to TTY private structure
 * @retval 0 Success
 * @retval <0 Error
 */
static int msg_upld_send_done_cb(frame_elem_t *frame_elem, void *arg, bool success)
{
    int ret;
    struct nh_tty_priv *priv = (struct nh_tty_priv *)arg;
    struct nh_tty_msg_packt *msg_packt;

    if (!priv || !frame_elem) {
        return -1;
    }

    msg_packt = (struct nh_tty_msg_packt *)frame_elem->buff_addr;
    if (!success && msg_packt->flag == nh_TTY_FLAG_UPLD_DATA) {
        nh_tx_credit_put(&priv->flow_ctrl);
    }

    /* Release back to pool */
    ret = frame_queue_free_elem(frame_elem);

    return ret;
}

/**
 * @brief Send MSG upload control packet
 * @param[in] priv Pointer to TTY private structure
 * @param[in] tty_flag TTY flag to send
 * @retval 0 Success
 * @retval <0 Error
 */
static int msg_upld_send_cmd_packt(struct nh_tty_priv *priv, uint8_t tty_flag, uint8_t tofront, int credit)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_tty_msg_packt *msg_tx_packt;

    ret = frame_queue_alloc_elem(priv->rx_frame_ctrl, &frame_elem, 100);
    if (ret < 0) {
        LOG_E("Failed to allocate RX frame element\r\n");
        return -1;
    }

    msg_tx_packt = (struct nh_tty_msg_packt *)frame_elem->buff_addr;

    frame_elem->data_size = sizeof(struct nh_tty_msg_packt);

    msg_tx_packt->msg_packt.len = frame_elem->data_size - sizeof(nh_msg_packt_t);
    msg_tx_packt->msg_packt.tag = priv->msg_tag;
    msg_tx_packt->msg_packt.sub_tag = 0;
    msg_tx_packt->flag = tty_flag;

#if TTY_DNLD_CREDIT_MAX
    if (credit >= 0) {
        msg_tx_packt->credit_update_flag = true;
        msg_tx_packt->credit_limit_cnt = (uint8_t)credit;
    } else {
        msg_tx_packt->credit_update_flag = false;
        msg_tx_packt->credit_limit_cnt = 0;
    }

    ret = nh_msg_ctrl_send(priv->msg_ctrl, frame_elem, tofront);
#else
    msg_tx_packt->credit_update_flag = false;
    msg_tx_packt->credit_limit_cnt = 0;
    ret = nh_msg_ctrl_send(priv->msg_ctrl, frame_elem, tofront);
#endif

    if (ret < 0) {
        LOG_E("Failed to send TTY control message\r\n");
        frame_queue_free_elem(frame_elem);
        return -1;
    }

    return 0;
}

/**
 * @brief Send TTY data for upload
 * @param[in] priv Pointer to TTY private structure
 * @param[in] data_buff Pointer to data buffer to send
 * @param[in] data_size Size of data to send
 * @retval 0 Success
 * @retval <0 Error
 */
static int tty_upld_data_send(struct nh_tty_priv *priv, const uint8_t *data_buff, uint32_t data_size)
{
    int ret;
    frame_elem_t *frame_elem;
    struct nh_tty_msg_packt *msg_tx_packt;

    if (!priv || !data_buff || data_size == 0) {
        return -1;
    }

    ret = nh_tx_credit_get(&priv->flow_ctrl, portMAX_DELAY);
    if (ret < 0) {
        LOG_E("Failed to get TTY upld credit\r\n");
        return -1;
    }

    ret = frame_queue_alloc_elem(priv->rx_frame_ctrl, &frame_elem, 10);
    if (ret < 0) {
        nh_tx_credit_put(&priv->flow_ctrl);
        return -1; /* No available buffer, try later */
    }

    msg_tx_packt = (struct nh_tty_msg_packt *)frame_elem->buff_addr;

    /* Copy user data */
    memcpy(msg_tx_packt->data, data_buff, data_size);
    frame_elem->data_size = sizeof(struct nh_tty_msg_packt) + data_size;

    msg_tx_packt->msg_packt.tag = priv->msg_tag;
    msg_tx_packt->msg_packt.sub_tag = 0;
    msg_tx_packt->msg_packt.len = frame_elem->data_size - sizeof(nh_msg_packt_t);
    msg_tx_packt->flag = nh_TTY_FLAG_UPLD_DATA;

#if TTY_DNLD_CREDIT_MAX

    ret = nh_rx_credit_update(&priv->flow_ctrl);
    msg_tx_packt->credit_limit_cnt = (ret >= 0) ? (uint8_t)ret : 0;
    msg_tx_packt->credit_update_flag = (ret >= 0) ? true : false;

    /* Send through msg ctrl */
    ret = nh_msg_ctrl_send(priv->msg_ctrl, frame_elem, 0);
#else
    msg_tx_packt->credit_update_flag = false;
    msg_tx_packt->credit_limit_cnt = 0;

    ret = nh_msg_ctrl_send(priv->msg_ctrl, frame_elem, 0);
#endif

    if (ret < 0) {
        nh_tx_credit_put(&priv->flow_ctrl);
        LOG_E("Failed to send TTY upld data to msg_ctrl: %d\r\n", ret);
        frame_queue_free_elem(frame_elem);
        return -1;
    }

    return 0;
}

/**
 * @brief Hardware reset callback for message controller
 * @param[in] unused Unused parameter
 * @param[in] priv Pointer to TTY private structure
 * @retval 0 Success
 * @retval <0 Error
 */
static int msg_hw_reset_cb(frame_elem_t *unused, void *arg)
{
    struct nh_tty_priv *priv = (struct nh_tty_priv *)arg;

    nh_credit_reset(&priv->flow_ctrl);
    priv->reset_flag = true;
    tty_proc_task_wakeup(priv);

    LOG_W("Received hw reset request\r\n");

    return 0;
}

int nh_tty_upld_send(uint8_t *data_buff, uint32_t data_size)
{
    struct nh_tty_priv *priv = g_tty_priv;

    if (!priv) {
        return -1;
    }

    if (priv->tty_status != nh_TTY_DSTA_DEVICE_RUN) {
        printf("%s, drop data_size:%d, because of tty_status:%d\r\n", __FUNCTION__, data_size, priv->tty_status);
        return data_size;
    }
    return tty_upld_data_send(priv, data_buff, data_size);
}

int nh_tty_dnld_register(nh_tty_callback_t dnld_cb, void *cbpri_arg)
{
    struct nh_tty_priv *priv = g_tty_priv;

    priv->dnld_cb = dnld_cb;
    priv->cbpri_arg = cbpri_arg;

    return 0;
}

/*****************************************************************************
 * Daemon thread
 *****************************************************************************/

/**
 * @brief TTY daemon thread
 * @details Main tasks:
 *   1. Process received messages from host
 *   2. Handle state machine transitions
 *   3. Generate periodic uplink test data
 *   4. Monitor flow control
 * @param[in] arg Pointer to TTY private structure
 */
static void tty_proc_task(void *arg)
{
    int ret;
    struct nh_tty_priv *priv = (struct nh_tty_priv *)arg;

    frame_elem_t *frame_elem;
    struct nh_tty_msg_packt *msg_packt;

    // uint32_t upld_test_ms = 0;

    bool msg_dnld_recv_suspend = false;

    LOG_I("TTY daemon thread started\r\n");

    /* Initial state */
    priv->tty_status = nh_TTY_DSTA_IDLE;
    priv->tty_ready = false;

    while (!priv->stop_requested) {
        /* Wait for notification or timeout */
        if (msg_dnld_recv_suspend) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        }

        if (priv->reset_flag) {
            LOG_W("Processing reset request\r\n");
            priv->reset_flag = false;
            priv->tty_ready = false;
            priv->tty_status = nh_TTY_DSTA_IDLE;
            nh_credit_reset(&priv->flow_ctrl);
        }

        /* Get message from queue */
        ret = frame_queue_receive(priv->msg_dnld_queue, &frame_elem, 0);
        if (ret < 0) {
            msg_dnld_recv_suspend = true;
            frame_elem = NULL;
            msg_packt = NULL;
        } else {
            msg_dnld_recv_suspend = false;
            msg_packt = (struct nh_tty_msg_packt *)frame_elem->buff_addr;
        }

        /* Process reset/stop commands and reset the state machine. */
        if (msg_packt && (msg_packt->flag == nh_TTY_FLAG_HOST_RESET || msg_packt->flag == nh_TTY_FLAG_HOST_STOP)) {
            LOG_W("Received host %s command\r\n", msg_packt->flag == nh_TTY_FLAG_HOST_RESET ? "RESET" : "STOP");

            priv->tty_status = nh_TTY_DSTA_IDLE;
            priv->tty_ready = false;
            nh_credit_reset(&priv->flow_ctrl);

            frame_queue_free_elem(frame_elem);
            continue;
        }

        /* State machine */
        switch (priv->tty_status) {
            case nh_TTY_DSTA_IDLE:
                /* Wait for HOST to become ready */
                if (msg_packt == NULL || msg_packt->flag != nh_TTY_FLAG_HOST_READY) {
                    break;
                }
                LOG_I("Received HOST_READY message\r\n");

                priv->tty_status = nh_TTY_DSTA_HOST_READY;
                // break;

            case nh_TTY_DSTA_HOST_READY:

                if (msg_packt) {
                    nh_credit_init(&priv->flow_ctrl, "TTY",
                               msg_packt->credit_limit_cnt,
                               TTY_DNLD_CREDIT_UPDATE_THRESHOLD,
                               TTY_DNLD_CREDIT_MAX,
                               TTY_DNLD_CREDIT_UPDATE_THRESHOLD);
                }

                LOG_W("Host ready (credit=%d), sending DEVICE_READY\r\n",
                      priv->flow_ctrl.credit[NH_CREDIT_DIR_TX].credit_limit_cnt);
                /* Host ready, send DEVICE_START */
                ret = msg_upld_send_cmd_packt(priv, nh_TTY_FLAG_DEVICE_READY, 0, -1);
                if (ret < 0) {
                    LOG_E("Failed to send DEVICE_START command\r\n");
                    break;
                }

                priv->tty_status = nh_TTY_DSTA_DEVICE_READY;
                break;

            case nh_TTY_DSTA_DEVICE_READY:
                ret = msg_upld_send_cmd_packt(priv, nh_TTY_FLAG_DEVICE_START, 0, TTY_DNLD_CREDIT_MAX);
                if (ret < 0) {
                    LOG_W("Failed to send DEVICE_START message\r\n");
                    break;
                }


                nh_msg_ctrl_reset_stream_stat(priv->msg_ctrl, priv->msg_tag);
                priv->tty_status = nh_TTY_DSTA_DEVICE_RUN;
                nh_credit_enable(&priv->flow_ctrl, NH_CREDIT_EN_TX | NH_CREDIT_EN_RX);
                break;

            case nh_TTY_DSTA_DEVICE_RUN:
                if (msg_packt == NULL) {
                    break;
                }

                if (msg_packt->credit_update_flag) {
                    nh_tx_credit_update(&priv->flow_ctrl, msg_packt->credit_limit_cnt);
                }

                if (msg_packt->flag == nh_TTY_FLAG_OPEN) {
                    LOG_I("Received tty OPEN message\r\n");
                    if (priv->tty_ready == false) {
                        priv->tty_ready = true;
                    } else {
                        LOG_W("TTY already ready, ignoring OPEN message\r\n");
                    }

                } else if (msg_packt->flag == nh_TTY_FLAG_CLOSE) {
                    LOG_I("Received tty CLOSE message\r\n");
                    if (priv->tty_ready == true) {
                        priv->tty_ready = false;
                    } else {
                        LOG_W("TTY already closed, ignoring CLOSE message\r\n");
                    }

                } else if (msg_packt->flag == nh_TTY_FLAG_SET_TERMIOS) {
                    LOG_I("Received tty SET_TERMIOS message\r\n");

                } else if (msg_packt->flag == nh_TTY_FLAG_BREAK) {
                    LOG_I("Received tty BREAK message\r\n");

                } else if (msg_packt->flag == nh_TTY_FLAG_HANGUP) {
                    LOG_I("Received tty HANGUP message\r\n");
                }

                break;

            default:
                LOG_E("Invalid TTY status: %d\r\n", priv->tty_status);
                priv->tty_status = nh_TTY_DSTA_IDLE;
                if (msg_packt) {
                    frame_queue_free_elem(frame_elem);
                }
                break;
        }

        /* Output and release the received packet */
        if (msg_packt) {
            if (priv->tty_status == nh_TTY_DSTA_DEVICE_RUN && msg_packt->flag == nh_TTY_FLAG_DNLD_DATA) {
                tty_dnld_data_output(priv, frame_elem);
            } else {
                frame_queue_free_elem(frame_elem);
            }
        }

#if TTY_DNLD_CREDIT_MAX
        /* Send a proactive flow-control update */
        if (priv->tty_status == nh_TTY_DSTA_DEVICE_RUN && priv->tty_ready &&
            nh_rx_credit_need_update(&priv->flow_ctrl)) {
            ret = nh_rx_credit_update(&priv->flow_ctrl);
            if (ret >= 0) {
                ret = msg_upld_send_cmd_packt(priv, nh_TTY_FLAG_CREDIT_UPDATE, 1, ret);
            }
            if (ret < 0) {
                LOG_E("Failed to send credit update message\r\n");
            }
        }
#endif

    }

    LOG_I("TTY daemon thread exiting\r\n");
    vTaskDelete(NULL);
}

static void cmd_tty_send_str(int argc, char **argv)
{
    static char send_buffer[512];
    int buffer_pos = 0;
    int ret;
    struct nh_tty_priv *priv;

    if (argc < 2) {
        LOG_W("Usage: %s <str_1> <str_2> ...\r\n", argv[0]);
        return;
    }

    priv = g_tty_priv;
    if (!priv) {
        LOG_E("TTY not initialized\r\n");
        return;
    }

    /* Check state consistency */
    if (priv->tty_status != nh_TTY_DSTA_DEVICE_RUN || !priv->tty_ready) {
        LOG_E("TTY not ready or not in RUN state (status=%d, ready=%d)\r\n",
              priv->tty_status, priv->tty_ready);
        return;
    }

    /* Clear buffer */
    memset(send_buffer, 0, sizeof(send_buffer));

    /* Concatenate all arguments with space separator */
    for (int i = 1; i < argc; i++) {
        int arg_len = strlen(argv[i]);

        /* Check if we have enough space for this argument + space + null terminator */
        if (buffer_pos + arg_len + 2 >= sizeof(send_buffer)) {
            LOG_W("Command line too long, truncating at %d characters\r\n", buffer_pos);
            break;
        }

        /* Add space before argument (except for first one) */
        if (i > 1) {
            send_buffer[buffer_pos++] = ' ';
        }

        /* Copy argument */
        memcpy(&send_buffer[buffer_pos], argv[i], arg_len);
        buffer_pos += arg_len;
    }

    /* Add newline at the end */
    if (buffer_pos + 2 < sizeof(send_buffer)) {
        send_buffer[buffer_pos++] = '\r';
        send_buffer[buffer_pos++] = '\n';
    }

    /* Send the concatenated string */
    ret = tty_upld_data_send(priv, (const uint8_t *)send_buffer, buffer_pos);
    if (ret < 0) {
        LOG_E("Failed to send TTY data: %d\r\n", ret);
    } else {
        LOG_I("Sent %d bytes: %.*s", buffer_pos, buffer_pos - 1, send_buffer);
    }
}
SHELL_CMD_EXPORT_ALIAS(cmd_tty_send_str, tty_send_str, cmd tty_send_str);

/*****************************************************************************
 * Public API Implementation
 *****************************************************************************/

/**
 * @brief Initialize TTY device
 * @param[in] msg_ctrl Message control interface
 * @param[in] msg_tag Message tag for routing
 * @retval 0 Success
 * @retval <0 Error code
 */
int nh_tty_init(nh_msg_ctrl_t *msg_ctrl, uint8_t msg_tag)
{
    int ret;
    struct nh_tty_priv *priv;

    /* Allocate the private structure */
    priv = malloc(sizeof(struct nh_tty_priv));
    if (!priv) {
        LOG_E("Failed to allocate TTY private structure\r\n");
        return -1;
    }
    memset(priv, 0, sizeof(struct nh_tty_priv));

    /* Initialize the basic fields */
    priv->msg_ctrl = msg_ctrl;
    priv->msg_tag = msg_tag;
    priv->tty_status = nh_TTY_DSTA_IDLE;
    priv->stop_requested = false;
    priv->tty_ready = false;

    ret = nh_credit_init(&priv->flow_ctrl, "TTY",
                         0,
                         TTY_DNLD_CREDIT_UPDATE_THRESHOLD,
                         TTY_DNLD_CREDIT_MAX,
                         TTY_DNLD_CREDIT_UPDATE_THRESHOLD);
    if (ret < 0) {
        LOG_E("Failed to init TTY flow control\r\n");
        free(priv);
        return -1;
    }

    /* Initialize private memory pool */
    frame_queue_ctrl_init_cfg_t rx_cfg = {
        .frame_buff = tty_frame_buff,
        .frame_type = 0x03,
        .frame_elem_cnt = TTY_FRAME_CNT,
        .frame_elem_size = TTY_FRAME_SIZE,
        .name = "tty"
    };
    priv->rx_frame_ctrl = frame_queue_create(&rx_cfg);
    if (!priv->rx_frame_ctrl) {
        LOG_E("Failed to create TTY frame queue\r\n");
        free(priv);
        return -1;
    }
    /* Register the callback for freeing frame_elem after processing */
    frame_queue_free_cb_register(priv->rx_frame_ctrl,
                                 NULL, NULL,
                                 tty_free_after_cb, priv);

    /* Create message download queue */
    priv->msg_dnld_queue = xQueueCreate(TTY_MSG_QUEUE_DEPTH, sizeof(frame_elem_t *));

    /* Register MSG message callbacks */
    ret = nh_msg_cb_register(msg_ctrl, msg_tag,
                             msg_dnld_recv_done_cb, priv,
                             msg_upld_send_done_cb, priv,
                             msg_hw_reset_cb, priv);
    if (ret < 0) {
        LOG_E("Failed to register TTY message callbacks\r\n");
        return -1;
    }

    g_tty_priv = priv;

    /* Create the daemon thread */
    ret = xTaskCreate(tty_proc_task, "tty_proc", 2048, priv, 15, &priv->tty_proc_task);
    if (ret != pdPASS) {
        LOG_E("Failed to create TTY processing task\r\n");
        return -1;
    }

    LOG_I("TTY device initialized successfully, tag=%d\r\n", msg_tag);

    return 0;
}
