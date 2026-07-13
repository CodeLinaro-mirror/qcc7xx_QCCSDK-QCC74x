#include <stdio.h>
#include <string.h>

#include <buffer_mgmt_ctrl.h>
#include <rcp_interface.h>
#include <rcp_mux_dispatcher.h>
#include <rcp_os.h>
#include <rcp_stat.h>

#include "rcp_data_test.h"

typedef struct {
	uint64_t bytes;
	uint32_t packets;
} data_test_stat_t;

static struct {
	rcp_data_test_mode_t mode;
	uint32_t duration_ms;
	uint32_t packet_len;
	uint32_t start_ms;
	uint32_t end_ms;
	uint32_t last_report_ms;
	volatile bool running;
	volatile bool stop_requested;
	data_test_stat_t tx;
	data_test_stat_t rx;
} g_data_test;

#define DATA_TEST_STREAM              RCP_MSG_TYPE_WIFI_STA
#define DATA_TEST_REPORT_INTERVAL_MS  1000U
#define DATA_TEST_RX_IDLE_MS          10U
#define DATA_TEST_PROTO_SIZE          ((uint32_t)sizeof(rcp_pro_packt_t))
#define DATA_TEST_MAX_PACKET_LEN      FRAME_BUFF_RCP_PACKET_LEN
#define DATA_TEST_TAG_SIZE            ((uint32_t)sizeof(g_data_test_magic))
#define DATA_TEST_MIN_PACKET_LEN      (DATA_TEST_PROTO_SIZE + DATA_TEST_TAG_SIZE)

static const uint8_t g_data_test_magic[8] = {'S', 'D', 'I', 'O', 'P', 'E', 'R', 'F'};

static uint32_t data_test_now_ms(void)
{
	return rcp_os_time_ms();
}

static unsigned long long data_test_bytes_to_mbps_x100(uint64_t bytes, uint32_t elapsed_ms)
{
	if (elapsed_ms == 0U) {
		return 0U;
	}

	return (bytes * 8ULL * 100ULL) / ((uint64_t)elapsed_ms * 1000ULL);
}

static bool data_test_mode_has_tx(rcp_data_test_mode_t mode)
{
	return (mode & RCP_DATA_TEST_MODE_TX) != 0;
}

static bool data_test_mode_has_rx(rcp_data_test_mode_t mode)
{
	return (mode & RCP_DATA_TEST_MODE_RX) != 0;
}

static void data_test_reset(void)
{
	memset(&g_data_test, 0, sizeof(g_data_test));
}

static bool data_test_parse_packet(const uint8_t *data, uint32_t len)
{
	if (data == NULL || len < DATA_TEST_TAG_SIZE) {
		return false;
	}

	return memcmp(data, g_data_test_magic, DATA_TEST_TAG_SIZE) == 0;
}

static void data_test_stat_add(data_test_stat_t *stat, uint32_t bytes)
{
	uintptr_t flags;

	if (stat == NULL) {
		return;
	}

	flags = rcp_os_irq_save();
	stat->bytes += bytes;
	stat->packets++;
	rcp_os_irq_restore(flags);
}

static void data_test_snapshot(data_test_stat_t *tx, data_test_stat_t *rx)
{
	uintptr_t flags = rcp_os_irq_save();

	if (tx != NULL) {
		*tx = g_data_test.tx;
	}
	if (rx != NULL) {
		*rx = g_data_test.rx;
	}

	rcp_os_irq_restore(flags);
}

static void data_test_print_dir(const char *label, uint64_t bytes, uint32_t packets, uint32_t elapsed_ms)
{
	unsigned long long mbps_x100 = data_test_bytes_to_mbps_x100(bytes, elapsed_ms);

	printf("%s %lu.%02lu Mbps %lu pkts\n",
	       label,
	       (unsigned long)(mbps_x100 / 100ULL),
	       (unsigned long)(mbps_x100 % 100ULL),
	       (unsigned long)packets);
}

