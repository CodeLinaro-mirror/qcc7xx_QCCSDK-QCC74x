/**
 * @file mr_msg_ctrl.c
 * @brief Message controller implementation for message routing and management
 */

#include "qcc74x_core.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "mr_frame_buff_ctrl.h"
#include "mr_msg_ctrl.h"
#include "mr_msg_ctrl_stat.h"

#define DBG_TAG "MSG"
#include "log.h"

#define MR_MSG_DEBUG_ENABLE     0
#if     MR_MSG_DEBUG_ENABLE
#define MR_MSG_DEBUG(...)       printf(__VA_ARGS__)
#else
#define MR_MSG_DEBUG(...)
#endif

#if MR_DNLD_PROFILE_ENABLE
mr_dnld_prof_counter_t g_mr_dnld_prof[MR_DNLD_PROF_MAX];

void mr_dnld_profile_reset(void)
{
    memset(g_mr_dnld_prof, 0, sizeof(g_mr_dnld_prof));
}

void mr_dnld_profile_dump(void)
{
    static const char *const name[MR_DNLD_PROF_MAX] = {
        [MR_DNLD_PROF_SDIO_TO_MSG] = "sdio_to_msg",
        [MR_DNLD_PROF_MSG_CB] = "msg_cb",
        [MR_DNLD_PROF_MSG_TO_NETDEV] = "msg_to_netdev",
        [MR_DNLD_PROF_NETDEV_OUTPUT] = "netdev_output",
        [MR_DNLD_PROF_NETHUB_SEND] = "nethub_send",
        [MR_DNLD_PROF_FHOST_TX] = "fhost_tx",
    };

    printf("=== Device DNLD profile (cycles) ===\n");
    printf("  %-16s %10s %12s %12s %12s\n",
           "stage", "count", "avg", "max", "total_k");
    for (int i = 0; i < MR_DNLD_PROF_MAX; i++) {
        mr_dnld_prof_counter_t *c = &g_mr_dnld_prof[i];

        if (c->count == 0U) {
            continue;
        }

        printf("  %-16s %10lu %12lu %12lu %12lu\n",
               name[i],
               (unsigned long)c->count,
               (unsigned long)(c->total_cycles / c->count),
               (unsigned long)c->max_cycles,
               (unsigned long)(c->total_cycles / 1000ULL));
    }
    printf("=== End ===\n");
}
#endif
/**
 * @brief Message controller event flags
 * @note Events are accumulated and processed in msg_proc_task
 */
#define MSG_EVENT_DNLD_HW_READY (1 << 0) /**< Hardware ready to receive download */
#define MSG_EVENT_DNLD_RECV     (1 << 1) /**< Download buffer available for receive */
#define MSG_EVENT_DNLD_DONE     (1 << 2) /**< Download complete, ready for callback */

#define MSG_EVENT_UPLD_HW_READY (1 << 3) /**< Hardware ready to send upload */
#define MSG_EVENT_UPLD_SEND     (1 << 4) /**< Upload data available for send */
#define MSG_EVENT_UPLD_DONE     (1 << 5) /**< Upload complete, ready for callback */

#define MSG_EVENT_HW_RESET      (1 << 6) /**< Hardware reset request */

#define MR_MSG_LP_TYPE_MASK    0x03
#define MR_MSG_LP_TYPE_PREPARE 0x01
#define MR_MSG_LP_TYPE_EXIT    0x02
#define MR_MSG_LP_SEQ_SHIFT    2
#define MR_MSG_LP_SEQ_MASK     0x3f
#define MR_MSG_LP_TOKEN(seq, type) \
    ((((seq) & MR_MSG_LP_SEQ_MASK) << MR_MSG_LP_SEQ_SHIFT) | ((type) & MR_MSG_LP_TYPE_MASK))

/**
 * @brief Wake up message processing task with event notification
 * @param msg_ctrl Message controller instance
 * @param event Event flags to notify (bitwise OR of MSG_EVENT_*)
 * @retval 0 Success
 * @retval -1 Error (task not initialized)
 * @note Can be called from ISR or normal context
 */
