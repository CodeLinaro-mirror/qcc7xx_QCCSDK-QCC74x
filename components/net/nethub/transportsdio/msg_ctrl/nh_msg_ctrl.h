/**
 * @file nh_msg_ctrl.h
 * @brief Message controller header file for SDIO message routing and management
 * @author mlwang
 * @date 2025-07-17
 */

#ifndef __nh_MSG_CTRL___
#define __nh_MSG_CTRL___

#include "qcc74x_core.h"
#include "nh_frame_buff_ctrl.h"

/*****************************************************************************
 * Macro Definitions
 *****************************************************************************/

/** Download frame type identifier */
#define nh_MSG_CTRL_DNLD_FRAME_TYPE  (0x01)
/** Number of download frame buffers */
#define nh_MSG_CTRL_DNLD_FRAME_NUM   (24)
#define nh_MSG_CTRL_DNLD_FRAME_SIZE  FRAME_BUFF_TOTALSIZE

/** Upload queue depth */
#define nh_MSG_CTRL_UPLD_QUEUE_DEPTH (32)// todo: sync with rx count, fixme

/*****************************************************************************
 * Type Definitions
 *****************************************************************************/

/**
 * @brief Message tags for routing different message types
 */
enum {
    nh_MSG_TAG_NONE = 0, /**< No message tag */

    nh_MSG_TAG_TEST_1, /**< Test message tag 1 */
    nh_MSG_TAG_TEST_2, /**< Test message tag 2 */
    nh_MSG_TAG_TEST_3, /**< Test message tag 3 */

    nh_MSG_TAG_SYS,  /**< System message tag */
    nh_MSG_TAG_WIFI, /**< WiFi message tag */

    nh_MSG_TAG_ETH, /**< Ethernet message tag */

    nh_MSG_TAG_TTY, /**< TTY message tag */

    nh_MSG_TAG_NETLINK, /**< Netlink message tag */

    nh_MSG_TAG_HCI, /**< HCI message tag */

    nh_MSG_TAG_MAX /**< Maximum message tag */
};

#pragma pack(push, 1)

/**
 * @brief Message packet header structure
 */
typedef struct nh_msg_packt {
    uint8_t tag;     /**< Message tag for routing */
    uint8_t sub_tag; /**< Message sub-tag */
    uint16_t len;    /**< Message data length */
    uint8_t data[0]; /**< Message data payload */
} nh_msg_packt_t;

#pragma pack(pop)

/**
 * @brief Message callback function type for download receive and hardware reset
 * @param frame_elem Frame element
 * @param arg Callback argument
 * @retval 0 Success
 * @retval -1 Error
 */
typedef int (*nh_msg_cb_t)(frame_elem_t *frame_elem, void *arg);

/**
 * @brief Message callback function type for upload send done
 * @param frame_elem Frame element
 * @param arg Callback argument
 * @param success Upload success flag
 * @retval 0 Success
 * @retval -1 Error
 */
typedef int (*nh_msg_upld_cb_t)(frame_elem_t *frame_elem, void *arg, bool success);

/**
 * @brief Message controller operations structure for SDIO interaction
 */
typedef struct
{
    struct nh_device_s *sdio_hd;
    int (*sdio_is_ready)(void);
    int (*sdio_upld_is_busy)(void);
    int (*sdio_dnld_is_busy)(void);
    int (*sdio_upld_start)(frame_elem_t *elem, void *ctrl);
    int (*sdio_dnld_start)(frame_elem_t *elem, void *ctrl);
    int (*sdio_feature_control)(struct nh_device_s *dev, int cmd, uintptr_t arg);
    const char *sdio_name;
} nh_msg_ctrl_ops_t;

typedef struct {
    volatile uint32_t tx_data_packets;
    volatile uint32_t tx_data_bytes;
    volatile uint32_t tx_ctrl_packets;
    volatile uint32_t tx_ctrl_bytes;
    volatile uint32_t rx_data_packets;
    volatile uint32_t rx_data_bytes;
    volatile uint32_t rx_ctrl_packets;
    volatile uint32_t rx_ctrl_bytes;
} nh_msg_stream_stat_t;

/**
 * @brief Message controller structure
 */
