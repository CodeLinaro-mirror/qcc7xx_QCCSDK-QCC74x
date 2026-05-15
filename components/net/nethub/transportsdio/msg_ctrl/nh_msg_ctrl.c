/**
 * @file nh_msg_ctrl.c
 * @brief Message controller implementation for SDIO message routing and management
 * @author mlwang
 * @date 2025-07-17
 */

#include "qcc74x_core.h"
#include "qcc74x_sdio2.h"
//#include "qcc74x_sdio3.h"
//#include "mm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#include "nh_frame_buff_ctrl.h"
#include "nh_msg_ctrl.h"
#include "nh_sdio_drv.h"

#define DBG_TAG "MSG"
#include "log.h"

/**
 * @brief Simple packet distribution interface
 *
 * Routes packets to corresponding callback functions based on tag.
 */
/** Frame buffer memory attribute (aligned and non-cached) */
#define FRAME_BUFFER_ATTR __ALIGNED(32) __attribute__((section("SHAREDRAM")));

/** Download frame buffer pool */
uint8_t msg_dnld_frame_buff[nh_MSG_CTRL_DNLD_FRAME_NUM][FRAME_BUFF_TOTALSIZE] FRAME_BUFFER_ATTR;

/** Message processing task handle */
static TaskHandle_t msg_proc_task_hd;

static nh_msg_ctrl_t *g_msg_ctrl;

typedef struct {
    nh_msg_packt_t msg_packt;
    uint8_t reserved[1];
    uint8_t flag;
    uint8_t credit_update_flag;
    uint8_t credit_limit_cnt;
} nh_msg_stat_packt_t;

#define nh_MSG_FLAG_IS_DATA(flag) (((flag) == 1U) || ((flag) == 2U))
#define nh_MSG_FLAG_IS_HANDSHAKE(flag) (((flag) >= 3U) && ((flag) <= 9U))
#define nh_MSG_PROTO_META_SIZE ((uint32_t)(sizeof(nh_msg_stat_packt_t) - sizeof(nh_msg_packt_t)))
#define nh_MSG_PAYLOAD_SIZE(msg_packt) \
    ((uint32_t)(((msg_packt)->len >= nh_MSG_PROTO_META_SIZE) ? ((msg_packt)->len - nh_MSG_PROTO_META_SIZE) : 0U))

/**
 * @brief Wake up message processing task
 * @retval 0 Success
 * @retval -1 Error
 */
