
#include "buffer_mgmt_ctrl.h"
#include "rcp_interface.h"
#include "rcp_log.h"
#include "rcp_protocol.h"
#include "rcp_tx_scheduler.h"

#include <stdio.h>
#include <stdlib.h>

//struct {
//    uint8_t stream:4;
//    uint8_t msg_type:4; // 1 byte
//
//    union {
//        uint8_t send_tsn:3;
//        uint8_t send_rsn:3;
//        uint8_t ack_required:2;
//        uint8_t msg_subtype;
//    }; // 1 byte
//    uint16_t payload_length; // 2 byte
//    uint8_t data[0];
//};

uint16_t marshal_rcp_msg(rcp_msg_ptr_t msg_ptr)
{
    rcp_pro_packt_t *dest_ptr;
	uint16_t payload_size = 0;

	if (msg_ptr->payload_ptr == NULL) {
        return 0;
    }

	dest_ptr = (rcp_pro_packt_t *)buffer_mgmt_add_header_impl(msg_ptr->payload_ptr, sizeof(rcp_pro_packt_t));
	if (dest_ptr == NULL) {
		RCP_LOG_ERR("Failed to prepend RCP header");
		return 0;
	}

	payload_size = BUFFER_MGMT_DATA_SIZE(msg_ptr->payload_ptr);

	dest_ptr->msg_packt.len = payload_size - sizeof(dest_ptr->msg_packt);
	dest_ptr->msg_packt.tag = msg_ptr->msg_type;
	dest_ptr->msg_packt.sub_tag = msg_ptr->msg_subtype;
	dest_ptr->flag = msg_ptr->flag;
	dest_ptr->credit_update_flag = msg_ptr->credit_update_flag;
	dest_ptr->credit_limit_cnt = msg_ptr->credit_limit_cnt;

	return payload_size;
}

bool decode_rcp_msg(rcp_msg_ptr_t msg_ptr, buffer_mgmt_elem_t *rx_buffer)
{
	if (rx_buffer == NULL) {
		RCP_LOG_ERR("Recieve buffer is NULL");
		return false;
	}

	if (BUFFER_MGMT_DATA_SIZE(rx_buffer) < sizeof(rcp_pro_packt_t)) {
		RCP_LOG_ERR("Invalid RX message: size %d < header %d",
			    BUFFER_MGMT_DATA_SIZE(rx_buffer), sizeof(rcp_pro_packt_t));
		return false;
	}

	rcp_pro_packt_t *scan_ptr = (rcp_pro_packt_t *)BUFFER_MGMT_DATA_PTR(rx_buffer);
	msg_ptr->msg_type = scan_ptr->msg_packt.tag;
	msg_ptr->msg_subtype = scan_ptr->msg_packt.sub_tag;
	msg_ptr->flag = scan_ptr->flag;
	msg_ptr->credit_update_flag = scan_ptr->credit_update_flag;
	msg_ptr->credit_limit_cnt = scan_ptr->credit_limit_cnt;

	uint16_t payload_size = scan_ptr->msg_packt.len;
	const uint16_t proto_meta_size = sizeof(*scan_ptr) - sizeof(scan_ptr->msg_packt);
	const uint16_t total_size = sizeof(scan_ptr->msg_packt) + payload_size;

	if (payload_size < proto_meta_size || BUFFER_MGMT_DATA_SIZE(rx_buffer) < total_size) {
		RCP_LOG_ERR("Invalid RX message: msg type=%d msg size %d < %d", msg_ptr->msg_type,
			    BUFFER_MGMT_DATA_SIZE(rx_buffer), total_size);
		return false;
	}

	if (!buffer_mgmt_consume_data(rx_buffer, sizeof(*scan_ptr))) {
		RCP_LOG_ERR("Failed to strip RCP header from RX buffer");
		return false;
	}

	rx_buffer->data_size = payload_size - proto_meta_size;
	msg_ptr->payload_ptr = rx_buffer;

	return true;
}

uint16_t get_payld_size_from_buff(uint8_t *buffer_ptr)
{
	uint16_t length = 0;
	length = *buffer_ptr++;
	length += *buffer_ptr++ << 8;
	return length;
}

uint8_t *put_payld_size_in_buff(uint8_t *buffer_ptr, uint16_t payload_length)
{
	*buffer_ptr++ = (uint8_t)(payload_length & 0x00FF);
	*buffer_ptr++ = (uint8_t)((payload_length & 0xFF00) >> 8);
	return buffer_ptr;
}

void build_rcp_msg(rcp_msg_ptr_t msg_ptr, uint8_t msg_type, enum rcp_flag flag,
		buffer_mgmt_elem_t *payload_ptr)
{
	msg_ptr->flag = flag;
	msg_ptr->msg_type = msg_type;
	msg_ptr->msg_subtype = 0;
	msg_ptr->credit_update_flag = 0;
	msg_ptr->credit_limit_cnt = 0;
	msg_ptr->payload_ptr = payload_ptr;
}

void build_cntrl_rcp_msg(rcp_msg_ptr_t msg_ptr,
		uint8_t msg_type, uint8_t msg_subtype, buffer_mgmt_elem_t *payload_ptr)
{
	// Control messages go over link management stream
	msg_ptr->flag = RCP_FLAG_NONE;
	msg_ptr->msg_type = msg_type;
	msg_ptr->msg_subtype = msg_subtype;
	msg_ptr->credit_update_flag = 0;
	msg_ptr->credit_limit_cnt = 0;
	msg_ptr->payload_ptr = payload_ptr;
}

bool send_rcp_msg(rcp_msg_ptr_t send_msg, uint32_t timeout_ms)
{
	lnk_sts_t *lnk = NULL;

	if (!send_msg || !send_msg->payload_ptr) {
		return false;
	}

	if (marshal_rcp_msg(send_msg) == 0) {
		return false;
	}

	lnk = link_get_status(send_msg->msg_type);
	if (!lnk || !lnk->buffer_ctrl) {
		RCP_LOG_ERR("Invalid stream %d for TX", send_msg->msg_type);
		return false;
	}

	/* On success, the scheduler owns the buffer until the event loop frees it.
	 * On failure, ownership stays with the caller.
	 */
	if (!schedule_tx(lnk->bus, send_msg->msg_type, send_msg->payload_ptr, timeout_ms)) {
		return false;
	}

	return true;
}

bool tx_rcp_msg(enum rcp_bus bus, buffer_mgmt_elem_t *tx_buffer)
{
	if (RCP_BUS_OK != rcp_bus_write(bus, (rcp_msg_packt_t *)BUFFER_MGMT_DATA_PTR(tx_buffer))) {
		return false;
	}
	return true;
}

uint8_t inc_seq_num(uint8_t current_value)
{
	return (current_value < RPC_SEQ_NUM_MAX ? current_value + 1 : 0) & 0x07;
}

uint8_t diff_seq_nums(uint8_t tsn_sent, uint8_t last_rsn)
{
	if (tsn_sent >= last_rsn) {
		return tsn_sent - last_rsn;
	} else {
		return (RPC_SEQ_NUM_MAX - last_rsn) + tsn_sent;
	}
}