typedef struct {
    struct nh_device_s *sdio2_hd; /**< SDIO2 device handle */
    volatile bool hw_reset_flag;    /**< Hardware reset flag */

    uint32_t msg_dnld_max_size; /**< Maximum download message size */
    uint32_t msg_upld_max_size; /**< Maximum upload message size */

    QueueHandle_t dnld_queue; /**< Download done queue for callback processing */
    QueueHandle_t upld_queue; /**< Upload queue from upper layer */

    frame_queue_ctrl_t *dnld_queue_ctrl; /**< Download queue controller */

    nh_msg_cb_t nh_msg_dnld_recv_cb[nh_MSG_TAG_MAX]; /**< Download receive callback functions */
    void *nh_msg_dnld_recv_arg[nh_MSG_TAG_MAX];      /**< Download receive callback arguments */

    nh_msg_upld_cb_t nh_msg_upld_send_cb[nh_MSG_TAG_MAX]; /**< Upload send done callback functions */
    void *nh_msg_upld_send_arg[nh_MSG_TAG_MAX];           /**< Upload send done callback arguments */

    nh_msg_cb_t nh_msg_hw_reset_cb[nh_MSG_TAG_MAX]; /**< Hardware reset callback function */
    void *nh_msg_hw_reset_arg[nh_MSG_TAG_MAX];      /**< Hardware reset callback argument */

    nh_msg_stream_stat_t stream_stats[nh_MSG_TAG_MAX];

    nh_msg_ctrl_ops_t *msg_sdio_ops;
} nh_msg_ctrl_t;

/*****************************************************************************
 * Function Declarations
 *****************************************************************************/

/**
 * @brief Initialize message controller
 * @retval Pointer to message controller on success
 * @retval NULL on failure
 */
nh_msg_ctrl_t *nh_msg_ctrl_init(nh_msg_ctrl_ops_t *sdio_ops);

/**
 * @brief Deinitialize message controller
 * @param msg_ctrl Message controller instance
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_ctrl_deinit(nh_msg_ctrl_t *msg_ctrl);

/**
 * @brief Allocate frame element for message sending
 * @param msg_ctrl Message controller instance
 * @param xTicksToWait Ticks to wait for allocation
 * @retval Pointer to frame element on success
 * @retval NULL on failure
 */
frame_elem_t *nh_msg_ctrl_alloc(nh_msg_ctrl_t *msg_ctrl, TickType_t xTicksToWait);

/**
 * @brief Send message through message controller
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element to send
 * @param frame_elem Frame element to send to front
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_ctrl_send(nh_msg_ctrl_t *msg_ctrl, frame_elem_t *frame_elem, uint8_t tofront);

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
                       nh_msg_cb_t hw_reset_cb, void *hw_reset_arg);

/**
 * @brief Unregister message callbacks
 * @param msg_ctrl Message controller instance
 * @param tag Message tag
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_cb_unregister(nh_msg_ctrl_t *msg_ctrl, uint8_t tag);

/**
 * @brief Upload send done callback (called from ISR)
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element that was sent
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_upld_send_done_cb(nh_msg_ctrl_t *msg_ctrl, frame_elem_t *frame_elem, bool success);

/**
 * @brief Download receive done callback (called from ISR)
 * @param msg_ctrl Message controller instance
 * @param frame_elem Frame element that was received
 * @retval 0 Success
 * @retval -1 Error
 */
int nh_msg_dnld_recv_done_cb(nh_msg_ctrl_t *msg_ctrl, frame_elem_t *frame_elem, bool success);

/**
 * @brief Reset per-stream statistics counters
 * @param msg_ctrl Message controller instance
 */
void nh_msg_ctrl_reset_stream_stats(nh_msg_ctrl_t *msg_ctrl);
void nh_msg_ctrl_reset_stream_stat(nh_msg_ctrl_t *msg_ctrl, uint8_t tag);

/**
 * @brief Message host reset callback
 *
 * This function is called when the message host is reset.
 * It wakes up the message processing task and sets the hardware reset flag.
 *
 * @retval 0 Success
 */
int nh_msg_host_reset_cb(void);

#endif /* __nh_MSG_CTRL___ */