static int msg_proc_task_wakeup(void)
{
    if (msg_proc_task_hd == NULL) {
        LOG_E("msg_proc_task_hd is NULL, please init msg_ctrl first!\r\n");
        return -1;
    }

    /* Wake up message processing task */
    if (xPortIsInsideInterrupt()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(msg_proc_task_hd, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotifyGive(msg_proc_task_hd);
    }

    return 0;
}

/**
 * @brief Message host reset callback
 *
 * This function is called when the message host is reset.
 * It wakes up the message processing task and sets the hardware reset flag.
 *
 * @retval 0 Success
 */
int nh_msg_host_reset_cb(void)
{
    LOG_W("msg host reset!\r\n");

    /* Wake up message processing task */
    msg_proc_task_wakeup();

    g_msg_ctrl->hw_reset_flag = true;

    return 0;
}

/**
 * @brief Upload send done interrupt callback
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element that was sent
 * @retval 0 Success
 */
int nh_msg_upld_send_done_cb(nh_msg_ctrl_t *msg_ctrl, frame_elem_t *frame_elem, bool success)
{
    int ret = 0;
    nh_msg_upld_cb_t upld_send_cb = NULL;
    void *upld_send_arg = NULL;
    nh_msg_packt_t *msg_packt = (nh_msg_packt_t *)frame_elem->buff_addr;

    /* Wake up message processing task */
    msg_proc_task_wakeup();

    // printf("--------> %s, frame_elem:%p, skb_free:%p, skb:%p, frame_elem->buff_addr:%p\r\n",
    //     __func__,
    //     frame_elem,
    //     frame_elem->skb_free,
    //     frame_elem->skb,
    //     frame_elem->buff_addr);

    if (msg_packt->tag >= nh_MSG_TAG_MAX) {
        LOG_E("msg upld send err: tag 0x%02X is invalid, drop it!\r\n", msg_packt->tag);
        goto err_exit;
    }

    upld_send_cb = msg_ctrl->nh_msg_upld_send_cb[msg_packt->tag];
    upld_send_arg = msg_ctrl->nh_msg_upld_send_arg[msg_packt->tag];

    if (success) {
        nh_msg_stream_stat_t *stat = &msg_ctrl->stream_stats[msg_packt->tag];
        uint8_t flag = 0;
        uint32_t data_size = nh_MSG_PAYLOAD_SIZE(msg_packt);

        if (frame_elem->data_size >= sizeof(nh_msg_stat_packt_t)) {
            flag = ((nh_msg_stat_packt_t *)frame_elem->buff_addr)->flag;
        }

        if (!nh_MSG_FLAG_IS_HANDSHAKE(flag)) {
            if (nh_MSG_FLAG_IS_DATA(flag)) {
                stat->tx_data_packets++;
                stat->tx_data_bytes += data_size;
            } else {
                stat->tx_ctrl_packets++;
                stat->tx_ctrl_bytes += data_size;
            }
        }
    }

    if (upld_send_cb != NULL) {
        /* Call the registered upload send done callback */
        ret = upld_send_cb(frame_elem, upld_send_arg, success);
        if (ret < 0) {
            goto err_exit;
        }
    } else {
        goto err_exit;
    }

    return 0;

err_exit:
    /* Free to pool_queue of message controller */
    frame_queue_free_elem(frame_elem);
    return 0;
}

/**
 * @brief Download receive done interrupt callback
 * @param msg_ctrl Message controller instance
 * @param frame_elem Received frame element
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_dnld_recv_done_cb(nh_msg_ctrl_t *msg_ctrl, frame_elem_t *frame_elem, bool success)
{
    nh_msg_packt_t *msg_packt = (nh_msg_packt_t *)frame_elem->buff_addr;

    /* Wake up message processing task */
    msg_proc_task_wakeup();

    // printf("%s, line:%d\r\n", __func__, __LINE__);
    if (success == false) {
        goto err_exit;
    }
    #if CONFIG_NETHUB_DUMP_DNLDDATA
        // Print detailed frame_elem information
        printf("    === frame_elem:%p details ===\r\n", frame_elem);
        printf("    frame_type: 0x%04X, frame_id: 0x%04X\r\n", frame_elem->frame_type, frame_elem->frame_id);
        printf("    buff_addr: 0x%08X, buff_size: %d, data_size: %d\r\n",
              (uint32_t)frame_elem->buff_addr, frame_elem->buff_size, frame_elem->data_size);
        printf("    frame_ctrl: 0x%08X, skb: 0x%08X\r\n",
              (uint32_t)frame_elem->frame_ctrl, (uint32_t)frame_elem->skb);

        // Print detailed msg_packt information
        printf("    === msg_packt details ===\r\n");
        printf("    msg_packt address: 0x%08X\r\n", (uint32_t)msg_packt);
        printf("    tag: 0x%02X, sub_tag: 0x%02X, len: %d\r\n",
              msg_packt->tag, msg_packt->sub_tag, msg_packt->len);
        printf("    Expected total length: %d (header: %zu + data: %d)\r\n",
              msg_packt->len + sizeof(nh_msg_packt_t), sizeof(nh_msg_packt_t), msg_packt->len);
        // Dump memory data
        void nethub_dumpbuf(uint8_t *str, uint8_t *buf, int len);
        nethub_dumpbuf("frame_elem buffer data", (uint8_t *)frame_elem->buff_addr,
            frame_elem->data_size > 64 ? 64 : frame_elem->data_size);
    #endif
    if (msg_packt->tag >= nh_MSG_TAG_MAX) {
        LOG_E("msg dnld recv err: tag 0x%02X is invalid, drop it!\r\n", msg_packt->tag);
        goto err_exit;
    }

    if (frame_elem->data_size < msg_packt->len + sizeof(nh_msg_packt_t)) {
        LOG_E("msg dnld recv err: frame size (%d < %d) is too small for tag 0x%02X, drop it!\r\n",
              frame_elem->data_size, msg_packt->len + sizeof(nh_msg_packt_t), msg_packt->tag);
        goto err_exit;
    }

    {
        nh_msg_stream_stat_t *stat = &msg_ctrl->stream_stats[msg_packt->tag];
        uint8_t flag = 0;
        uint32_t data_size = nh_MSG_PAYLOAD_SIZE(msg_packt);

        if (frame_elem->data_size >= sizeof(nh_msg_stat_packt_t)) {
            flag = ((nh_msg_stat_packt_t *)frame_elem->buff_addr)->flag;
        }

        if (!nh_MSG_FLAG_IS_HANDSHAKE(flag)) {
            if (nh_MSG_FLAG_IS_DATA(flag)) {
                stat->rx_data_packets++;
                stat->rx_data_bytes += data_size;
            } else {
                stat->rx_ctrl_packets++;
                stat->rx_ctrl_bytes += data_size;
            }
        }
    }

    /* Send to message download queue */
    frame_queue_send(msg_ctrl->dnld_queue, &frame_elem, 0);
    return 0;

err_exit:
    /* Free to pool_queue of message controller */
    frame_queue_free_elem(frame_elem);
    return 0;
}

