#ifndef __RCP_ERROR_CODES_H__
#define __RCP_ERROR_CODES_H__

#include "buffer_mgmt_ctrl.h"

// Nack Errors
#define RCP_ERR_NO_CODE				0x00    // No Nack data available
#define RCP_ERR_INVALID_SEQ_NUM		0x01	// Sequence error detected at peer
#define RCP_ERR_UNHANDELED_STREAM	0x02	// peer can not handler stream
#define RCP_ERR_NOT_SYNCED			0x03	// peer not in sync state
#define RCP_ERR_REJECTED			0x04	// peer rejected message i.e. unable to process
#define RCP_ERR_HW_ERROR			0x05	// peer has HW error

// Helper functions
void set_err_code(buffer_mgmt_elem_t *buffer, uint8_t error_code);
uint8_t get_err_code(buffer_mgmt_elem_t *buffer);

#endif
