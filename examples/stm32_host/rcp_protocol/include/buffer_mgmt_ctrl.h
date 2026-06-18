/**
 * @file buffer_mgmt_ctrl.h
 * @brief Frame buffer controller header file for memory pool management
 * @author mlwang
 * @date 2025-07-17
 */

#ifndef __STREAM_QUEUE_CTRL_H__
#define __STREAM_QUEUE_CTRL_H__

#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "rcp_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Macro Definitions
 *****************************************************************************/

/** Frame buffer header room size in bytes */
#define FRAME_BUFF_DMA_ALIGN        (32U)
#define FRAME_BUFF_ELEM_SIZE        (24U)
#define FRAME_BUFF_PBUFCUSTOM       (24U)
#define FRAME_BUFF_HWDESCTAG        (0U)
#define FRAME_BUFF_MSGSTRUCT_OFFSET (FRAME_BUFF_ELEM_SIZE + FRAME_BUFF_PBUFCUSTOM + FRAME_BUFF_HWDESCTAG)
#define FRAME_BUFF_MSGSTRUCT        (8U)
#define FRAME_BUFF_PAYLOAD_OFFSET   (FRAME_BUFF_MSGSTRUCT_OFFSET + FRAME_BUFF_MSGSTRUCT)
/* 1536 is the whole raw RCP packet length, not just payload. */
#define FRAME_BUFF_RCP_PACKET_LEN   (1536U)
#define FRAME_BUFF_PAYLOAD          (FRAME_BUFF_RCP_PACKET_LEN - FRAME_BUFF_MSGSTRUCT)
#define FRAME_BUFF_TOTALSIZE        (FRAME_BUFF_MSGSTRUCT_OFFSET + FRAME_BUFF_RCP_PACKET_LEN)
#define FRAME_BUFF_DMA_MEM_ATTR     __attribute__((section("noncacheable_buffer"), aligned(FRAME_BUFF_DMA_ALIGN)))

/*****************************************************************************
 * Type Definitions
 *****************************************************************************/

typedef struct buffer_mgmt_elem_t buffer_mgmt_elem_t;
typedef struct buffer_mgmt_ctrl_t buffer_mgmt_ctrl_t;

/**
 * @brief Buffer management free callback function type
 * @param buffer_elem Buffer element being freed
 * @param arg Callback argument
 * @retval None
 */
typedef void (*buffer_mgmt_free_cb_t)(buffer_mgmt_elem_t *buffer_elem, void *arg);

/**
 * @brief Buffer management element structure
 * @note sizeof(buffer_mgmt_elem_t) must be < FRAME_BUFF_PAYLOAD_OFFSET
 */
struct buffer_mgmt_elem_t {
    /**** Fields that cannot be modified ****/
    buffer_mgmt_ctrl_t *buffer_ctrl;        /**< Buffer controller pointer */

    void *buff_addr;                        /**< Buffer address */
    uint32_t offset;
    buffer_mgmt_free_cb_t free_cb;   /**< Per-element before free callback */
    void *cb_arg;                    /**< Per-element before free callback argument */

    /**** Fields that can be modified ****/
    uint16_t data_size;                     /**< Actual data size */
    uint16_t max_size;
    uint8_t ex_data[0];                     /**< Extra data for custom use */
};

/**
 * @brief Buffer management controller structure
 */
struct buffer_mgmt_ctrl_t {
    rcp_queue_t pool_queue;                 /**< Pool queue for memory block management */
    char *name;                             /**< Controller name */
    uint16_t frame_data_offset;             /**< Default data offset restored on free */
    uint16_t frame_elem_cnt;                /**< Number of elements owned by this pool */
};

/**
 * @brief Buffer management controller initialization configuration
 */
typedef struct {
    void *frame_buff;                       /**< Frame buffer pointer */
    uint16_t frame_elem_cnt;                /**< Frame element count */
    uint16_t frame_elem_size;               /**< Frame element size */
    uint16_t frame_data_offset;             /**< Default data offset for fresh buffers */

    char *name;                             /**< Controller name */
} buffer_mgmt_ctrl_init_cfg_t;