static int msg_proc_task_wakeup(mr_msg_ctrl_priv_t *msg_ctrl, uint32_t event)
{
    if (msg_ctrl->msg_proc_task == NULL) {
        LOG_E("msg_proc_task is NULL, please init msg_ctrl first!\r\n");
        return -1;
    }

    /* Wake up message processing task */
    if (xPortIsInsideInterrupt()) {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(msg_ctrl->msg_proc_task, event, eSetBits, &pxHigherPriorityTaskWoken);
        if (pxHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotify(msg_ctrl->msg_proc_task, event, eSetBits);
    }

    return 0;
}

static void mr_msg_ctrl_dnld_free_after_cb(mr_frame_elem_t *frame_elem, void *arg)
{
    mr_msg_ctrl_priv_t *msg_ctrl = (mr_msg_ctrl_priv_t *)arg;

    (void)frame_elem;

    if (msg_ctrl != NULL) {
        msg_proc_task_wakeup(msg_ctrl, MSG_EVENT_DNLD_RECV);
    }
}

static int mr_msg_ctrl_dnld_pool_reinit(mr_msg_ctrl_priv_t *msg_ctrl)
{
    if (msg_ctrl == NULL) {
        return -1;
    }

    return mr_frame_queue_pool_reinit(msg_ctrl->dnld_queue_ctrl);
}

static int mr_msg_signal_host(mr_msg_ctrl_priv_t *msg_ctrl, bool notify)
{
    if (msg_ctrl == NULL) {
        return -1;
    }

    mr_lp_gpio_wakeup_notify(&msg_ctrl->lp, notify);
    return 0;
}

static int mr_msg_peer_ready(mr_msg_ctrl_priv_t *msg_ctrl, mr_lp_gpio_ready_cb_t cb, void *arg)
{
    if (msg_ctrl == NULL) {
        return -1;
    }

    return mr_lp_gpio_peer_ready(&msg_ctrl->lp, cb, arg);
}

static int mr_msg_is_busy(mr_msg_ctrl_priv_t *msg_ctrl)
{
    int i;
    int dnld_hw_busy;
    int pool_free;
    int dnld_in_use;
    int dnld_idle_depth;
    mr_msg_ctrl_ops_t *dev_ops;

    if (msg_ctrl == NULL || msg_ctrl->dnld_queue_ctrl == NULL) {
        return 0;
    }

    dev_ops = msg_ctrl->cfg.dev_ops;
    if (dev_ops == NULL) {
        return 0;
    }

    if (dev_ops->dev_dnld_is_busy) {
        dnld_hw_busy = dev_ops->dev_dnld_is_busy();
        if (dnld_hw_busy == 0) {
            MR_MSG_DEBUG("%s %d %d\r\n", __func__, __LINE__, dnld_hw_busy);
            return 1;
        }
    }

    if (dev_ops->dev_upld_is_busy && dev_ops->dev_upld_is_busy()) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    if (dev_ops->dev_upld_pending && dev_ops->dev_upld_pending()) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    pool_free = mr_frame_queue_messages_waiting(msg_ctrl->dnld_queue_ctrl->pool_queue);
    dnld_in_use = msg_ctrl->dnld_queue_ctrl->frame_elem_cnt - pool_free;
    dnld_idle_depth = dev_ops->dev_dnld_queue_depth;
    if (dnld_idle_depth <= 0 || dnld_idle_depth > msg_ctrl->dnld_queue_ctrl->frame_elem_cnt) {
        dnld_idle_depth = msg_ctrl->dnld_queue_ctrl->frame_elem_cnt;
    }
    if (dnld_in_use != dnld_idle_depth) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    if (mr_frame_queue_messages_waiting(msg_ctrl->upld_wait_queue) > 0) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    if (mr_frame_queue_messages_waiting(msg_ctrl->upld_done_queue) > 0) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    if (mr_frame_queue_messages_waiting(msg_ctrl->dnld_done_queue) > 0) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    for (i = 0; i < MR_MSG_TAG_MAX; i++) {
        if (msg_ctrl->mr_msg_is_busy_cb[i] != NULL &&
            msg_ctrl->mr_msg_is_busy_cb[i](msg_ctrl->mr_msg_dnld_recv_arg[i])) {
            MR_MSG_DEBUG("%s %d %d\r\n", __func__, __LINE__, i);
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************
 * Message Transfer Interrupt Callbacks (mr_msg_*)
 * @note Called from HW ISR context
 *****************************************************************************/

/**
 * @brief Host hardware reset interrupt callback
 * @param msg_ctrl Message controller instance
 * @retval 0 Success
 * @retval -1 Error
 */
int mr_msg_host_reset_cb(mr_msg_ctrl_priv_t *msg_ctrl)
{
    LOG_D("msg host reset!\r\n");

    /* Wake up message processing task */
    msg_proc_task_wakeup(msg_ctrl, MSG_EVENT_HW_RESET);

    return 0;
}

/**
 * @brief Upload send complete interrupt callback
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element that was sent
 * @param success Transfer success flag
 * @retval 0 Always returns 0
 * @note Called from HW ISR context
 * @note If tag valid, enqueues to upld_done_queue for callback processing with success flag preserved
 * @note If tag invalid, frees the frame element immediately
 */
int mr_msg_upld_send_done_cb(mr_msg_ctrl_priv_t *msg_ctrl, mr_frame_elem_t *frame_elem, bool success)
{
    int ret = 0;
    mr_msg_t *msg_pkt = (mr_msg_t *)frame_elem->buff_addr;
    uint16_t data_size;

    if (msg_pkt->tag >= MR_MSG_TAG_MAX) {
        LOG_E("msg upld send err: tag 0x%02X is invalid, drop it!\r\n", msg_pkt->tag);
        goto err_exit;
    }

    if (success) {
        MR_MSG_STREAM_STAT_TX_UPDATE(msg_ctrl, frame_elem);
    }

    if (success == false) {
        data_size = frame_elem->data_size;
        frame_elem->data_size = (uint16_t)~data_size;
    }

    /* Send to upld done queue */
    ret = mr_frame_queue_send(msg_ctrl->upld_done_queue, &frame_elem, 0);
    if (ret < 0) {
        LOG_E("Failed to send frame to upload done queue\r\n");
        goto err_exit;
    }

    msg_proc_task_wakeup(msg_ctrl, MSG_EVENT_UPLD_DONE | MSG_EVENT_UPLD_HW_READY);
    return 0;

err_exit:
    /* Free to pool_queue of message controller */
    mr_frame_queue_free_elem(frame_elem);
    return 0;
}

/**
 * @brief Download receive complete interrupt callback
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element that was received
 * @param success Transfer success flag
 * @retval 0 Always returns 0
 * @note Called from HW ISR context
 * @note Validates tag and data size before enqueueing
 * @note If successful and valid, enqueues to dnld_done_queue for callback processing
 * @note If failed or invalid, frees the frame element immediately
 */
int mr_msg_dnld_recv_done_cb(mr_msg_ctrl_priv_t *msg_ctrl, mr_frame_elem_t *frame_elem, bool success)
{
    int ret = 0;
    mr_msg_t *msg_pkt = (mr_msg_t *)frame_elem->buff_addr;

    if (success == false) {
        goto err_exit;
    }

    if (frame_elem->data_size < sizeof(mr_msg_t)) {
        LOG_E("msg dnld recv err: frame size %d is too small, drop it!\r\n", frame_elem->data_size);
        goto err_exit;
    }

    if (msg_pkt->tag >= MR_MSG_TAG_MAX) {
        LOG_E("msg dnld recv err: tag 0x%02X is invalid, drop it!\r\n", msg_pkt->tag);
        goto err_exit;
    }

    if (frame_elem->data_size < msg_pkt->len + sizeof(mr_msg_t)) {
        LOG_E("msg dnld recv err: frame size (%d < %d) is too small for tag 0x%02X, drop it!\r\n",
              frame_elem->data_size, msg_pkt->len + sizeof(mr_msg_t), msg_pkt->tag);
        goto err_exit;
    }

    MR_MSG_STREAM_STAT_RX_UPDATE(msg_ctrl, frame_elem);
    MR_DNLD_PROF_SET(frame_elem, 0);

    /* Send to message download queue */
    ret = mr_frame_queue_send(msg_ctrl->dnld_done_queue, &frame_elem, 0);
    if (ret < 0) {
        LOG_E("Failed to send frame to download done queue\r\n");
        goto err_exit;
    }

    msg_proc_task_wakeup(msg_ctrl, MSG_EVENT_DNLD_DONE | MSG_EVENT_DNLD_HW_READY);
    return 0;

err_exit:
    /* Free to pool_queue of message controller */
    mr_frame_queue_free_elem(frame_elem);
    return 0;
}

/*****************************************************************************
 * Message Controller Functions
 *****************************************************************************/

/**
 * @brief Allocate download frame buffer from message controller pool
 * @param msg_ctrl Message controller instance
 * @param frame_elem Pointer to receive allocated frame element
 * @param xTicksToWait Timeout in ticks (0 for non-blocking, portMAX_DELAY for infinite)
 * @retval 0 Success
 * @retval -1 Error (allocation failed or invalid parameter)
 * @note Use with caution - not recommended for direct application use in most cases
 * @note Applications should use application-layer allocation APIs instead
 */
int mr_msg_ctrl_dnld_alloc(mr_msg_ctrl_priv_t *msg_ctrl, mr_frame_elem_t **frame_elem, TickType_t xTicksToWait)
{
    int ret;
    *frame_elem = NULL;

    if (msg_ctrl == NULL) {
        LOG_E("msg_ctrl is NULL\r\n");
        return -1;
    }

    /* Allocate element from message pool */
    ret = mr_frame_queue_alloc_elem(msg_ctrl->dnld_queue_ctrl, frame_elem, xTicksToWait);
    if (ret < 0) {
        *frame_elem = NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief Free download frame buffer back to message controller pool
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element to free
 * @retval 0 Success
 * @retval <0 Error (free failed)
 * @note The frame queue after-free callback triggers the next receive cycle
 * @note Called by application layer after processing download data
 */
int mr_msg_ctrl_dnld_free(mr_msg_ctrl_priv_t *msg_ctrl, mr_frame_elem_t *frame_elem)
{
    int ret;

    ret = mr_frame_queue_free_elem(frame_elem);
    if (ret < 0) {
        LOG_E("Failed to free frame element\r\n");
        return ret;
    }

    return 0;
}

/**
 * @brief Send message to upload queue for transmission
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element containing message to send
 * @retval 0 Success (enqueued to upload wait queue)
 * @retval -1 Error (invalid tag, size error, or queue full)
 * @note Validates tag and data size before enqueueing
 * @note Triggers MSG_EVENT_UPLD_SEND event to start transmission
 * @note Caller retains ownership if returns error
 */
int mr_msg_ctrl_upld_send(mr_msg_ctrl_priv_t *msg_ctrl, mr_frame_elem_t *frame_elem)
{
    int ret;
    mr_msg_t *msg_pkt = (mr_msg_t *)frame_elem->buff_addr;

    if (msg_pkt->tag >= MR_MSG_TAG_MAX) {
        LOG_E("msg send error: tag 0x%02X is invalid, drop it!\r\n", msg_pkt->tag);
        return -1;
    }

    if (frame_elem->data_size < msg_pkt->len + sizeof(mr_msg_t)) {
        LOG_E("msg send err: frame size (%d < %d) is too small for tag 0x%02X !\r\n", frame_elem->data_size,
              msg_pkt->len + sizeof(mr_msg_t), msg_pkt->tag);

        return -1;
    }

    ret = mr_frame_queue_send(msg_ctrl->upld_wait_queue, &frame_elem, 0);

    /* Wake up message processing task */
    msg_proc_task_wakeup(msg_ctrl, MSG_EVENT_UPLD_SEND);

    return ret;
}

int mr_msg_lp_enter_prepare(mr_msg_ctrl_priv_t *msg_ctrl)
{
    mr_msg_ctrl_ops_t *dev_ops;
    uint8_t host_status;
    uint8_t next_token;

    if (msg_ctrl == NULL) {
        return 1;
    }

    dev_ops = msg_ctrl->cfg.dev_ops;
    if (dev_ops == NULL || dev_ops->dev_lp_device_status_notify == NULL ||
        dev_ops->dev_lp_host_status_get == NULL) {
        msg_ctrl->lp_state = MR_MSG_LP_IDLE;
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    if (mr_msg_is_busy(msg_ctrl)) {
        msg_ctrl->lp_state = MR_MSG_LP_IDLE;
        return 1;
    }

    switch (msg_ctrl->lp_state) {
        case MR_MSG_LP_IDLE:
            host_status = (uint8_t)dev_ops->dev_lp_host_status_get();
            msg_ctrl->lp_seq = (msg_ctrl->lp_seq + 1) & MR_MSG_LP_SEQ_MASK;
            next_token = MR_MSG_LP_TOKEN(msg_ctrl->lp_seq, MR_MSG_LP_TYPE_PREPARE);
            if (next_token == host_status) {
                msg_ctrl->lp_seq = (msg_ctrl->lp_seq + 1) & MR_MSG_LP_SEQ_MASK;
                next_token = MR_MSG_LP_TOKEN(msg_ctrl->lp_seq, MR_MSG_LP_TYPE_PREPARE);
            }
            msg_ctrl->lp_token = next_token;
            (void)dev_ops->dev_lp_device_status_notify(msg_ctrl->lp_token);
            msg_ctrl->lp_state = MR_MSG_LP_PREPARE_SENT;
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
            return 1;

        case MR_MSG_LP_PREPARE_SENT:
            if ((uint8_t)dev_ops->dev_lp_host_status_get() != msg_ctrl->lp_token) {
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
                return 1;
            }
            msg_ctrl->lp_state = MR_MSG_LP_QUIESCED;
            break;

        case MR_MSG_LP_QUIESCED:
            break;

        default:
            msg_ctrl->lp_state = MR_MSG_LP_IDLE;
            return 1;
    }

    if (mr_msg_is_busy(msg_ctrl)) {
        msg_ctrl->lp_state = MR_MSG_LP_IDLE;
            MR_MSG_DEBUG("%s %d\r\n", __func__, __LINE__);
        return 1;
    }

    return 0;
}

int mr_msg_lp_exit_notify(mr_msg_ctrl_priv_t *msg_ctrl)
{
    mr_msg_ctrl_ops_t *dev_ops;

    if (msg_ctrl == NULL) {
        return -1;
    }

    if (msg_ctrl->lp_state != MR_MSG_LP_QUIESCED) {
        return -1;
    }

    dev_ops = msg_ctrl->cfg.dev_ops;
    if (dev_ops != NULL && dev_ops->dev_lp_device_status_notify != NULL) {
        (void)dev_ops->dev_lp_device_status_notify(MR_MSG_LP_TOKEN(msg_ctrl->lp_seq, MR_MSG_LP_TYPE_EXIT));
    }

    msg_ctrl->lp_state = MR_MSG_LP_IDLE;

    return 0;
}

int mr_msg_lp_exit_resume(mr_msg_ctrl_priv_t *msg_ctrl)
{
    mr_msg_ctrl_ops_t *dev_ops;

    if (msg_ctrl == NULL) {
        return -1;
    }

    (void)mr_msg_ctrl_dnld_pool_reinit(msg_ctrl);

    dev_ops = msg_ctrl->cfg.dev_ops;
    if (dev_ops != NULL && dev_ops->dev_lp_recover != NULL) {
        (void)dev_ops->dev_lp_recover(msg_ctrl);
    }

    (void)mr_msg_peer_ready(msg_ctrl, NULL, NULL);
    (void)mr_msg_lp_exit_notify(msg_ctrl);

    return 0;
}

/**
 * @brief Message controller processing task (daemon thread)
 * @param arg Pointer to mr_msg_ctrl_priv_t instance
 * @note Event-driven processing loop with 7 event types:
 *       - MSG_EVENT_HW_RESET: Hardware reset, restart task
 *       - MSG_EVENT_UPLD_SEND | MSG_EVENT_UPLD_HW_READY: Upload transmission
 *       - MSG_EVENT_DNLD_RECV | MSG_EVENT_DNLD_HW_READY: Download reception
 *       - MSG_EVENT_UPLD_DONE: Upload complete, callback processing
 *       - MSG_EVENT_DNLD_DONE: Download complete, callback processing
 * @note Uses fast cycling (0 timeout) when events remain, otherwise waits task_period_max_ms
 * @note All DONE events are processed via while loops until queues empty
 */
static void msg_proc_task(void *arg)
{
    int ret;
    mr_msg_ctrl_priv_t *msg_ctrl = (mr_msg_ctrl_priv_t *)arg;

    mr_frame_elem_t *frame_elem = NULL;
    mr_msg_t *msg_pkt = NULL;

    mr_msg_cb_t msg_cb = NULL;
    void *msg_arg = NULL;

    uint32_t notified_value = 0;
    uint32_t notified_mask = 0;

    LOG_W("%s msg_proc_task start/restart\r\n", msg_ctrl->cfg.dev_ops->name);

wait_ready:
    /* Wait for hardware device to be ready */
    /* Polled until device initialization complete */
    while (msg_ctrl->cfg.dev_ops->dev_is_ready() < 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    notified_value = MSG_EVENT_DNLD_HW_READY | MSG_EVENT_UPLD_HW_READY;

    LOG_D("%s is_ready\r\n", msg_ctrl->cfg.dev_ops->name);

    while (1) {
        /* Wait for event notification or timeout */
        /* Use 0 timeout if events pending (fast cycling), otherwise wait task_period_max_ms */
        uint32_t notified_new = 0;
        if (notified_value) {
            xTaskNotifyWait(0, 0xffffffff, &notified_new, pdMS_TO_TICKS(0));
        } else {
            xTaskNotifyWait(0, 0xffffffff, &notified_new, pdMS_TO_TICKS(msg_ctrl->cfg.task_period_max_ms));
        }
        notified_value |= notified_new; /* Accumulate events for batch processing */

        /* Optional application task callback */
        if (msg_ctrl->cfg.msg_task_cb) {
            msg_ctrl->cfg.msg_task_cb(msg_ctrl, &notified_value);
        }

        /* Process hardware reset event */
        /* Calls all registered reset callbacks and restarts task */
        notified_mask = MSG_EVENT_HW_RESET;
        if (notified_value & notified_mask) {
            notified_value &= ~notified_mask;
            LOG_D("%s processing reset request \r\n", msg_ctrl->cfg.dev_ops->name);

            /* Call application layer reset callbacks */
            for (int i = 0; i < MR_MSG_TAG_MAX; i++) {
                if (msg_ctrl->mr_msg_hw_reset_cb[i] != NULL) {
                    msg_ctrl->mr_msg_hw_reset_cb[i](NULL, msg_ctrl->mr_msg_hw_reset_arg[i]);
                }
            }
            goto wait_ready;
        }

        /* Process upload send event (upload data to hardware) */
        /* Requires both UPLD_SEND and UPLD_HW_READY flags */
        notified_mask = MSG_EVENT_UPLD_SEND | MSG_EVENT_UPLD_HW_READY;
        while (notified_value & notified_mask) {
            /* Check if hardware ready for upload */
            if (msg_ctrl->cfg.dev_ops->dev_upld_is_busy()) {
                /* Hardware still busy, defer to next cycle */
                notified_value &= ~notified_mask;
                break;
            }
            /* Get message from upload wait queue (non-blocking) */
            ret = mr_frame_queue_receive(msg_ctrl->upld_wait_queue, &frame_elem, 0);
            if (ret < 0) {
                /* queue empty */
                notified_value &= ~notified_mask;
                break;
            } else {
                ret = msg_ctrl->cfg.dev_ops->dev_upld_start(frame_elem);
                if (ret < 0) {
                    LOG_W("%s upld start failed, ret: %d\r\n", msg_ctrl->cfg.dev_ops->name, ret);
                    mr_msg_upld_send_done_cb(msg_ctrl, frame_elem, false);
                }
            }
        }

        /* Process download receive event (receive data from hardware) */
        /* Requires both DNLD_RECV and DNLD_HW_READY flags */
        notified_mask = MSG_EVENT_DNLD_RECV | MSG_EVENT_DNLD_HW_READY;
        while (notified_value & notified_mask) {
            /* Check if hardware ready for download */
            if (msg_ctrl->cfg.dev_ops->dev_dnld_is_busy()) {
                /* Hardware still busy, defer to next cycle */
                notified_value &= ~notified_mask;
                break;
            }
            /* Allocate buffer from download pool (non-blocking) */
            ret = mr_frame_queue_alloc_elem(msg_ctrl->dnld_queue_ctrl, &frame_elem, 0);
            if (ret < 0) {
                /* no free dnld buffer, wait next time */
                notified_value &= ~notified_mask;
                break;
            } else {
                ret = msg_ctrl->cfg.dev_ops->dev_dnld_start(frame_elem);
                if (ret < 0) {
                    LOG_E("%s dnld start failed, ret: %d\r\n", msg_ctrl->cfg.dev_ops->name, ret);
                    mr_frame_queue_free_elem(frame_elem);
                }
            }
        }

        /* Process upload complete event (hardware send done callback) */
        /* Loop processes all packets in upld_done_queue */
        notified_mask = MSG_EVENT_UPLD_DONE;
        while (notified_value & notified_mask) {
            ret = mr_frame_queue_receive(msg_ctrl->upld_done_queue, &frame_elem, 0);
            if (ret < 0) {
                /* Queue empty, clear event flag */
                notified_value &= ~notified_mask;
            } else {
                bool success = true;
                msg_pkt = (mr_msg_t *)frame_elem->buff_addr;
                mr_msg_upld_cb_t upld_cb = msg_ctrl->mr_msg_upld_send_cb[msg_pkt->tag];
                msg_arg = msg_ctrl->mr_msg_upld_send_arg[msg_pkt->tag];
                if (frame_elem->data_size != msg_pkt->len + sizeof(mr_msg_t)) {
                    success = false;
                    frame_elem->data_size = (uint16_t)~frame_elem->data_size;
                }
                if (upld_cb != NULL) {
                    ret = upld_cb(frame_elem, msg_arg, success);
                    if (ret < 0) {
                        LOG_E("msg upld send err: tag 0x%02X callback failed, drop it!\r\n", msg_pkt->tag);
                        mr_frame_queue_free_elem(frame_elem);
                    }
                } else {
                    mr_frame_queue_free_elem(frame_elem);
                }
            }
        }

        /* Process download complete event (hardware receive done callback) */
        /* Loop processes all packets in dnld_done_queue */
        notified_mask = MSG_EVENT_DNLD_DONE;
        while (notified_value & notified_mask) {
            ret = mr_frame_queue_receive(msg_ctrl->dnld_done_queue, &frame_elem, 0);
            if (ret < 0) {
                /* Queue empty, clear event flag */
                notified_value &= ~notified_mask;
            } else {
                msg_pkt = (mr_msg_t *)frame_elem->buff_addr;
                msg_cb = msg_ctrl->mr_msg_dnld_recv_cb[msg_pkt->tag];
                msg_arg = msg_ctrl->mr_msg_dnld_recv_arg[msg_pkt->tag];
                if (msg_cb != NULL) {
                    /* Call the registered callback */
                    MR_DNLD_PROF_FROM(MR_DNLD_PROF_SDIO_TO_MSG, frame_elem, 0);
                    MR_DNLD_PROF_BEGIN(msg_cb_cycles);
                    ret = msg_cb(frame_elem, msg_arg);
                    MR_DNLD_PROF_END(MR_DNLD_PROF_MSG_CB, msg_cb_cycles);
                    if (ret < 0) {
                        LOG_E("msg dnld recv err: tag 0x%02X callback failed, drop it!\r\n", msg_pkt->tag);
                        mr_frame_queue_free_elem(frame_elem);
                    }
                } else {
                    mr_frame_queue_free_elem(frame_elem);
                }
            }
        }

        /* Detect unknown/unexpected event flags */
        /* All known events should be cleared by their handlers */
        notified_mask = ~(0);
        if (notified_value & notified_mask) {
            LOG_W("%s unknown event: 0x%08x\r\n", msg_ctrl->cfg.dev_ops->name, (notified_value & notified_mask));
            notified_value &= ~notified_mask;
        }
    }
}

/*****************************************************************************
 * Public API Implementation
 * @note These functions can be called by upper layer applications
 *****************************************************************************/

/**
 * @brief Initialize message controller instance
 * @param cfg Message controller configuration
 * @retval msg_ctrl Initialized message controller instance
 * @retval NULL Initialization failed (check logs for details)
 * @note Creates frame buffers, queues, and processing task
 * @note Validates all configuration parameters before initialization
 * @note Hardware device initialized via dev_ops->dev_init callback
 */
mr_msg_ctrl_priv_t *mr_msg_ctrl_init(mr_msg_ctrl_cfg_t *cfg)
{
    int ret;
    mr_msg_ctrl_priv_t *msg_ctrl;

    /* Parameter validation */
    if (cfg == NULL || cfg->dnld_frame_size <= sizeof(mr_msg_t) || cfg->dnld_frame_buff == NULL ||
        cfg->upld_queue_depth == 0 || cfg->dev_ops == NULL || cfg->dev_ops->dev_init == NULL ||
        cfg->dev_ops->dev_is_ready == NULL || cfg->dev_ops->dev_upld_is_busy == NULL ||
        cfg->dev_ops->dev_dnld_is_busy == NULL || cfg->dev_ops->dev_upld_start == NULL ||
        cfg->dev_ops->dev_dnld_start == NULL) {
        LOG_E("Invalid parameters for msg_ctrl init\r\n");
        return NULL;
    }

    msg_ctrl = malloc(sizeof(mr_msg_ctrl_priv_t));
    if (!msg_ctrl) {
        LOG_E("msg_ctrl malloc failed\r\n");
        return NULL;
    }
    memset(msg_ctrl, 0, sizeof(mr_msg_ctrl_priv_t));

    msg_ctrl->cfg = *cfg;
    mr_lp_gpio_init(&msg_ctrl->lp, cfg->lp_cfg);

    /* Create download frame queue controller */
    mr_frame_queue_ctrl_init_cfg_t dnld_frame_cfg = {
        .frame_buff = msg_ctrl->cfg.dnld_frame_buff,
        .frame_type = msg_ctrl->cfg.dnld_frame_type,
        .frame_elem_cnt = msg_ctrl->cfg.dnld_frame_count,
        .frame_elem_size = msg_ctrl->cfg.dnld_frame_size,
        .name = msg_ctrl->cfg.name
    };
    msg_ctrl->dnld_queue_ctrl = mr_frame_queue_create(&dnld_frame_cfg);
    if (msg_ctrl->dnld_queue_ctrl == NULL) {
        LOG_E("Failed to create download queue controller\r\n");
        return NULL;
    }
    mr_frame_queue_free_cb_register(msg_ctrl->dnld_queue_ctrl,
                                    NULL, NULL,
                                    mr_msg_ctrl_dnld_free_after_cb, msg_ctrl);

    /* Create queues */
    msg_ctrl->dnld_done_queue = xQueueCreate(msg_ctrl->cfg.dnld_frame_count, sizeof(void *));
    if (msg_ctrl->dnld_done_queue == NULL) {
        LOG_E("%s Failed to create msg download done queue\r\n", msg_ctrl->cfg.name);
        goto error_exit;
    }
    msg_ctrl->upld_wait_queue = xQueueCreate(msg_ctrl->cfg.upld_queue_depth, sizeof(void *));
    if (msg_ctrl->upld_wait_queue == NULL) {
        LOG_E("%s Failed to create msg upload wait queue\r\n", msg_ctrl->cfg.name);
        goto error_exit;
    }
    msg_ctrl->upld_done_queue = xQueueCreate(msg_ctrl->cfg.upld_queue_depth, sizeof(void *));
    if (msg_ctrl->upld_done_queue == NULL) {
        LOG_E("%s Failed to create msg upload done queue\r\n", msg_ctrl->cfg.name);
        goto error_exit;
    }

    mr_msg_peer_ready(msg_ctrl, NULL, NULL);

    /* Device initialization */
    ret = msg_ctrl->cfg.dev_ops->dev_init(msg_ctrl);
    if (ret != 0) {
        LOG_E("%s Device initialization failed\r\n", msg_ctrl->cfg.name);
        goto error_exit;
    }

    /* Create daemon thread */
    ret = xTaskCreate(msg_proc_task, msg_ctrl->cfg.name, msg_ctrl->cfg.task_stack_size, msg_ctrl,
                      msg_ctrl->cfg.task_priority, &msg_ctrl->msg_proc_task);
    if (ret != pdPASS) {
        LOG_E("%s Failed to create msg processing task\r\n", msg_ctrl->cfg.name);
        goto error_exit;
    }

    return msg_ctrl;

error_exit:
    if (msg_ctrl->dnld_done_queue) {
        vQueueDelete(msg_ctrl->dnld_done_queue);
    }
    if (msg_ctrl->upld_wait_queue) {
        vQueueDelete(msg_ctrl->upld_wait_queue);
    }
    if (msg_ctrl->upld_done_queue) {
        vQueueDelete(msg_ctrl->upld_done_queue);
    }
    if (msg_ctrl->dnld_queue_ctrl) {
        mr_frame_queue_remove(msg_ctrl->dnld_queue_ctrl);
    }
    free(msg_ctrl);
    LOG_I("%s device initialized failed\r\n", cfg->name);
    return NULL;
}

/**
 * @brief Deinitialize and cleanup message controller
 * @param msg_ctrl Message controller instance to deinitialize
 * @retval 0 Success
 * @retval -1 Error (invalid parameter)
 * @note Deletes processing task, queues, and frame buffers
 * @note Frees all allocated resources
 */
int mr_msg_ctrl_deinit(mr_msg_ctrl_priv_t *msg_ctrl)
{
    if (msg_ctrl == NULL) {
        LOG_E("msg_ctrl is NULL\r\n");
        return -1;
    }

    /* Delete message processing task */
    vTaskDelete(msg_ctrl->msg_proc_task);
    msg_ctrl->msg_proc_task = NULL;

    /* Delete queues */
    vQueueDelete(msg_ctrl->dnld_done_queue);
    vQueueDelete(msg_ctrl->upld_wait_queue);
    vQueueDelete(msg_ctrl->upld_done_queue);

    /* Delete download queue controller */
    mr_frame_queue_remove(msg_ctrl->dnld_queue_ctrl);

    free(msg_ctrl);
    return 0;
}

/**
 * @brief Register application callbacks for message tag
 * @param msg_ctrl Message controller instance
 * @param tag Message tag (0 to MR_MSG_TAG_MAX-1)
 * @param dnld_recv_cb Download receive callback (NULL to skip registration)
 * @param dnld_recv_arg Download receive callback argument
 * @param upld_send_cb Upload send done callback (NULL to skip registration)
 * @param upld_send_arg Upload send done callback argument
 * @param hw_reset_cb Hardware reset callback (NULL to skip registration)
 * @param hw_reset_arg Hardware reset callback argument
 * @retval 0 Success
 * @retval -1 Error (invalid parameter)
 * @note Callback return value semantics: 0=ownership transferred, <0=caller frees
 * @note Protected by IRQ disable for atomic registration
 */
int mr_msg_cb_register(mr_msg_ctrl_priv_t *msg_ctrl, uint8_t tag, mr_msg_cb_t dnld_recv_cb, void *dnld_recv_arg,
                       mr_msg_upld_cb_t upld_send_cb, void *upld_send_arg, mr_msg_cb_t hw_reset_cb, void *hw_reset_arg,
                       mr_msg_is_busy_cb_t is_busy_cb)
{
    uintptr_t flag;

    if (msg_ctrl == NULL) {
        LOG_E("Invalid msg_ctrl for cb register\r\n");
        return -1;
    }

    if (tag >= MR_MSG_TAG_MAX) {
        LOG_E("Invalid msg tag: 0x%02X\r\n", tag);
        return -1;
    }

    flag = qcc74x_irq_save();
    /* Register download receive callback if provided */
    if (dnld_recv_cb != NULL) {
        msg_ctrl->mr_msg_dnld_recv_cb[tag] = dnld_recv_cb;
        msg_ctrl->mr_msg_dnld_recv_arg[tag] = dnld_recv_arg;
    }

    /* Register upload send done callback if provided */
    if (upld_send_cb != NULL) {
        msg_ctrl->mr_msg_upld_send_cb[tag] = upld_send_cb;
        msg_ctrl->mr_msg_upld_send_arg[tag] = upld_send_arg;
    }

    if (hw_reset_cb != NULL) {
        msg_ctrl->mr_msg_hw_reset_cb[tag] = hw_reset_cb;
        msg_ctrl->mr_msg_hw_reset_arg[tag] = hw_reset_arg;
    }

    msg_ctrl->mr_msg_is_busy_cb[tag] = is_busy_cb;
    qcc74x_irq_restore(flag);

    LOG_I("Msg callback registered for tag 0x%02X\r\n", tag);

    return 0;
}

/**
 * @brief Unregister application callbacks for message tag
 * @param msg_ctrl Message controller instance
 * @param tag Message tag (0 to MR_MSG_TAG_MAX-1)
 * @retval 0 Success
 * @retval -1 Error (invalid parameter)
 * @note Clears all callbacks (dnld_recv, upld_send) for the specified tag
 * @note Protected by IRQ disable for atomic unregistration
 */
int mr_msg_cb_unregister(mr_msg_ctrl_priv_t *msg_ctrl, uint8_t tag)
{
    uintptr_t flag;

    if (msg_ctrl == NULL) {
        LOG_E("Invalid msg_ctrl for cb unregister\r\n");
        return -1;
    }

    if (tag >= MR_MSG_TAG_MAX) {
        LOG_E("Invalid msg tag: 0x%02X\r\n", tag);
        return -1;
    }

    flag = qcc74x_irq_save();
    msg_ctrl->mr_msg_dnld_recv_cb[tag] = NULL;
    msg_ctrl->mr_msg_dnld_recv_arg[tag] = NULL;
    msg_ctrl->mr_msg_upld_send_cb[tag] = NULL;
    msg_ctrl->mr_msg_upld_send_arg[tag] = NULL;
    msg_ctrl->mr_msg_hw_reset_cb[tag] = NULL;
    msg_ctrl->mr_msg_hw_reset_arg[tag] = NULL;
    msg_ctrl->mr_msg_is_busy_cb[tag] = NULL;
    qcc74x_irq_restore(flag);

    LOG_I("Msg callbacks unregistered for tag 0x%02X\r\n", tag);
    return 0;
}
