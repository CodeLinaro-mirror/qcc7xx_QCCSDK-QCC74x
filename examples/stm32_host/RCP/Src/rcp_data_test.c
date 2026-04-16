#include <rcp_os.h>
#include <rcp_user_config.h>
#include <rcp_interface.h>
#include <rcp_mux_dispatcher.h>

#include "rcp_config.h"
#include "rcp_data_test.h"
#include "log.h"

#include <string.h>
#define DATA_TEST_LEN                (1536 - 8)
#define DATA_TEST_STREAM             RCP_MSG_TYPE_ETH
#define DATA_TEST_REPORT_INTERVAL_MS 1000U
#define DATA_TEST_RX_POLL_MS         1000U
#define DATA_TEST_TX_TIMEOUT_MS      3000U
#define DATA_TEST_PATTERN_WORD_SIZE  sizeof(uint32_t)

typedef struct {
	uint32_t start_ms;
	uint32_t interval_start_ms;
	uint32_t last_ms;
	uint32_t packets;
	uint32_t interval_packets;
	uint64_t total_bytes;
	uint64_t interval_bytes;
} data_test_perf_stat_t;

static struct {
	rcp_data_test_mode_t mode;
	uint32_t start_time_ms;
	uint32_t end_time_ms;
	uint32_t tx_packets;
	uint32_t rx_packets;
	uint64_t tx_bytes;
	uint64_t rx_bytes;
	bool running;
	volatile bool stop_requested;
	volatile bool rx_collecting;
	data_test_perf_stat_t rx_stat;
	bool previous_rx_queue_enabled;
	rcp_rx_event_handler_t previous_handler;
} test_info;

static void reset_test_info(void)
{
	memset(&test_info, 0, sizeof(test_info));
}

static uint32_t now_ms(void)
{
	return rcp_os_time_ms();
}

static unsigned long long bytes_to_mbps_x100(uint64_t bytes, uint32_t elapsed_ms)
{
	if (elapsed_ms == 0U) {
		return 0U;
	}

	return (bytes * 8ULL * 100ULL) / ((uint64_t)elapsed_ms * 1000ULL);
}

static const char *mode_to_str(rcp_data_test_mode_t mode)
{
	return (mode == RCP_DATA_TEST_MODE_RX) ? "RX" : "TX";
}

static void data_test_perf_stat_reset(data_test_perf_stat_t *stat)
{
	if (stat == NULL) {
		return;
	}

	memset(stat, 0, sizeof(*stat));
}

static void data_test_print_interval(uint32_t start_ms, uint32_t end_ms, uint64_t bytes, uint32_t packets)
{
	uint32_t mbps_x100 = bytes_to_mbps_x100(bytes, end_ms - start_ms);

    printf("[ %4d.%03d-%4d.%03d sec ] %s %4d.%2d Mbps %4d pkts\n",
           (uint32_t)(start_ms / 1000U),
           (uint32_t)(start_ms % 1000U),
           (uint32_t)(end_ms / 1000U),
           (uint32_t)(end_ms % 1000U),
           mode_to_str(test_info.mode),
           (uint32_t)mbps_x100 / 100UL,
           (uint32_t)mbps_x100 % 100UL,
           (uint32_t)packets);
}

static void data_test_rx_stat_update(uint32_t bytes)
{
	data_test_perf_stat_t snapshot;
	bool should_print = false;
	uint32_t now;

	if (!test_info.rx_collecting || (bytes == 0U)) {
		return;
	}

	now = now_ms();

    //rcp_os_disable_irq();
	if (test_info.rx_stat.start_ms == 0U) {
		test_info.rx_stat.start_ms = now;
		test_info.rx_stat.interval_start_ms = test_info.rx_stat.start_ms;
		test_info.start_time_ms = test_info.rx_stat.start_ms;
	}

	test_info.rx_stat.last_ms = now;
	test_info.rx_stat.interval_bytes += bytes;
	test_info.rx_stat.total_bytes += bytes;
	test_info.rx_stat.packets += 1U;
	test_info.rx_stat.interval_packets += 1U;

	test_info.rx_packets = test_info.rx_stat.packets;
	test_info.rx_bytes = test_info.rx_stat.total_bytes;

	if ((test_info.rx_stat.last_ms - test_info.rx_stat.interval_start_ms) >= DATA_TEST_REPORT_INTERVAL_MS) {
		snapshot = test_info.rx_stat;
		test_info.rx_stat.interval_start_ms = test_info.rx_stat.last_ms;
		test_info.rx_stat.interval_bytes = 0U;
		test_info.rx_stat.interval_packets = 0U;
		should_print = true;
	}

    //rcp_os_enable_irq();

	if (should_print) {
		data_test_print_interval(snapshot.interval_start_ms,
					 snapshot.last_ms,
					 snapshot.interval_bytes,
					 snapshot.interval_packets);
	}
}