static void data_test_print_interval(uint32_t start_ms,
				     uint32_t end_ms,
				     const data_test_stat_t *prev_tx,
				     const data_test_stat_t *prev_rx,
				     const data_test_stat_t *curr_tx,
				     const data_test_stat_t *curr_rx)
{
	uint32_t elapsed_ms;

	if (end_ms <= start_ms) {
		return;
	}

	elapsed_ms = end_ms - start_ms;
	if (data_test_mode_has_tx(g_data_test.mode)) {
		printf("[ %4lu.%03lu-%4lu.%03lu sec ] ",
		       (unsigned long)(start_ms / 1000U),
		       (unsigned long)(start_ms % 1000U),
		       (unsigned long)(end_ms / 1000U),
		       (unsigned long)(end_ms % 1000U));
		data_test_print_dir("TX",
				    curr_tx->bytes - prev_tx->bytes,
				    curr_tx->packets - prev_tx->packets,
				    elapsed_ms);
	}
	if (data_test_mode_has_rx(g_data_test.mode)) {
		printf("[ %4lu.%03lu-%4lu.%03lu sec ] ",
		       (unsigned long)(start_ms / 1000U),
		       (unsigned long)(start_ms % 1000U),
		       (unsigned long)(end_ms / 1000U),
		       (unsigned long)(end_ms % 1000U));
		data_test_print_dir("RX",
				    curr_rx->bytes - prev_rx->bytes,
				    curr_rx->packets - prev_rx->packets,
				    elapsed_ms);
	}
}

static void data_test_print_summary(void)
{
	data_test_stat_t tx = {0};
	data_test_stat_t rx = {0};
	uint32_t elapsed_ms;

	data_test_snapshot(&tx, &rx);
	elapsed_ms = (g_data_test.end_ms > g_data_test.start_ms) ?
		     (g_data_test.end_ms - g_data_test.start_ms) : 0U;

	printf("RCP data test complete\n");
	if (data_test_mode_has_tx(g_data_test.mode)) {
		data_test_print_dir("TX avg", tx.bytes, tx.packets, elapsed_ms);
	}
	if (data_test_mode_has_rx(g_data_test.mode)) {
		data_test_print_dir("RX avg", rx.bytes, rx.packets, elapsed_ms);
	}
	RCP_TX_PROF_DUMP();
}

static uint8_t data_test_rx_proxy(rcp_message_t *pmessage, lnk_sts_t *lnk)
{
	const uint8_t *data;
	uint32_t len;
	const buffer_mgmt_elem_t *payload_ptr = pmessage ? pmessage->msg_buffer : NULL;

	(void)lnk;

	if (pmessage != NULL && pmessage->msg_flags == RCP_FLAG_UPLD_DATA && payload_ptr != NULL) {
		data = buffer_mgmt_data_ptr_const(payload_ptr);
		len = buffer_mgmt_data_size(payload_ptr);
		if (data_test_parse_packet(data, len)) {
			if (g_data_test.running && data_test_mode_has_rx(g_data_test.mode)) {
				data_test_stat_add(&g_data_test.rx, len + DATA_TEST_PROTO_SIZE);
			}
		}
	}

	return RCP_RX_NO_ACTION;
}

