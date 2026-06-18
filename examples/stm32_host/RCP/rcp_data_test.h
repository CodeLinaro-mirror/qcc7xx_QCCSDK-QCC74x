#ifndef __RCP_DATA_TEST_H__
#define __RCP_DATA_TEST_H__

#include <stdbool.h>
#include <stdint.h>

#define RCP_DATA_TEST_DEFAULT_DURATION_S  999999U
#define RCP_DATA_TEST_DEFAULT_PACKET_LEN  1536U

typedef enum {
	RCP_DATA_TEST_MODE_NONE = 0,
	RCP_DATA_TEST_MODE_TX = 1U << 0,
	RCP_DATA_TEST_MODE_RX = 1U << 1,
	RCP_DATA_TEST_MODE_TXRX = RCP_DATA_TEST_MODE_TX | RCP_DATA_TEST_MODE_RX,
} rcp_data_test_mode_t;

bool start_data_test(rcp_data_test_mode_t mode, uint32_t duration_ms, uint32_t packet_len);
bool stop_data_test(void);

#endif