static void data_test_rx_stat_snapshot(data_test_perf_stat_t *stat)
{
	if (stat == NULL) {
		return;
	}

	rcp_os_disable_irq();
	*stat = test_info.rx_stat;
	rcp_os_enable_irq();
}

static uint8_t data_test_rx_handler(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr)
{
	if ((msg_flags == RCP_FLAG_UPLD_DATA) && (payload_ptr != NULL)) {

		//data_test_rx_stat_update(payload_ptr->data_size);
	}

	return RCP_RX_UPLOAD;
}

static void init_tx_payload(uint8_t *buffer, uint32_t size, uint32_t sequence)
{
	uint32_t copy_len;

	if ((buffer == NULL) || (size == 0U)) {
		return;
	}

	copy_len = (size < DATA_TEST_PATTERN_WORD_SIZE) ? size : DATA_TEST_PATTERN_WORD_SIZE;
	//memcpy(buffer, &sequence, copy_len);
}

static void print_summary(void)
{
	uint64_t bytes = (test_info.mode == RCP_DATA_TEST_MODE_TX) ? test_info.tx_bytes : test_info.rx_bytes;
	uint32_t packets = (test_info.mode == RCP_DATA_TEST_MODE_TX) ? test_info.tx_packets : test_info.rx_packets;
	uint32_t elapsed_ms = test_info.end_time_ms - test_info.start_time_ms;
	unsigned long long mbps_x100 = bytes_to_mbps_x100(bytes, elapsed_ms);

	printf("RCP Data Test Complete\n");
	printf("%s %llu.%02llu Mbps %lu pkts\n",
	       mode_to_str(test_info.mode),
	       mbps_x100 / 100ULL,
	       mbps_x100 % 100ULL,
	       (unsigned long)packets);
}

static void restore_previous_dispatch(lnk_sts_t *lnk)
{
}

static void drain_rx_queue(void)
{
	buffer_mgmt_elem_t *rx_buffer = NULL;

	while (link_receive(DATA_TEST_STREAM, &rx_buffer, 0U) == RCP_LNK_SUCCESS) {
		if (rx_buffer != NULL) {
			buffer_mgmt_free_elem(rx_buffer);
			rx_buffer = NULL;
		}
	}
}

static bool prepare_test_dispatch(lnk_sts_t *lnk)
{
	test_info.previous_handler = get_rx_handler(DATA_TEST_STREAM);
	test_info.previous_rx_queue_enabled = lnk->rx_queue_enabled;

	if (!register_data_msg_handler(DATA_TEST_STREAM, data_test_rx_handler)) {
		return false;
	}

	drain_rx_queue();
	lnk->rx_queue_enabled = false;

	return true;
}

static link_result_t send_tx_packet(lnk_sts_t *lnk, uint32_t packet_size, uint32_t sequence)
{
	buffer_mgmt_elem_t *tx_buffer = NULL;
	link_result_t result;

	if (buffer_mgmt_alloc_elem(lnk->buffer_ctrl, &tx_buffer, DATA_TEST_TX_TIMEOUT_MS) != 0 || tx_buffer == NULL) {
		return RCP_LNK_ERR_TX_NO_RESOURCES;
	}

	tx_buffer->data_size = packet_size;
	init_tx_payload(BUFFER_MGMT_DATA_PTR(tx_buffer), packet_size, sequence);

	result = link_send(DATA_TEST_STREAM, tx_buffer, DATA_TEST_TX_TIMEOUT_MS);
	if (result != RCP_LNK_SUCCESS) {
		buffer_mgmt_free_elem(tx_buffer);
	}

	return result;
}