bool start_data_test(rcp_data_test_mode_t mode, uint32_t duration_ms, uint32_t packet_len)
{
	data_test_stat_t prev_tx = {0};
	data_test_stat_t prev_rx = {0};
	data_test_stat_t curr_tx = {0};
	data_test_stat_t curr_rx = {0};
	lnk_sts_t *lnk = link_get_status(DATA_TEST_STREAM);
	rcp_rx_event_handler_t old_rx_handler;
	bool tx_enable = data_test_mode_has_tx(mode);
	bool rx_enable = data_test_mode_has_rx(mode);
	uint32_t tx_packet_len = 0U;
	uint32_t tx_payload_len = 0U;
	buffer_mgmt_ctrl_t *tx_pool = NULL;

	if (g_data_test.running) {
		printf("RCP data test is already running\n");
		return false;
	}
	if (!tx_enable && !rx_enable) {
		printf("Invalid RCP data test mode\n");
		return false;
	}
	if (duration_ms == 0U) {
		duration_ms = RCP_DATA_TEST_DEFAULT_DURATION_S * 1000U;
	}
	if (tx_enable) {
		if (packet_len == 0U) {
			packet_len = RCP_DATA_TEST_DEFAULT_PACKET_LEN;
		}
		if (packet_len < DATA_TEST_MIN_PACKET_LEN ||
		    packet_len > DATA_TEST_MAX_PACKET_LEN) {
			printf("Invalid packet length %lu (min %lu, max %lu)\n",
			       (unsigned long)packet_len,
			       (unsigned long)DATA_TEST_MIN_PACKET_LEN,
			       (unsigned long)DATA_TEST_MAX_PACKET_LEN);
			return false;
		}
		tx_packet_len = packet_len;
		tx_payload_len = packet_len - DATA_TEST_PROTO_SIZE;
	}
	if (lnk == NULL) {
		printf("ETH stream is not initialized\n");
		return false;
	}
	if (tx_enable) {
		tx_pool = lnk->rx_buffer_ctrl;
		if (tx_pool == NULL) {
			printf("RCP data test TX pool is unavailable\n");
			return false;
		}
	}
	if (link_get_state(DATA_TEST_STREAM) != LINK_STATE_HSTA_DEVICE_RUN) {
		printf("ETH stream is not ready, current state=%d\n", link_get_state(DATA_TEST_STREAM));
		return false;
	}
	old_rx_handler = get_rx_handler(DATA_TEST_STREAM);
	if (link_register_rx_handler(DATA_TEST_STREAM, data_test_rx_proxy) != RCP_LNK_SUCCESS) {
		printf("Failed to register ETH test RX handler\n");
		return false;
	}

	data_test_reset();
	RCP_TX_PROF_RESET();
	g_data_test.mode = mode;
	g_data_test.duration_ms = duration_ms;
	g_data_test.packet_len = packet_len;
	g_data_test.start_ms = data_test_now_ms();
	g_data_test.last_report_ms = g_data_test.start_ms;
	g_data_test.running = true;

	printf("RCP data test started: mode=%s%s",
	       tx_enable ? "tx" : "",
	       (tx_enable && rx_enable) ? "+rx" : (rx_enable ? "rx" : ""));
	if (duration_ms != 0U) {
		printf(", duration=%lu ms", (unsigned long)duration_ms);
	}
	if (tx_enable) {
		printf(", packet=%lu bytes", (unsigned long)tx_packet_len);
	}
	printf("\n");

	while (!g_data_test.stop_requested) {
		uint32_t now = data_test_now_ms();

		if (g_data_test.duration_ms != 0U &&
		    (now - g_data_test.start_ms) >= g_data_test.duration_ms) {
			break;
		}

		if (tx_enable) {
			buffer_mgmt_elem_t *tx_buffer = NULL;
			uint8_t *tx_data;
			link_result_t send_result;
			RCP_TX_PROF_BEGIN(alloc_start_cycles);

			if (buffer_mgmt_alloc_elem(tx_pool, &tx_buffer, RCP_OS_WAIT_FOREVER) != 0 ||
			    tx_buffer == NULL) {
				RCP_TX_PROF_ACC(RCP_TX_PROF_PROD_ALLOC, alloc_start_cycles);
				printf("RCP data test TX stopped: no buffer\n");
				break;
			}
			RCP_TX_PROF_ACC(RCP_TX_PROF_PROD_ALLOC, alloc_start_cycles);
			tx_buffer->offset = FRAME_BUFF_PAYLOAD_OFFSET;
			tx_data = buffer_mgmt_data_ptr(tx_buffer);
			if (tx_data == NULL || buffer_mgmt_max_size(tx_buffer) < tx_payload_len) {
				buffer_mgmt_free_elem(tx_buffer);
				printf("RCP data test TX stopped: invalid buffer\n");
				break;
			}
			memcpy(tx_data, g_data_test_magic, DATA_TEST_TAG_SIZE);
			tx_buffer->data_size = tx_payload_len;
			RCP_TX_PROF_BEGIN(link_send_start_cycles);
			send_result = link_send(DATA_TEST_STREAM, tx_buffer, RCP_OS_WAIT_FOREVER);
			RCP_TX_PROF_ACC(RCP_TX_PROF_PROD_LINK_SEND, link_send_start_cycles);
			if (send_result != RCP_LNK_SUCCESS) {
				buffer_mgmt_free_elem(tx_buffer);
				printf("RCP data test TX stopped: link_send=%d\n", send_result);
				break;
			}
			data_test_stat_add(&g_data_test.tx, tx_packet_len);
		} else {
			rcp_os_delay_ms(DATA_TEST_RX_IDLE_MS);
		}

		now = data_test_now_ms();
		if ((now - g_data_test.last_report_ms) < DATA_TEST_REPORT_INTERVAL_MS) {
			continue;
		}

		data_test_snapshot(&curr_tx, &curr_rx);
		data_test_print_interval(g_data_test.last_report_ms,
					now,
					&prev_tx,
					&prev_rx,
					&curr_tx,
					&curr_rx);
		prev_tx = curr_tx;
		prev_rx = curr_rx;
		g_data_test.last_report_ms = now;
	}

	g_data_test.end_ms = data_test_now_ms();
	data_test_snapshot(&curr_tx, &curr_rx);
	data_test_print_interval(g_data_test.last_report_ms,
				g_data_test.end_ms,
				&prev_tx,
				&prev_rx,
				&curr_tx,
				&curr_rx);
	data_test_print_summary();
	g_data_test.running = false;
	g_data_test.stop_requested = false;
	(void)link_register_rx_handler(DATA_TEST_STREAM, old_rx_handler);

	return true;
}

bool stop_data_test(void)
{
	if (!g_data_test.running) {
		return false;
	}

	g_data_test.stop_requested = true;
	return true;
}
