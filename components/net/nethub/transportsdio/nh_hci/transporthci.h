/**
 * @file transporthci.h
 * @brief HCI transport layer over SDIO via nethub
 * @details Provides HCI packet transport over SDIO using RCP protocol
 */

#ifndef __TRANSPORTHCI_H_
#define __TRANSPORTHCI_H_

#include <stdint.h>
#include <stdbool.h>
#include "nethub_types.h"
#include "qcc74x_core.h"
#include "nh_credit_flow.h"
#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"

/*****************************************************************************
 * Macro Definitions
 *****************************************************************************/

/** HCI receive buffer size */
#define HCI_RX_BUFF_SIZE                     (FRAME_BUFF_PAYLOAD)

/** Private memory pool configuration */
#define HCI_FRAME_CNT                        (10 + 2)

/** Reserved memory blocks for command packets */
#define HCI_CMD_FRAME_CNT                    (2)

/** Downlink flow-control credit limit. Also the initial credit limit. 0 means disabled. */
#define HCI_DNLD_CREDIT_MAX             (HCI_FRAME_CNT - HCI_CMD_FRAME_CNT)
#define HCI_DNLD_CREDIT_UPDATE_THRESHOLD (2)

/** Queue depth for receiving data from msg */
#define HCI_MSG_QUEUE_DEPTH                  (32)

/** @defgroup nh_HCI_DEVICE_STATES HCI Device State Machine
 * @brief State definitions for HCI device state machine
 * @{
 */
enum {
    nh_HCI_STA_ERROR,        /**< Error state */
    nh_HCI_STA_IDLE,         /**< Idle, waiting for HOST_READY */
    nh_HCI_STA_DEVICE_READY, /**< DEVICE_READY sent, sending DEVICE_START */
    nh_HCI_STA_RUN,          /**< Normal data transfer state */
};
/** @} */

/** Debug log control */
#ifndef DEBUG_HCI_LOG_ENABLE
#define DEBUG_HCI_LOG_ENABLE    (0)
#endif

/*****************************************************************************
 * Type Definitions
 *****************************************************************************/

/** HCI packet flags — must match host-side enum rcp_flag in rcp_protocol.h */
#define NH_HCI_FLAG_NONE            0  /**< No flag */
#define NH_HCI_FLAG_DNLD_DATA       1  /**< Download data (host -> device) */
#define NH_HCI_FLAG_UPLD_DATA       2  /**< Upload data (device -> host) */
#define NH_HCI_FLAG_HOST_RESET      3  /**< Host reset, device enters IDLE */
#define NH_HCI_FLAG_HOST_READY      4  /**< Host ready (carries host RX credit) */
#define NH_HCI_FLAG_HOST_STOP       5  /**< Host stop, device enters IDLE */
#define NH_HCI_FLAG_DEVICE_READY    7  /**< Device ready */
#define NH_HCI_FLAG_DEVICE_START    8  /**< Device start (carries initial download credit) */
#define NH_HCI_FLAG_CREDIT_UPDATE   10 /**< Standalone RX credit update (device -> host) */

#pragma pack(push, 1)

/**
 * @struct nh_hci_msg_packt
 * @brief HCI message packet structure
 * @details HCI packet format with RCP protocol header matching WiFi implementation
 */
struct nh_hci_msg_packt {
    struct nh_msg_packt msg_packt; /**< Message packet header (4 bytes) */
    uint8_t reserved[1];           /**< Reserved field (1 byte) */
    uint8_t flag;                  /**< RCP packet flag (1 byte) - RCP_FLAG_UPLD_DATA=2 */
    uint8_t credit_update_flag;    /**< Credit update flag (1 byte) */
    uint8_t credit_limit_cnt;      /**< Credit limit counter (1 byte) */
    uint8_t data[0];               /**< Variable length HCI packet data */
};

#pragma pack(pop)

/*****************************************************************************
 * Type Definitions
 *****************************************************************************/

/**
 * @struct hci_priv
 * @brief HCI device private data structure
 */
typedef struct hci_priv {
    nh_msg_ctrl_t *msg_ctrl; /**< Message control interface */
    uint8_t msg_tag;         /**< Message tag for routing */

    int hci_status;          /**< Current HCI device status */

    nh_flow_cntrl_t flow_ctrl; /**< Shared credit flow control state */

    TaskHandle_t hci_proc_task;       /**< HCI processing task handle */
    QueueHandle_t msg_dnld_queue;     /**< Message download processing queue */
    frame_queue_ctrl_t *rx_frame_ctrl; /**< RX frame buffer controller */
    bool stop_requested;               /**< Thread exit flag */
    bool reset_flag;                   /**< HW reset pending flag */

    bool hci_ready;                   /**< HCI ready status */

} hci_priv_t;

/*****************************************************************************
 * Function Declarations
 *****************************************************************************/

/**
 * @brief Initialize HCI device
 * @param[in] ctx HCI private context
 * @param[in] msg_ctrl Message control interface
 * @param[in] msg_tag Message tag for routing
 * @retval 0 Success
 * @retval <0 Error code
 */
int transporthci_init(struct hci_priv *ctx, nh_msg_ctrl_t *msg_ctrl, uint8_t msg_tag);

/**
 * @brief Send HCI data to host (upld)
 * @param[in] data Data buffer to send
 * @param[in] len Data length
 * @retval 0 Success
 * @retval <0 Error code
 */
int transporthci_send(const void *data, uint32_t len);

/**
 * @brief Send HCI data to host with skb callback (upld)
 * @param[in] data Data buffer to send
 * @param[in] len Data length
 * @param[in] free_cb Free callback for skb
 * @param[in] cb_arg Callback argument
 * @retval 0 Success
 * @retval <0 Error code
 */
int transporthci_send_skb(const void *data, uint32_t len,
                           void (*free_cb)(void *arg), void *cb_arg);

/**
 * @brief Initialize HCI channel
 * @details This function initializes the HCI channel using existing SDIO msg_ctrl
 * @retval NH_OK on success
 * @retval <0 Error code
 */
int hci_channel_init(void);

#endif /* __TRANSPORTHCI_H_ */
