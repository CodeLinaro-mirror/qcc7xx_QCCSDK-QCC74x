/**
 * @file buffer_mgmt_ctrl.c
 * @brief Frame buffer controller implementation for memory pool management
 * @author mlwang
 * @date 2025-07-17
 */

#include <string.h>
#include "buffer_mgmt_ctrl.h"

#define DBG_TAG "BF_CTRL"
#include "log.h"

/**
 * @brief Simple flexible memory block management
 *
 * Each memory block header saves descriptor information.
 * Uses OS-abstracted Queue for memory block pool management.
 */

/***************** buffer mgmt ctrl *****************/

/**
 * @brief Create buffer management controller
 * @param cfg Configuration structure
 * @retval Pointer to buffer management controller on success
 * @retval NULL on failure
 */
buffer_mgmt_ctrl_t *buffer_mgmt_create(buffer_mgmt_ctrl_init_cfg_t *cfg)
{
    buffer_mgmt_elem_t *elem_head;

    if (!cfg || !cfg->frame_buff || !cfg->frame_elem_cnt ||
        cfg->frame_elem_size < FRAME_BUFF_PAYLOAD_OFFSET) {
        LOG_ERR("invalid buffer mgmt cfg");
        return NULL;
    }

    buffer_mgmt_ctrl_t *ctrl = rcp_os_malloc(sizeof(buffer_mgmt_ctrl_t));
    if (!ctrl) {
        LOG_ERR("malloc ctrl failed");
        return NULL;
    }
    memset(ctrl, 0, sizeof(buffer_mgmt_ctrl_t));

    /* Create pool queue (memory block pool management) */
    if (!rcp_os_queue_init(&ctrl->pool_queue, sizeof(uintptr_t), cfg->frame_elem_cnt)) {
        LOG_ERR("create pool queue failed");
        goto err_free_ctrl;
    }

    for (int i = 0; i < cfg->frame_elem_cnt; i++) {
        elem_head = (buffer_mgmt_elem_t *)((uintptr_t)cfg->frame_buff + cfg->frame_elem_size * i);

        elem_head->buffer_ctrl = ctrl;
        elem_head->buff_addr = (uint8_t *)elem_head;
        elem_head->offset = FRAME_BUFF_PAYLOAD_OFFSET;
        elem_head->data_size = 0;
        elem_head->max_size = cfg->frame_elem_size;

        if (rcp_os_queue_send(&ctrl->pool_queue, &elem_head, 0) != 0) {
            LOG_ERR("send to pool failed at index %d", i);
            goto err_deinit_pool;
        }
    }

    ctrl->name = cfg->name;

    LOG_INF("buffer_mgmt_create %s", ctrl->name);
    return ctrl;

err_deinit_pool:
    rcp_os_queue_deinit(&ctrl->pool_queue);
err_free_ctrl:
    rcp_os_free(ctrl);
    return NULL;
}

/**
 * @brief Remove buffer management controller
 * @param ctrl Buffer management controller
 * @retval 0 Success
 */
int buffer_mgmt_remove(buffer_mgmt_ctrl_t *ctrl)
{
    if (!ctrl) {
        return -1;
    }

    rcp_os_queue_deinit(&ctrl->pool_queue);

    LOG_INF("buffer_mgmt_remove %s", ctrl->name);

    rcp_os_free(ctrl);

    return 0;
}

/**
 * @brief Get available elements in the pool
 * @param ctrl Buffer management controller
 * @retval Number of available elements
 */
int buffer_mgmt_pool_get_available(buffer_mgmt_ctrl_t *ctrl)
{
    if (!ctrl) {
        return 0;
    }
    return rcp_os_queue_messages_waiting(&ctrl->pool_queue);
}

/**
 * @brief Register buffer management free callbacks
 * @param ctrl Buffer management controller
 * @param before_free_cb Before free callback
 * @param before_free_cb_arg Before free callback argument
 * @param after_free_cb After free callback
 * @param after_free_cb_arg After free callback argument
 * @retval 0 Success
 * @retval -1 Error
 */
int buffer_mgmt_free_cb_register(buffer_mgmt_ctrl_t *ctrl,
                                 buffer_mgmt_free_cb_t before_free_cb,
                                 void *before_free_cb_arg,
                                 buffer_mgmt_free_cb_t after_free_cb,
                                 void *after_free_cb_arg)
{
    if (!ctrl) {
        return -1;
    }

    ctrl->before_free_cb = before_free_cb;
    ctrl->before_free_cb_arg = before_free_cb_arg;
    ctrl->after_free_cb = after_free_cb;
    ctrl->after_free_cb_arg = after_free_cb_arg;

    return 0;
}

/**
 * @brief Allocate buffer element from pool
 * @param ctrl Buffer management controller
 * @param buffer_elem Pointer to store allocated buffer element
 * @param timeout_ms Timeout in milliseconds
 * @retval 0 Success
 * @retval -1 Error
 */
int buffer_mgmt_alloc_elem(buffer_mgmt_ctrl_t *ctrl,
                           buffer_mgmt_elem_t **buffer_elem,
                           uint32_t timeout_ms)
{
    if (!ctrl || !buffer_elem) {
        return -1;
    }

    int ret = rcp_os_queue_receive(&ctrl->pool_queue, buffer_elem, timeout_ms);

    if (ret == 0) {
    	LOG_DBG("alloc elem:%p, buff_addr:%p\r\n", (*buffer_elem), (*buffer_elem)->buff_addr);
    }

    return ret;
}

/**
 * @brief Free buffer element back to pool
 * @param buffer_elem Buffer element to free
 * @retval 0 Success
 * @retval -1 Error
 */
int buffer_mgmt_free_elem(buffer_mgmt_elem_t *buffer_elem)
{
    if (!buffer_elem || !buffer_elem->buff_addr || !buffer_elem->buffer_ctrl) {
        LOG_ERR("invalid elem");
        return -1;
    }

    buffer_mgmt_ctrl_t *ctrl = buffer_elem->buffer_ctrl;

    LOG_DBG("free elem:%p, buff_addr:%p\r\n", buffer_elem, buffer_elem->buff_addr);

    if ((uintptr_t)buffer_elem != (uintptr_t)buffer_elem->buff_addr) {
        LOG_ERR("elem addr mismatch: elem:%p, buff:%p\r\n",
              buffer_elem, buffer_elem->buff_addr);
        return -1;
    }

    if (ctrl->before_free_cb) {
        ctrl->before_free_cb(buffer_elem, ctrl->before_free_cb_arg);
    }

    buffer_elem->data_size = 0;
    buffer_elem->offset = FRAME_BUFF_PAYLOAD_OFFSET;
    int ret = rcp_os_queue_send(&ctrl->pool_queue, &buffer_elem, 0);
    if (ret != 0) {
        LOG_ERR("send to pool failed, ret:%d", ret);
        return -1;
    }

    if (ctrl->after_free_cb) {
        ctrl->after_free_cb(buffer_elem, ctrl->after_free_cb_arg);
    }

    return 0;
}