static bool run_tx_test(lnk_sts_t *lnk, uint32_t duration_ms)
{
	uint32_t tx_packet_size = DATA_TEST_LEN;
	uint32_t deadline_ms;
	uint32_t interval_start_ms;
	uint32_t interval_bytes = 0U;
	uint32_t interval_packets = 0U;

	if (tx_packet_size == 0U) {
		printf("Failed to resolve TX packet size\n");
		return false;
	}

	printf("RCP TX test started: duration=%lu ms payload=%lu bytes\n",
	       (unsigned long)duration_ms,
	       (unsigned long)tx_packet_size);

	test_info.start_time_ms = now_ms();
	interval_start_ms = test_info.start_time_ms;
	deadline_ms = test_info.start_time_ms + duration_ms;

	while ((now_ms() < deadline_ms) && !test_info.stop_requested) {
		link_result_t result = send_tx_packet(lnk, tx_packet_size, test_info.tx_packets);

		if (result == RCP_LNK_SUCCESS) {
			test_info.tx_packets++;
			test_info.tx_bytes += tx_packet_size;
			interval_packets++;
			interval_bytes += tx_packet_size;
		} else if ((result == RCP_LNK_TX_BUSY) || (result == RCP_LNK_ERR_TX_NO_RESOURCES)) {
			printf("TX busy %d...\n", result);
		} else {
			printf("TX test aborted, link_send failed: %d\n", result);
			test_info.end_time_ms = now_ms();
			restore_previous_dispatch(lnk);
			return false;
		}

		if ((now_ms() - interval_start_ms) >= DATA_TEST_REPORT_INTERVAL_MS) {
			uint32_t interval_end_ms = now_ms();

			data_test_print_interval(interval_start_ms, interval_end_ms, interval_bytes, interval_packets);
			interval_start_ms = interval_end_ms;
			interval_bytes = 0U;
			interval_packets = 0U;
		}
	}

	test_info.end_time_ms = now_ms();
	if ((interval_bytes > 0U) || (interval_packets > 0U)) {
		data_test_print_interval(interval_start_ms, test_info.end_time_ms, interval_bytes, interval_packets);
	}

	restore_previous_dispatch(lnk);
	return true;
}

static bool run_rx_test(lnk_sts_t *lnk)
{
	data_test_perf_stat_t snapshot;
	buffer_mgmt_elem_t *rx_buffer = NULL;

	if (!prepare_test_dispatch(lnk)) {
		printf("Failed to prepare RX test dispatch\n");
		return false;
	}

	printf("RCP RX test started: run until stop command\n");

	test_info.start_time_ms = now_ms();
	data_test_perf_stat_reset(&test_info.rx_stat);
	test_info.rx_collecting = true;

	while (!test_info.stop_requested) {

        if (link_receive(DATA_TEST_STREAM, &rx_buffer, DATA_TEST_TX_TIMEOUT_MS) != RCP_LNK_SUCCESS) {
            continue;
        }

		if (rx_buffer != NULL) {
            data_test_rx_stat_update(buffer_mgmt_data_size(rx_buffer));
			buffer_mgmt_free_elem(rx_buffer);
			rx_buffer = NULL;
		}
	}

	test_info.rx_collecting = false;
	data_test_rx_stat_snapshot(&snapshot);
	test_info.rx_packets = snapshot.packets;
	test_info.rx_bytes = snapshot.total_bytes;
	if (snapshot.last_ms != 0U) {
		test_info.start_time_ms = snapshot.start_ms;
		test_info.end_time_ms = snapshot.last_ms;
	} else {
		test_info.end_time_ms = now_ms();
	}
	drain_rx_queue();
	restore_previous_dispatch(lnk);
	return true;
}

bool start_data_test(rcp_data_test_mode_t mode, uint32_t duration_ms)
{
	lnk_sts_t *lnk = link_get_status(DATA_TEST_STREAM);
	bool result;

	if (test_info.running) {
		printf("RCP data test is already running\n");
		return false;
	}

	reset_test_info();
	test_info.mode = mode;
	test_info.running = true;

	if (!lnk || !lnk->buffer_ctrl) {
		printf("ETH stream is not initialized\n");
		test_info.running = false;
		return false;
	}

	if (link_get_state(DATA_TEST_STREAM) != LINK_STATE_HSTA_DEVICE_RUN) {
		printf("ETH stream is not ready, current state=%d\n", link_get_state(DATA_TEST_STREAM));
		test_info.running = false;
		return false;
	}

	test_info.stop_requested = false;
	result = (mode == RCP_DATA_TEST_MODE_TX) ? run_tx_test(lnk, duration_ms) : run_rx_test(lnk);
	test_info.running = false;
	print_summary();

	return result;
}

bool stop_data_test(void)
{
	if (!test_info.running) {
		return false;
	}

	test_info.stop_requested = true;
	return true;
}