/*****************************************************************************
 * Function Declarations
 *****************************************************************************/

#define BUFFER_MGMT_DATA_PTR(buffer) ((uint8_t *)(buffer)->buff_addr + (buffer)->offset)
#define BUFFER_MGMT_DATA_SIZE(buffer) (buffer->data_size)
#define BUFFER_MGMT_MAX_SIZE(buffer) ((buffer->offset > buffer->max_size) \
		? 0 : (buffer->max_size - buffer->offset))

static inline bool buffer_mgmt_write(buffer_mgmt_elem_t *buffer_elem, const void *data, uint16_t len)
{
	if (!buffer_elem || !data || len > BUFFER_MGMT_MAX_SIZE(buffer_elem)) {
		return false;
	}

	memcpy(BUFFER_MGMT_DATA_PTR(buffer_elem), data, len);
	buffer_elem->data_size = len;
	return true;
}

static inline uint16_t buffer_mgmt_read(buffer_mgmt_elem_t *buffer_elem, void *data, uint16_t len)
{
	uint16_t xfr_len;

	if (!buffer_elem || !data) {
		return 0;
	}

	xfr_len = buffer_elem->data_size < len ? buffer_elem->data_size : len;
	memcpy(data, BUFFER_MGMT_DATA_PTR(buffer_elem), xfr_len);
	return xfr_len;
}

static inline void *buffer_mgmt_add_header_impl(buffer_mgmt_elem_t *buffer_elem, uint32_t offset)
{
	if (buffer_elem->offset < offset) {
		return NULL;
	}
	buffer_elem->offset -= offset;
	buffer_elem->data_size += offset;
	return (uint8_t *)buffer_elem->buff_addr + buffer_elem->offset;
}

static inline bool buffer_mgmt_consume_data(buffer_mgmt_elem_t *buffer_elem, uint32_t len)
{
	if (!buffer_elem || len > buffer_elem->data_size ||
	    (buffer_elem->offset + len) > buffer_elem->max_size) {
		return false;
	}

	buffer_elem->offset += len;
	buffer_elem->data_size -= len;
	return true;
}

static inline void buffer_mgmt_elem_set_free_cb(buffer_mgmt_elem_t *buffer_elem,
						buffer_mgmt_free_cb_t free_cb,
						void *free_cb_arg)
{
	if (!buffer_elem) {
		return;
	}

	buffer_elem->free_cb = free_cb;
	buffer_elem->cb_arg = free_cb_arg;
}

/* Compatibility aliases for legacy lowercase helper names. */
#define buffer_mgmt_data_ptr(buffer) ((uint8_t *)BUFFER_MGMT_DATA_PTR(buffer))
#define buffer_mgmt_data_ptr_const(buffer) ((const uint8_t *)BUFFER_MGMT_DATA_PTR(buffer))
#define buffer_mgmt_data_size(buffer) BUFFER_MGMT_DATA_SIZE(buffer)
#define buffer_mgmt_max_size(buffer) BUFFER_MGMT_MAX_SIZE(buffer)

/**
 * @brief Create buffer management controller
 * @param cfg Configuration structure
 * @retval Pointer to buffer management controller on success
 * @retval NULL on failure
 */
buffer_mgmt_ctrl_t *buffer_mgmt_create(buffer_mgmt_ctrl_init_cfg_t *cfg);

/**
 * @brief Remove buffer management controller
 * @param ctrl Buffer management controller
 * @retval 0 Success
 * @retval -1 Error
 */
int buffer_mgmt_remove(buffer_mgmt_ctrl_t *ctrl);

/**
 * @brief Get available elements in the pool
 * @param ctrl Buffer management controller
 * @retval Number of available elements
 */
int buffer_mgmt_pool_get_available(buffer_mgmt_ctrl_t *ctrl);

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
                           uint32_t timeout_ms);

/**
 * @brief Free buffer element back to pool
 * @param buffer_elem Buffer element to free
 * @retval 0 Success
 * @retval -1 Error
 */
int buffer_mgmt_free_elem(buffer_mgmt_elem_t *buffer_elem);

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_QUEUE_CTRL_H__ */
