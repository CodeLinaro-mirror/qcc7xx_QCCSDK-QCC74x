#ifndef __RCP_INTERFACE_H__
#define __RCP_INTERFACE_H__

#include "rcp_protocol.h"
#include "rcp_cntrl_msgs.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * Inititializes RCP link
 * Parameters
 *
 * return  On success RCP_LNK_SUCCESS
 *         otherwise error code
 **/
link_result_t init_rcp(enum rcp_bus bus);

// Dispatch Response
enum RCP_DSPTCH_RESULT {
	RCP_RX_NO_ACTION, // Take no action
	RCP_RX_UPLOAD,    // upload data to queue
	RCP_RX_TAKEN,     // handler takes ownership of payload buffer
};

typedef struct rcp_message {
	uint8_t stream;
	uint8_t msg_subtype;
	uint8_t msg_flags;
	buffer_mgmt_elem_t *msg_buffer;
} rcp_message_t;

typedef struct rcp_stream_config {
	uint16_t tx_cnt;
	uint8_t rx_wnd;
	uint8_t rx_threshold;
    uint8_t tx_threshold;
} rcp_stream_config_t;

/**
 * Handler for RCP recieved events
 * Parameters
 * msg_flags - Indicates status
 * payload_ptr  - Poiter to msg payload buffer
 * return  - How to RCP should respond to request (see Dispatch Response)
 **/
typedef uint8_t (*rcp_rx_event_handler_t)(rcp_message_t *pmessage, lnk_sts_t *lnk);

/**
 * Register handler for data message recieved over link stream
 * Parameters
 * Stream        - Stream for rx messages
 * rx_message_cb - ptr to store any error code on failure
 * return  On success RCP_LNK_SUCCESS
 *         otherwise error code
 **/
link_result_t link_register_rx_handler(uint8_t stream, rcp_rx_event_handler_t rx_message_cb);

/**
 * Receive one queued RX packet for a stream.
 *
 * On success the caller owns *rx_buffer and must release it with
 * buffer_mgmt_free_elem(). RX packets without payload return success with
 * *rx_buffer == NULL.
 */
link_result_t link_receive(uint8_t stream, buffer_mgmt_elem_t **rx_buffer, uint32_t timeout_ms);

/**
 * Send request over link
 * Parameters
 * Stream      - Stream rquest should be transmitted over
 * tx_buffer   - ptr to buffer holding data to send
 * return  On success RCP_LNK_SUCCESS
 *         otherwise error code
 **/
link_result_t link_send(uint8_t stream, buffer_mgmt_elem_t *tx_buffer, uint32_t timeout_ms);
link_result_t link_send_msg(rcp_message_t *pmessage, uint32_t timeout_ms);

/**
 * Send request and register handler for response
 * Parameters
 * Stream      - Stream rquest should be transmitted over
 * tx_buffer   - ptr to buffer holding data to send
 * response_cb - Handler to deal with request
 * return     - On success RCP_LNK_SUCCESS
 *              Otherwise send failed, error code written via error_ptr
 **/
link_result_t link_request(uint8_t stream, buffer_mgmt_elem_t *tx_buffer,
			   rcp_rx_event_handler_t response_cb, uint32_t timeout_ms);

/**
 * Parameters
 * link_id - Link to reset
 * update_cb - function ptr to callback for status update (optional)
 *
 * return - RCP_LNK_SUCCESS = reset successful
 *          otherwise link error
 **/
link_result_t link_reset(uint8_t stream, buffer_mgmt_elem_t *tx_buffer);

/**
 * Get current link state
 **/
link_state_t link_get_state(uint8_t stream);

lnk_sts_t *link_get_status(uint8_t stream);

lnk_sts_t *rcp_stream_create(uint8_t stream_id, const rcp_stream_config_t *conf);
void rcp_dump_status(void);
void rcp_reset_status_counters(void);

/* Recover timing instrumentation has moved to rcp_stat.h. */

#endif
