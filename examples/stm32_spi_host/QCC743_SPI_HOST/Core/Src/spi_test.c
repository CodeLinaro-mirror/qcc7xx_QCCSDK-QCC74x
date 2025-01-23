/*
 * spi_test.c
 *
 *  Created on: Nov 21, 2024
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "spi.h"
#include "cmsis_os2.h"
#include "task.h"

static uint8_t show_tput;

static void spi_show_throuput(struct spi_stat *stat, struct spi_stat *last, unsigned elapsed)
{
	long tx_pkts = (long)(stat->tx_pkts - last->tx_pkts);
	long tx_bytes = (long)(stat->tx_bytes - last->tx_bytes);
	long rx_pkts = (long)(stat->rx_pkts - last->rx_pkts);
	long rx_bytes = (long)(stat->rx_bytes - last->rx_bytes);

	printf("Elapsed ticks %u, tx %ld pkts, %ld bits, rx %ld pkts, %ld bits\r\n",
			elapsed, tx_pkts, tx_bytes * 8, rx_pkts, rx_bytes * 8);
}

static void spi_show_tput(void *arg)
{
	struct spi_stat last_stat, stat;
	BaseType_t now, print_ticks, elapsed;

	print_ticks = 0;
	memset(&last_stat, 0, sizeof(last_stat));
	printf("spi tput printer starts running\r\n");
	while (show_tput) {
		now = xTaskGetTickCount();
		if (!print_ticks) {
			print_ticks = now;
			continue;
		}

		elapsed = now - print_ticks;
		if (elapsed >= 1000) {
			spi_get_stats(&stat);
			spi_show_throuput(&stat, &last_stat, elapsed);
			last_stat = stat;
			print_ticks = now;
		}

		osDelay(1000);
	}
	printf("spi tput printer exiting\r\n");
	osThreadExit();
}

void spi_start_show_tput(void)
{
	osThreadAttr_t task_attr = {
		.name = "spi_tput",
		.priority = (osPriority_t) osPriorityRealtime6,
		.stack_size = 2048
	};

	if (show_tput) {
		printf("spi tput printer is already running\r\n");
		return;
	}

	show_tput = 1;
	osThreadNew(spi_show_tput, NULL, &task_attr);
}

void spi_stop_show_tput(void)
{
	show_tput = 0;
	printf("stop spi tput printer\r\n");
}

#define TEST_DATA_XFER_SIZE	6 * 1024

static osThreadId_t tx_tid;
static uint8_t tx_perf_run;
static uint8_t test_txdata[TEST_DATA_XFER_SIZE];
static uint8_t test_rxdata[TEST_DATA_XFER_SIZE];
static uint8_t txdata[TEST_DATA_XFER_SIZE];
static uint8_t rxdata[TEST_DATA_XFER_SIZE];

static osThreadId_t rx_tid;
static uint8_t rx_perf_run;

static void spi_write_test0(int count, int len)
{
	int ret;
	struct spi_msg m;

	if (len > TEST_DATA_XFER_SIZE) {
		len = TEST_DATA_XFER_SIZE;
		printf("test tx msg is truncated to %d\r\n", len);
	}

	while (count-- > 0) {
		SPI_MSG_INIT(m, test_txdata, len, NULL, 0);
		ret = spi_write(&m, 2000);
		if (ret <= 0) {
			printf("failed to spi_write %d\r\n", ret);
		} else {
			//printf("spi write done\r\n");
		}
	}
}

static void spi_do_write_test1(void)
{
	int ret, len;
	struct spi_msg m;

	for (len = 1; len <= TEST_DATA_XFER_SIZE; ) {
		uint8_t b = len & 0xFF;

		memset(test_txdata, b, len);
		SPI_MSG_INIT(m, test_txdata, len, NULL, 0);
		ret = spi_write(&m, 10000);
		if (ret <= 0) {
			printf("failed to spi_write %d\r\n", ret);
		} else {
			//printf("spi write with length of %d done\r\n", len);
			len++;
		}
	}
}

static void spi_write_test1(int count)
{
	while (count--)
		spi_do_write_test1();
}

void spi_write_test(int mode, int count, int len)
{
	switch (mode) {
	case 0:
		spi_write_test0(count, len);
		break;

	case 1:
		spi_write_test1(count);
		break;

	default:
		printf("unknown write test mode %d\r\n", mode);
	}
}

static void spi_read_test0(int count, int len)
{
	int ret;
	struct spi_msg m;

	if (len > TEST_DATA_XFER_SIZE) {
		len = TEST_DATA_XFER_SIZE;
		printf("test tx msg is truncated to %d\r\n", len);
	}

	while (count-- > 0) {
		SPI_MSG_INIT(m, test_rxdata, len, NULL, 0);
		ret = spi_read(&m, 10000);
		if (ret <= 0) {
			printf("failed to spi_write %d\r\n", ret);
		} else {
			printf("spi read done\r\n");
		}
	}
}

static int spi_do_read_test1(int round)
{
	int ret, len;
	struct spi_msg m;

	for (len = 1; len <= TEST_DATA_XFER_SIZE; ) {
		SPI_MSG_INIT(m, test_rxdata, len, NULL, 0);
		ret = spi_read(&m, 10000);
		if (ret <= 0) {
			printf("failed to spi_read %d\r\n", ret);
		} else {
			if (m.flags & SPI_MSG_F_TRUNCATED || ret != len) {
				printf("Round %d Failure: We got %d bytes from spi, expected len %d, truncated %d\r\n",
						round, ret, len, m.flags & SPI_MSG_F_TRUNCATED);
				return -1;
			}

			uint8_t b = len & 0xFF;

			for (int i = 0; i < len; i++) {
				if (test_rxdata[i] != b) {
					printf("Round %d Failure: packet len %d, test_data[%d] = %d != expected %d\r\n",
							round, len, i, test_rxdata[i], b);
					return -1;
				}
			}

			//printf("spi read with length of %d done\r\n", len);
			len++;
		}
	}
	return 0;
}

static void spi_read_test1(int count)
{
	while (count) {
		if (spi_do_read_test1(count) < 0)
			break;

		count--;
	}
}

void spi_read_test(int mode, int count, int len)
{
	switch (mode) {
	case 0:
		spi_read_test0(count, len);
		break;

	case 1:
		spi_read_test1(count);
		break;

	default:
		printf("unknown read test mode %d\r\n", mode);
	}
}

static void spi_perf_writer(void *arg)
{
	int ret;
	struct spi_msg m;

	while (tx_perf_run) {
		SPI_MSG_INIT(m, txdata, sizeof(txdata), NULL, 0);
		ret = spi_write(&m, 5000);
		if (ret <= 0) {
			continue;
		}
	}
	printf("spi perf writer exiting\r\n");
	osThreadExit();
}

void spi_start_tx_perf(void)
{
	osThreadAttr_t tx_task_attr = {
		.name = "spi_tx",
		.priority = (osPriority_t) osPriorityRealtime6,
		.stack_size = 4096
	};

	if (tx_perf_run) {
		printf("spi writer is already running.\r\n");
		return;
	}
	tx_perf_run = 1;
	memset(txdata, 0x55, sizeof(txdata));
	tx_tid = osThreadNew(spi_perf_writer, NULL, &tx_task_attr);
}

void spi_stop_tx_perf(void)
{
	printf("stopping spi tx perf\r\n");
	tx_perf_run = 0;
	//osThreadJoin(tx_tid);
	printf("spi tx perf stopped\r\n");
}

static void spi_perf_reader(void *arg)
{
	int ret;
	struct spi_msg m;

	printf("spi perf reader starts running\r\n");
	while (rx_perf_run) {
		SPI_MSG_INIT(m, rxdata, sizeof(rxdata), NULL, 0);
		ret = spi_read(&m, 5000);
		if (ret < 0) {
			printf("spi_read failed, %d\r\n", ret);
		}
	}
	printf("spi perf reader exiting\r\n");
	osThreadExit();
}

void spi_start_rx_perf(void)
{
	osThreadAttr_t rx_task_attr = {
		.name = "spi_rx",
		.priority = (osPriority_t) osPriorityRealtime6,
		.stack_size = 4096
	};

	if (rx_perf_run) {
		printf("spi perf reader is already running.\r\n");
		return;
	}

	rx_perf_run = 1;
	rx_tid = osThreadNew(spi_perf_reader, NULL, &rx_task_attr);
}

void spi_stop_rx_perf(void)
{
	printf("stopping spi rx perf\r\n");
	rx_perf_run = 0;
	//osThreadJoin(rx_tid);
	printf("spi rx perf stopped\r\n");
}
