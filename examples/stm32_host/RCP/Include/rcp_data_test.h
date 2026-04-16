#ifndef __RCP_DATA_TEST_H__
#define __RCP_DATA_TEST_H__

#include <stdbool.h>
#include <stdint.h>

#define RCP_DATA_TEST_DEFAULT_DURATION_S  10U

typedef enum {
	RCP_DATA_TEST_MODE_TX = 0,
	RCP_DATA_TEST_MODE_RX,
} rcp_data_test_mode_t;

bool start_data_test(rcp_data_test_mode_t mode, uint32_t duration_ms);
bool stop_data_test(void);

#endif
