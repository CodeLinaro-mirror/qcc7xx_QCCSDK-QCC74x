#ifndef __nh_TTY_H_
#define __nh_TTY_H_

#include "qcc74x_core.h"
#include "nh_credit_flow.h"
#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"

/*****************************************************************************
 * Macro Definitions
 *****************************************************************************/

/* Private memory-pool configuration. Block count affects TTY RX and msg TX. */
#define TTY_FRAME_CNT                    (4)
#define TTY_FRAME_RAWSIZE                (1024)
#define TTY_FRAME_SIZE                   (TTY_FRAME_RAWSIZE + FRAME_BUFF_PAYLOAD_OFFSET)// call api

/* Downlink flow-control limit. Also the initial credit limit. 0 means disabled. */
#define TTY_DNLD_CREDIT_MAX              (TTY_FRAME_CNT)// 1

/* Flow-control report threshold. Send an update when the delta exceeds this value. */
#define TTY_DNLD_CREDIT_UPDATE_THRESHOLD (2)// less then TTY_DNLD_CREDIT_MAX

/** Queue depth for data received from msg */
#define TTY_MSG_QUEUE_DEPTH              (TTY_FRAME_CNT + 1)

/** @defgroup nh_TTY_HOST_STATES TTY Host State Machine
 * @brief State definitions for TTY host state machine
 * @{
 */
#define nh_TTY_HSTA_ERROR                0 /**< Error state */
#define nh_TTY_HSTA_RESET                1 /**< Host reset state, needs to send HOST_READY packet */
#define nh_TTY_HSTA_HOST_READY           2 /**< Host ready state, waiting for DEVICE_READY */
#define nh_TTY_HSTA_DEVICE_READY         3 /**< Normal data transfer state */
#define nh_TTY_HSTA_DEVICE_RUN           4 /**< Normal data transfer state */
/** @} */

/** @defgroup nh_TTY_DEVICE_STATES TTY Device State Machine
 * @brief State definitions for TTY device state machine
 * @{
 */
#define nh_TTY_DSTA_ERROR                0 /**< Error state */
#define nh_TTY_DSTA_IDLE                 1 /**< Device waiting for host ready */
#define nh_TTY_DSTA_HOST_READY           2 /**< Host ready, send DEVICE_READY after initialization */
#define nh_TTY_DSTA_DEVICE_READY         3 /**< Normal data transfer state */
#define nh_TTY_DSTA_DEVICE_RUN           4 /**< Normal data transfer state */
/** @} */

/** @defgroup nh_TTY_FLAGS TTY Packet Flags
 * @brief Unified command definitions for TTY packets
 * @{
 */
enum {
    nh_TTY_FLAG_NONE, /**< No specific flag */

    nh_TTY_FLAG_DNLD_DATA, /**< Host to device data packet */
    nh_TTY_FLAG_UPLD_DATA, /**< Device to host data packet (may carry flow control) */

    nh_TTY_FLAG_HOST_RESET, /**< Host reset command */
    nh_TTY_FLAG_HOST_READY, /**< Host ready command */
    nh_TTY_FLAG_HOST_STOP,  /**< Host stop command */

    nh_TTY_FLAG_DEVICE_RESET, /**< Device reset command */
    nh_TTY_FLAG_DEVICE_READY,
    nh_TTY_FLAG_DEVICE_START, /**< Device start command (with initial credit_limit) */
    nh_TTY_FLAG_DEVICE_STOP,  /**< Device stop command */

    nh_TTY_FLAG_CREDIT_UPDATE, /**< Flow control packet */

    nh_TTY_FLAG_OPEN,        /**< Port open command */
    nh_TTY_FLAG_CLOSE,       /**< Port close command */
    nh_TTY_FLAG_SET_TERMIOS, /**< Set terminal attributes */
    nh_TTY_FLAG_BREAK,       /**< Send break signal */
    nh_TTY_FLAG_HANGUP,      /**< Hangup signal */
};
/** @} */

/*****************************************************************************
 * Type Definitions
 *****************************************************************************/

#pragma pack(push, 1)

/**
 * @struct nh_tty_msg_packt
 * @brief TTY message packet header with flow control
 * @details Header structure for TTY message packets sent over transport
 *          Similar to nh_eth_msg_packt but for TTY communication
 */
struct nh_tty_msg_packt {
    struct nh_msg_packt msg_packt; /**< Base message packet header */

    uint8_t reseved[1]; /**< Alignment padding */
    uint8_t flag;       /**< Packet flag (see nh_TTY_FLAG_*) */

    uint8_t credit_update_flag; /**< Credit update flag */
    uint8_t credit_limit_cnt;   /**< Credit limit counter */

    uint8_t data[0]; /**< Variable length data */
};
#pragma pack(pop)


/**
 * @struct nh_tty_priv
 * @brief TTY private data structure
 * @details Private data structure for device-side TTY implementation
 */

typedef int (*nh_tty_callback_t)(void *ctx, uint8_t *data, uint32_t data_size);
struct nh_tty_priv {
    nh_msg_ctrl_t *msg_ctrl; /**< Message control interface */
    uint8_t msg_tag;         /**< Message tag for routing */
    bool reset_flag;         /**< Reset flag for msg_ctrl */

    int tty_status; /**< Current TTY device status */

    nh_flow_cntrl_t flow_ctrl; /**< Shared credit flow control state */

    TaskHandle_t tty_proc_task;        /**< TTY processing task handle */
    QueueHandle_t msg_dnld_queue;      /**< Message download processing queue */
    frame_queue_ctrl_t *rx_frame_ctrl; /**< RX frame buffer controller */
    bool stop_requested;               /**< Thread exit flag */

    bool tty_ready; /**< TTY ready status */

    nh_tty_callback_t dnld_cb;
    void *cbpri_arg;
};

/*****************************************************************************
 * Function Declarations
 *****************************************************************************/

/**
 * @brief Initialize TTY device
 * @param[in] msg_ctrl Message control interface
 * @param[in] msg_tag Message tag for routing
 * @retval 0 Success
 * @retval <0 Error code
 */
int nh_tty_init(nh_msg_ctrl_t *msg_ctrl, uint8_t msg_tag);

int nh_tty_upld_send(uint8_t *data_buff, uint32_t data_size); // nh send
int nh_tty_dnld_register(nh_tty_callback_t dnld_cb, void *cbpri_arg);        // nh recv cb

#endif /* __nh_TTY_H_ */