void nh_msg_ctrl_reset_stream_stats(nh_msg_ctrl_t *msg_ctrl)
{
    if (msg_ctrl == NULL) {
        return;
    }

    memset((void *)msg_ctrl->stream_stats, 0, sizeof(msg_ctrl->stream_stats));
}

void nh_msg_ctrl_reset_stream_stat(nh_msg_ctrl_t *msg_ctrl, uint8_t tag)
{
    if (msg_ctrl == NULL || tag >= nh_MSG_TAG_MAX) {
        return;
    }

    memset((void *)&msg_ctrl->stream_stats[tag], 0, sizeof(msg_ctrl->stream_stats[tag]));
}

/**
 * @brief Allocate frame element from message controller pool
 * @param msg_ctrl Message controller instance
 * @param xTicksToWait Ticks to wait for allocation
 * @retval Pointer to allocated frame element on success
 * @retval NULL on failure
 */
frame_elem_t *nh_msg_ctrl_alloc(nh_msg_ctrl_t *msg_ctrl, TickType_t xTicksToWait)
{
    int ret;
    frame_elem_t *frame_elem = NULL;

    if (msg_ctrl == NULL) {
        LOG_E("msg_ctrl is NULL\r\n");
        return NULL;
    }

    //alloc  (32,
    /* Allocate element from message pool */
    ret = frame_queue_alloc_elem(msg_ctrl->dnld_queue_ctrl, &frame_elem, xTicksToWait);
    if (ret < 0) {
        return NULL;
    }

    return frame_elem;
}

/**
 * @brief Send upload message
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element to send
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_ctrl_send(nh_msg_ctrl_t *msg_ctrl, frame_elem_t *frame_elem, uint8_t tofront)
{
    int ret;
    nh_msg_packt_t *msg_packt = (nh_msg_packt_t *)frame_elem->buff_addr;

    // printf("<-------- %s, %d, msg_ctrl:%p, frame_elem:%p, msg_packt:%p\r\n",
    //     __func__, __LINE__,
    //     msg_ctrl, frame_elem, msg_packt);
    if (msg_packt->tag >= nh_MSG_TAG_MAX) {
        printf("msg send error: tag 0x%02X is invalid, drop it!\r\n", msg_packt->tag);
        return -1;
    }
    if (frame_elem->data_size < msg_packt->len + sizeof(nh_msg_packt_t)) {
        LOG_E("msg send err: frame size (%d < %d) is too small for tag 0x%02X !\r\n",
              frame_elem->data_size, msg_packt->len + sizeof(nh_msg_packt_t), msg_packt->tag);

        return -1;
    }

    ret = frame_queue_send(msg_ctrl->upld_queue, &frame_elem, 0);

    /* Wake up message processing task */
    msg_proc_task_wakeup();

    return ret;
}

/**
 * @brief Free frame element after processing callback
 * @param frame_elem Frame element being freed
 * @param arg Callback argument (unused)
 */
void nh_msg_ctrl_free_after_cb(frame_elem_t *frame_elem, void *arg)
{
    /* Wake up message processing task */
    msg_proc_task_wakeup();
}

/**
 * @brief Message processing task
 * @param arg Task argument (message controller instance)
 * @note This task handles upload/download operations and message routing
 */
