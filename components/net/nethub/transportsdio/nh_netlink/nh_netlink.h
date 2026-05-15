#ifndef __nh_NETLINK_H_
#define __nh_NETLINK_H_

#include "qcc74x_core.h"
#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"
#include "nh_netlink_protocol.h"

/*****************************************************************************
 * Macro Definitions
 *****************************************************************************/

/** Private memory pool configuration, frame count affects netlink RX and msg TX */
#define NETLINK_FRAME_CNT                    (4)
#define NETLINK_DATA_HDR_LEN                 (4)
#define NETLINK_FRAME_RAWSIZE                (1500)
#define NETLINK_FRAME_SIZE                   (NETLINK_DATA_HDR_LEN + NETLINK_FRAME_RAWSIZE + FRAME_BUFF_PAYLOAD_OFFSET)

/** Download flow control limit value, also the initial flow control limit value */
#define NETLINK_DNLD_CREDIT_MAX              (NETLINK_FRAME_CNT - 1)

/** Flow control report trigger threshold */
#define NETLINK_DNLD_CREDIT_UPDATE_THRESHOLD (0)

/** Queue depth for receiving data from msg */
#define NETLINK_MSG_QUEUE_DEPTH              (NETLINK_FRAME_CNT + 1)

/** @defgroup nh_NETLINK_DEVICE_STATES Netlink Device State Machine
 * @brief State definitions for Netlink device state machine
 * @{
 */
#define nh_NETLINK_DSTA_ERROR                0 /**< Error state */
#define nh_NETLINK_DSTA_IDLE                 1 /**< Device waiting for host ready */
#define nh_NETLINK_DSTA_HOST_READY           2 /**< Host ready, send DEVICE_START after initialization */
#define nh_NETLINK_DSTA_DEVICE_RUN           3 /**< Normal data transfer state */
/** @} */

/** @defgroup nh_NETLINK_FLAGS Netlink Packet Flags
 * @brief Unified command definitions for Netlink packets
 * @{
 */
enum {
    nh_NETLINK_FLAG_NONE,       /**< No specific flag */

    nh_NETLINK_FLAG_DNLD_DATA,  /**< Host to device data packet */
    nh_NETLINK_FLAG_UPLD_DATA,  /**< Device to host data packet (may carry flow control) */

    nh_NETLINK_FLAG_HOST_RESET, /**< Host reset command */
    nh_NETLINK_FLAG_HOST_READY, /**< Host ready command */
    nh_NETLINK_FLAG_HOST_STOP,  /**< Host stop command */

    nh_NETLINK_FLAG_DEVICE_RESET, /**< Device reset command */
    nh_NETLINK_FLAG_DEVICE_START, /**< Device start command (with initial credit_limit) */
    nh_NETLINK_FLAG_DEVICE_STOP,  /**< Device stop command */

    nh_NETLINK_FLAG_CREDIT_UPDATE, /**< Flow control packet */
};
/** @} */

/*****************************************************************************
 * Type Definitions
 *****************************************************************************/

#pragma pack(push, 1)

/**
 * @struct nh_netlink_msg_packt
 * @brief Netlink message packet header with flow control
 * @details Header structure for Netlink message packets sent over transport
 *          Similar to nh_tty_msg_packt
 */
struct nh_netlink_msg_packt {
    struct nh_msg_packt msg_packt; /**< Base message packet header */

    uint8_t reseved[1]; /**< Alignment padding */
    uint8_t flag;       /**< Packet flag (see nh_NETLINK_FLAG_*) */

    uint8_t credit_update_flag; /**< Credit update flag */
    uint8_t credit_limit_cnt;   /**< Credit limit counter */

    uint8_t data[0]; /**< Variable length data */
};
#pragma pack(pop)

/**
 * @struct nh_netlink_priv
 * @brief Netlink private data structure
 * @details Private data structure for device-side Netlink implementation
 */

/**
 * @typedef nh_netlink_callback_t
 * @brief Netlink data receive callback function type (per data type)
 * @param[in] cbpri_arg Callback private argument
 * @param[in] data Received data buffer
 * @param[in] data_size Data size
 * @retval 0 Success
 */
typedef int (*nh_netlink_callback_t)(void *cbpri_arg, uint8_t *data, uint32_t data_size);
struct nh_netlink_priv {
    nh_msg_ctrl_t *msg_ctrl; /**< Message control interface */
    uint8_t msg_tag;           /**< Message tag for routing */
    bool reset_flag;           /**< Reset flag for msg_ctrl */

    int netlink_status;       /**< Current Netlink device status */

    uint8_t device_dnld_credit_limit;    /**< Device download credit limit */
    uint8_t credit_limit_update_last;    /**< Last credit limit update value */
    int credit_cnt;                      /**< Download credit counter */
    uint8_t host_upld_credit_limit;      /**< Host upload credit limit */
    uint8_t device_upld_credit_consumed; /**< Device upload credit consumed */

    TaskHandle_t netlink_proc_task; /**< Netlink processing task handle */
    QueueHandle_t msg_dnld_queue;    /**< Message download processing queue */
    frame_queue_ctrl_t *rx_frame_ctrl; /**< RX frame buffer controller */
    bool stop_requested;              /**< Thread exit flag */

    bool netlink_ready; /**< Netlink ready status */

    /**
     * @brief Per-data-type download callbacks
     * @note dnld_cb[data_type] stores callback for specific data type
     */
    nh_netlink_callback_t dnld_cb[nh_NETLINK_DATA_TYPE_MAX]; /**< Download data callbacks */
    void *cbpri_arg[nh_NETLINK_DATA_TYPE_MAX];                /**< Callback private arguments */
};

/*****************************************************************************
 * Function Declarations
 *****************************************************************************/

/**
 * @brief Initialize Netlink device
 * @param[in] msg_ctrl Message control interface
 * @param[in] msg_tag Message tag for routing
 * @retval 0 Success
 * @retval <0 Error code
 */
int nh_netlink_init(nh_msg_ctrl_t *msg_ctrl, uint8_t msg_tag);

/**
 * @brief Deinitialize Netlink device
 * @retval 0 Success
 * @retval <0 Error code
 */
int nh_netlink_deinit(void);

/**
 * @brief Send data to host (upload) with type
 * @param[in] data_type Data type (see nh_netlink_data_type enum)
 * @param[in] data_buff Data buffer to send
 * @param[in] data_size Data size
 * @retval >0 Number of bytes sent
 * @retval <0 Error code
 */
int nh_netlink_upld_send(uint8_t data_type, uint8_t *data_buff, uint32_t data_size);

/**
 * @brief Register download data callback for specific data type
 * @param[in] data_type Data type (see nh_netlink_data_type enum)
 * @param[in] dnld_cb Download callback function
 * @param[in] cbpri_arg Callback private argument
 * @retval 0 Success
 * @retval <0 Error code
 */
int nh_netlink_dnld_register(uint8_t data_type, nh_netlink_callback_t dnld_cb, void *cbpri_arg);

#endif /* __nh_NETLINK_H_ */
