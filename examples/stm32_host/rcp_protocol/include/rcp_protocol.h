#ifndef __RCP_PROTOCOL_H__
#define __RCP_PROTOCOL_H__

#include "buffer_mgmt_ctrl.h"
#include "rcp_definitions.h"
#include "rcp_bus.h"

#include <stdbool.h>

typedef struct rcp_pro_packt {
    rcp_msg_packt_t msg_packt; /**< Base message packet header */

    uint8_t reseved[1]; /**< Alignment padding */
    uint8_t flag;       /**< Packet flag */

    uint8_t credit_update_flag; /**< Credit update flag */
    uint8_t credit_limit_cnt;   /**< Credit limit counter */

    uint8_t data[0]; /**< Variable length data */
} rcp_pro_packt_t;

enum rcp_flag {
    RCP_FLAG_NONE, /**< No specific flag */
    RCP_FLAG_DNLD_DATA, /**< Host to device data packet */
    RCP_FLAG_UPLD_DATA, /**< Device to host data packet (may carry flow control) */
    RCP_FLAG_HOST_RESET, /**< Host reset (error recovery), device enters DSTA_IDLE */
    RCP_FLAG_HOST_READY, /**< Host ready, device enters DSTA_HOST_READY */
    RCP_FLAG_HOST_STOP,  /**< Host stop, device enters DSTA_IDLE */
    RCP_FLAG_DEVICE_RESET, /**< Device reset, host enters HSTA_HOST_READY */
    RCP_FLAG_DEVICE_READY, /**< Device ready (must carry MAC address), host enters HSTA_DEVICE_READY */
    RCP_FLAG_DEVICE_START, /**< Device start (must carry initial flow control), host enters HSTA_DEVICE_RUN */
    RCP_FLAG_DEVICE_STOP,  /**< Device stop, host enters HSTA_DEVICE_READY */
    RCP_FLAG_CREDIT_UPDATE, /**< Standalone flow control update when no DNLD_DATA */
    RCP_FLAG_MAX, /**< Maximum flag value */
};

typedef enum {
	RCP_LNK_SUCCESS = 0,
	RCP_LNK_NO_RX_DATA,
	RCP_LNK_TIMEOUT,
	RCP_LNK_TX_BUSY,
	RCP_LNK_ERR_TX_NO_RESOURCES,
	RCP_LNK_ERR_UNSUPPORTED_STREAM,
	RCP_LNK_ERR_INVALID_STREAM,
	RCP_LINK_ERR_INVALID_BUFFER,
	RCP_LNK_ERROR,
} link_result_t;

typedef enum {
	LINK_STATE_IDLE = 0,     // Link not synced
	LINK_STATE_SYNCING,      // Waiting for SYNC response
	LINK_STATE_SYNC_PENDING, // Got sync need to send
	LINK_STATE_ACTIVE,       // Link insync for data transfer
	LINK_STATE_FAULT,        // Link error can not do data transfer
	LINK_STATE_HSTA_RESET = 10,       /**< Reset state, needs to send HOST_READY */
	LINK_STATE_HSTA_HOST_READY,        /**< Host ready, waiting for DEVICE_READY */
	LINK_STATE_HSTA_DEVICE_READY,      /**< Device ready, waiting for DEVICE_START */
	LINK_STATE_HSTA_DEVICE_RUN,        /**< Normal operation state */
	LINK_STATE_HSTA_ERROR,             /**< Handshake error state */
} link_state_t;

typedef struct {
	uint8_t flag;
	/* enum rcp_msg_type */
	uint8_t msg_type;
	uint8_t msg_subtype;
	uint8_t credit_update_flag;
	uint8_t credit_limit_cnt;
	buffer_mgmt_elem_t *payload_ptr;
} rcp_msg_t;

typedef struct {
	uint8_t stream;
	link_state_t state;
	//rcp_seq_num_t seq_nums;
	link_result_t error;
	enum rcp_bus bus;
	uint8_t credit_limit_cnt;
	uint8_t credit_consumed_cnt;
	buffer_mgmt_ctrl_t *buffer_ctrl;
	bool rx_queue_enabled;
	rcp_queue_t rx_queue;
} lnk_sts_t;

typedef rcp_msg_t *rcp_msg_ptr_t;

uint16_t marshal_rcp_msg(rcp_msg_ptr_t msg_ptr);

bool decode_rcp_msg(rcp_msg_ptr_t msg_ptr, buffer_mgmt_elem_t *rx_buffer);

uint8_t *put_payld_size_in_buff(uint8_t *buffer_ptr, uint16_t payload_length);

uint16_t get_payld_size_from_buff(uint8_t *buffer_ptr);

uint8_t msg_type2fmt(const uint8_t msg_type);

void build_rcp_msg(rcp_msg_ptr_t msg_ptr, uint8_t msg_type, enum rcp_flag,
		   buffer_mgmt_elem_t *payload_ptr);

void build_cntrl_rcp_msg(rcp_msg_ptr_t msg_ptr,
		uint8_t msg_type, uint8_t msg_subtype, buffer_mgmt_elem_t *payload_ptr);

void build_ack_to_rxmsg(rcp_msg_ptr_t msg_ptr);

void build_nack_to_rxmsg(rcp_msg_ptr_t msg_ptr,
			 buffer_mgmt_elem_t *payload_ptr);

/**
 * Publish message to TX queue.
 *
 * \param send_msg Message to publish.
 *
 * \return true When the message is successfully marshalled and published to the TX queue.
 * The payload buffer associated with send_msg will be freed automatically.
 *
 * \return false In case of a failure (e.g. TX queue full). The function will NOT free the payload
 * buffer, letting user decide what to do with the data.
 */
bool send_rcp_msg(rcp_msg_ptr_t send_msg, uint32_t timeout_ms);

/** TX message over chosen bus.
 *
 * \param bus bus to use.
 * \param tx_buffer data to transfer.
 *
 * \return true if tx was successful.
 * \return false in case the bus reported an error.
 */
bool tx_rcp_msg(enum rcp_bus bus, buffer_mgmt_elem_t *tx_buffer);

uint8_t inc_seq_num(uint8_t current_value);

uint8_t diff_seq_nums(uint8_t tsn_sent, uint8_t last_rsn);

link_result_t send_rcp_cmd(lnk_sts_t *lnk, enum rcp_flag flag);

#endif