static void msg_proc_task(void *arg)
{
    int ret;
    nh_msg_ctrl_t *msg_ctrl = (nh_msg_ctrl_t *)arg;
    frame_elem_t *frame_elem;
    nh_msg_packt_t *msg_packt;

    bool upld_send_suspend = false;
    bool dnld_recv_suspend = false;
    bool dnld_proc_suspend = false;

    extern struct nh_device_s *sdio2_hd;

wait_ready:
    LOG_W("%s msg_proc_task start/restart\r\n", msg_ctrl->msg_sdio_ops->sdio_name);

    msg_ctrl->hw_reset_flag = false;
    /* Wait for SDIO to be ready */
    while (msg_ctrl->msg_sdio_ops->sdio_is_ready() < 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    LOG_W("%s is_ready\r\n", msg_ctrl->msg_sdio_ops->sdio_name);

    while (1) {
        /* Wait for message if all operations are suspended */
        if (upld_send_suspend && dnld_recv_suspend && dnld_proc_suspend) {
            if (msg_ctrl->hw_reset_flag) {
                /* Call hardware reset callbacks */
                for (int i = 0; i < nh_MSG_TAG_MAX; i++) {
                    if (msg_ctrl->nh_msg_hw_reset_cb[i] != NULL) {
                        msg_ctrl->nh_msg_hw_reset_cb[i](NULL, msg_ctrl->nh_msg_hw_reset_arg[i]);
                    }
                }
                goto wait_ready;
            }
            /* Wait for notification to wake up */
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        }

        /* 1. Try to start upload send */
        if (msg_ctrl->hw_reset_flag || msg_ctrl->msg_sdio_ops->sdio_upld_is_busy()) {
            upld_send_suspend = true;
        } else {
            ret = frame_queue_receive(msg_ctrl->upld_queue, &frame_elem, 0);
            if (ret < 0) {
                upld_send_suspend = true;
            } else {
                upld_send_suspend = false;
                ret = msg_ctrl->msg_sdio_ops->sdio_upld_start(frame_elem, msg_ctrl);
                if (ret < 0) {
                    LOG_W("%s upld start failed, ret: %d\r\n", msg_ctrl->msg_sdio_ops->sdio_name, ret);
                    nh_msg_upld_send_done_cb(msg_ctrl, frame_elem, false);
                }
            }
        }

        /* 2. Try to start download receive */
        if (msg_ctrl->hw_reset_flag || msg_ctrl->msg_sdio_ops->sdio_dnld_is_busy()) {
            dnld_recv_suspend = true;
        } else {
            ret = frame_queue_alloc_elem(msg_ctrl->dnld_queue_ctrl, &frame_elem, 0);
            // Print detailed frame_elem information
            if (ret < 0) {
                // if (nh_sdio3_feature_control(sdio3_hd, SDIO3_CMD_GET_DNLD_QUEUE_WAIT_NUM, 1) == 0) {
                //     // LOG_E("Failed to allocate frame element for download\r\n");
                // }
                dnld_recv_suspend = true;
            } else {
                #if CONFIG_NETHUB_DUMP_DNLDDATA
                printf("   alloc     === frame_elem:%p details ===\r\n", frame_elem);
                printf("   alloc     frame_type: 0x%04X, frame_id: 0x%04X\r\n", frame_elem->frame_type, frame_elem->frame_id);
                printf("   alloc     buff_addr: 0x%08X, buff_size: %d, data_size: %d\r\n",
                      (uint32_t)frame_elem->buff_addr, frame_elem->buff_size, frame_elem->data_size);
                printf("   alloc     frame_ctrl: 0x%08X, skb: 0x%08X\r\n",
                        (uint32_t)frame_elem->frame_ctrl, (uint32_t)frame_elem->skb);
                #endif

                dnld_recv_suspend = false;
                ret = msg_ctrl->msg_sdio_ops->sdio_dnld_start(frame_elem, msg_ctrl);
                if (ret < 0) {
                    LOG_E("%s dnld start failed, ret: %d\r\n", msg_ctrl->msg_sdio_ops->sdio_name, ret);
                    frame_queue_free_elem(frame_elem);
                }
            }
        }

        /* 3. Process download message */
        ret = frame_queue_receive(msg_ctrl->dnld_queue, &frame_elem, 0);
        if (ret < 0) {
            dnld_proc_suspend = true;
        } else {
            dnld_proc_suspend = false;
            msg_packt = (nh_msg_packt_t *)frame_elem->buff_addr;

            /* Check if the tag is registered */
            if (msg_ctrl->nh_msg_dnld_recv_cb[msg_packt->tag] == NULL) {
                // LOG_W("msg dnld recv err: tag 0x%02X is not registered, drop it!\r\n", msg_packt->tag);
                frame_queue_free_elem(frame_elem);
            } else {
                /* Call the registered callback */
                ret = msg_ctrl->nh_msg_dnld_recv_cb[msg_packt->tag](frame_elem, msg_ctrl->nh_msg_dnld_recv_arg[msg_packt->tag]);
                if (ret < 0) {
                    LOG_E("msg dnld recv err: tag 0x%02X callback failed, drop it!\r\n", msg_packt->tag);
                    frame_queue_free_elem(frame_elem);
                }
            }
        }

#if 0 /* msg debug info */
        static uint32_t time_ms = 0;
        if (nh_mtimer_get_time_ms() - time_ms > 500) {
            time_ms = nh_mtimer_get_time_ms();
            LOG_I("msg upld wait: %d, dnld wait: %d, dnld idle pool: %d\r\n",
                  sdio_feature_control(sdio_hd,
                      (msg_ctrl->sdio_type == SDIO_TYPE_3) ? SDIO3_CMD_GET_UPLD_QUEUE_WAIT_NUM : SDIO2_CMD_GET_UPLD_QUEUE_WAIT_NUM,
                      0),
                  sdio_feature_control(sdio_hd,
                      (msg_ctrl->sdio_type == SDIO_TYPE_3) ? SDIO3_CMD_GET_DNLD_QUEUE_WAIT_NUM : SDIO2_CMD_GET_DNLD_QUEUE_WAIT_NUM,
                      0),
                  frame_queue_messages_waiting(msg_ctrl->dnld_queue_ctrl->pool_queue));
        }
#endif
    }
}

/**
 * @brief Initialize message controller
 * @retval Pointer to message controller instance on success
 * @retval NULL on failure
 */
nh_msg_ctrl_t *nh_msg_ctrl_init(nh_msg_ctrl_ops_t *sdio_ops)
{
    frame_queue_ctrl_t *dnld_queue_ctrl = NULL;
    nh_msg_ctrl_t *msg_ctrl = NULL;

    msg_ctrl = kmalloc(sizeof(nh_msg_ctrl_t), 0);
    memset(msg_ctrl, 0, sizeof(nh_msg_ctrl_t));

    if (msg_ctrl == NULL) {
        LOG_E("msg_ctrl malloc failed\r\n");
    }

    msg_ctrl->msg_sdio_ops = sdio_ops;

    msg_ctrl->dnld_queue = xQueueCreate(nh_MSG_CTRL_DNLD_FRAME_NUM, sizeof(void *));
    msg_ctrl->upld_queue = xQueueCreate(nh_MSG_CTRL_UPLD_QUEUE_DEPTH, sizeof(void *));

    for (int i = 0; i < nh_MSG_CTRL_DNLD_FRAME_NUM; i++) {
        LOG_D("num[%d] ----: elem:%p, payload:%p\r\n",
                i,
                &(msg_dnld_frame_buff[i][0]),
                &(msg_dnld_frame_buff[i][FRAME_BUFF_ELEM_OFFSET]) );
    }
    frame_queue_ctrl_init_cfg_t cfg = {
        .frame_buff = (uint8_t *)msg_dnld_frame_buff,
        .frame_type = nh_MSG_CTRL_DNLD_FRAME_TYPE,
        .frame_elem_cnt = nh_MSG_CTRL_DNLD_FRAME_NUM,
        .frame_elem_size = FRAME_BUFF_TOTALSIZE,
        .name = "msg_ctrl_queue",
    };

    /* Create download queue controller */
    dnld_queue_ctrl = frame_queue_create(&cfg);
    if (dnld_queue_ctrl == NULL) {
        LOG_E("msg_ctrl dnld queue create failed\r\n");
        return NULL;
    }
    msg_ctrl->dnld_queue_ctrl = dnld_queue_ctrl;

    g_msg_ctrl = msg_ctrl;

    /* Register the callback for freeing frame_elem after processing */
    frame_queue_free_cb_register(dnld_queue_ctrl,
                                 NULL, NULL,
                                 nh_msg_ctrl_free_after_cb, NULL);

    /* Create message processing task */
    xTaskCreate(msg_proc_task, "msg_proc_task", 1024, msg_ctrl, 20, &msg_proc_task_hd);

    return msg_ctrl;
}

/**
 * @brief Deinitialize message controller
 * @param msg_ctrl Message controller instance
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_ctrl_deinit(nh_msg_ctrl_t *msg_ctrl)
{
    if (msg_ctrl == NULL) {
        LOG_E("msg_ctrl is NULL\r\n");
        return -1;
    }

    /* Delete message processing task */
    vTaskDelete(msg_proc_task_hd);
    msg_proc_task_hd = NULL;

    /* Delete queues */
    vQueueDelete(msg_ctrl->dnld_queue);
    vQueueDelete(msg_ctrl->upld_queue);

    /* Delete download queue controller */
    frame_queue_remove(msg_ctrl->dnld_queue_ctrl);

    kfree(msg_ctrl);
    return 0;
}

