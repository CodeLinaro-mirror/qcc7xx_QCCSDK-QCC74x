#include "rcp_error_codes.h"

void set_err_code(buffer_mgmt_elem_t* buffer, uint8_t error_code)
{
    //if (buffer)
    //{
    //    buffer->data_size = 1;
    //    *BUFFER_MGMT_DATA_PTR(buffer) = error_code;
    //}
}

uint8_t get_err_code(buffer_mgmt_elem_t* buffer)
{
    //if (buffer && (buffer->data_size > 0))
    //{
    //    return *BUFFER_MGMT_DATA_PTR(buffer);
    //}
    return RCP_ERR_NO_CODE;
}