/**
 * @brief Register message callbacks
 * @param msg_ctrl Message controller instance
 * @param tag Message tag for routing
 * @param dnld_recv_cb Download receive callback function
 * @param dnld_recv_arg Argument for download receive callback
 * @param upld_send_cb Upload send done callback function
 * @param upld_send_arg Argument for upload send done callback
 * @param hw_reset_cb Hardware reset callback function
 * @param hw_reset_arg Argument for hardware reset callback
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_cb_register(nh_msg_ctrl_t *msg_ctrl, uint8_t tag,
                       nh_msg_cb_t dnld_recv_cb, void *dnld_recv_arg,
                       nh_msg_upld_cb_t upld_send_cb, void *upld_send_arg,
                       nh_msg_cb_t hw_reset_cb, void *hw_reset_arg)
{
    uintptr_t flag;

    if (msg_ctrl == NULL) {
        LOG_E("Invalid msg_ctrl for cb register\r\n");
        return -1;
    }

    if (tag >= nh_MSG_TAG_MAX) {
        LOG_E("Invalid msg tag: 0x%02X\r\n", tag);
        return -1;
    }

    flag = qcc74x_irq_save();

    /* Register download receive callback if provided */
    if (dnld_recv_cb != NULL) {
        msg_ctrl->nh_msg_dnld_recv_cb[tag] = dnld_recv_cb;
        msg_ctrl->nh_msg_dnld_recv_arg[tag] = dnld_recv_arg;
    }

    /* Register upload send done callback if provided */
    if (upld_send_cb != NULL) {
        msg_ctrl->nh_msg_upld_send_cb[tag] = upld_send_cb;
        msg_ctrl->nh_msg_upld_send_arg[tag] = upld_send_arg;
    }

    if (hw_reset_cb != NULL) {
        msg_ctrl->nh_msg_hw_reset_cb[tag] = hw_reset_cb;
        msg_ctrl->nh_msg_hw_reset_arg[tag] = hw_reset_arg;
    }

    qcc74x_irq_restore(flag);

    LOG_I("Msg callback registered for tag 0x%02X\r\n", tag);

    return 0;
}

/**
 * @brief Unregister message callbacks
 * @param msg_ctrl Message controller instance
 * @param tag Message tag for routing
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_cb_unregister(nh_msg_ctrl_t *msg_ctrl, uint8_t tag)
{
    if (msg_ctrl == NULL) {
        LOG_E("Invalid msg_ctrl for cb unregister\r\n");
        return -1;
    }

    if (tag >= nh_MSG_TAG_MAX) {
        LOG_E("Invalid msg tag: 0x%02X\r\n", tag);
        return -1;
    }

    msg_ctrl->nh_msg_dnld_recv_cb[tag] = NULL;
    msg_ctrl->nh_msg_dnld_recv_arg[tag] = NULL;
    msg_ctrl->nh_msg_upld_send_cb[tag] = NULL;
    msg_ctrl->nh_msg_upld_send_arg[tag] = NULL;

    LOG_I("Msg callbacks unregistered for tag 0x%02X\r\n", tag);
    return 0;
}
