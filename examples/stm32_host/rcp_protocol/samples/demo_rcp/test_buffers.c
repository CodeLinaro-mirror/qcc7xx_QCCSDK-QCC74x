#include "rcp_buffer.h"
#include "demo_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RCP_NUM_BUFFERS 10
#define TEST_DATA_SIZE  64
#define LAST_BUFFER_IDX RCP_NUM_BUFFERS - 1

static rcp_buffer_hdl_t *acquired_buff[RCP_NUM_BUFFERS];

static void create_test_data(uint8_t *dest_ptr, uint16_t amount)
{
	for (int i = 0; i < 64; i++) {
		dest_ptr[i] = (uint8_t)(((double)rand() / (RAND_MAX)) * 255);
	}
}

int main(int argc, char *argv[])
{
	srand(time(NULL));

	rcp_buffer_pool_init();

	printf("Test: get_buffer: ");
	rcp_buffer_hdl_t *buff_ptr = rcp_buffer_alloc(TEST_DATA_SIZE);
	if (buff_ptr == NULL) {
		printf("FAILED\n");
	} else {
		printf("PASSED\n");
	}

	printf("Test: release_buffer: ");
	rcp_buffer_free(&buff_ptr);

	if (buff_ptr == NULL) {
		printf("PASSED\n");
	} else {
		printf("FAILED\n");
	}

	int buffer_count = 0;
	printf("Test: Get %d bufers: ", RCP_NUM_BUFFERS);
	for (int i = 0; i < RCP_NUM_BUFFERS; i++) {
		rcp_buffer_hdl_t *buff_ptr = rcp_buffer_alloc(TEST_DATA_SIZE);
		if (buff_ptr == NULL) {
			printf("FAILED on buffer: %d\n", i + 1);
			exit(-1);
		}
		acquired_buff[i] = buff_ptr;
		buffer_count++;
	}

	if (buffer_count == RCP_NUM_BUFFERS) {
		printf("PASSED\n");
	} else {
		printf("FAILD buffers acquired=%d Expected %d\n", buffer_count, RCP_NUM_BUFFERS);
	}

	buff_ptr = rcp_buffer_alloc(TEST_DATA_SIZE);
	printf("Test: Error on no buffer available: ");
	if (buff_ptr == NULL) {
		printf("PASSED\n");
	} else {
		printf("FAILED\n");
	}

	printf("Test: No buffers available, release one, then get one: ");
	rcp_buffer_free(&acquired_buff[LAST_BUFFER_IDX]);
	buff_ptr = rcp_buffer_alloc(TEST_DATA_SIZE);
	if (buff_ptr != NULL)
	{
		acquired_buff[LAST_BUFFER_IDX] = buff_ptr;
		printf("PASSED\n");
	}
	else
	{
		printf("Failed did not get buffer\n");
	}

	printf("Test: Release all buffers: ");
	for (int i = 0; i < RCP_NUM_BUFFERS; i++) {
		rcp_buffer_free(&acquired_buff[i]);
	}
	rcp_buffer_free(&acquired_buff[8]);
	printf("PASSED\n");

	printf("Test: Use write_to_buffer: ");
	buff_ptr = rcp_buffer_alloc(TEST_DATA_SIZE);
	if (buff_ptr == NULL) {
		printf("FAILED did not get buffer\n");
	}

	uint8_t test_data[64];
	create_test_data(test_data, 64);

	bool result = rcp_buffer_write(buff_ptr, test_data, TEST_DATA_SIZE);

	if (!result) {
		printf("FAILED to qrite to buffer\n");
	}

	// Check buffer
	buffer_count = 0;
	for (int i = 0; i < buff_ptr->data_size; i++) {
		if (rcp_buffer_data_ptr(buff_ptr)[i] != test_data[i]) {
			printf("FAILED");
		} else {
			buffer_count++;
		}
	}

	if (buffer_count == TEST_DATA_SIZE) {
		printf("PASSED\n");
	} else {
		printf("FAILED\n");
	}

	printf("Test: Use read_from_buffer: ");
	int data_correct = 0;
	uint8_t temp_buff[TEST_DATA_SIZE];
	uint16_t data_read = rcp_buffer_read(temp_buff, buff_ptr, buff_ptr->data_size);

	if (data_read != buff_ptr->data_size) {
		printf("FAILED expected to read %d got %d\n", data_read, buff_ptr->data_size);
	} else {
		for (int i = 0; i < buff_ptr->data_size; i++) {
			if (temp_buff[i] == test_data[i]) {
				data_correct++;
			}
		}
		if (data_correct == data_read) {
			printf("PASSED\n");
		} else {
			printf("FAILED data mismatch\n");
		}
	}

	printf("Test Random Get/Release: ");

	// Reset buffers
	rcp_buffer_pool_init();

	for (int i = 0; i < RCP_NUM_BUFFERS; i++) {
		acquired_buff[i] = NULL;
	}

	int acquired_buffers = 0;
	int selected_idx;
	int passes = 0;
	for (int i = 0; i < 500; i++) {
		passes++;
		selected_idx = (int)(((double)rand() / (RAND_MAX)) * (RCP_NUM_BUFFERS - 1));

		if (selected_idx >= RCP_NUM_BUFFERS) {
			printf("****> Invalid Index %d\n", selected_idx);
			continue;
		}

		if (acquired_buff[selected_idx] == NULL) {
			// No buffer - get One
			acquired_buff[selected_idx] = rcp_buffer_alloc(TEST_DATA_SIZE);
		} else {
			if (acquired_buff[selected_idx]->buffer_ctrl == NULL) {
				printf("\nRelease external buffer\n");
			}
			// Release acquired buffer
			buff_ptr = acquired_buff[selected_idx];
			rcp_buffer_free(&buff_ptr);
			acquired_buff[selected_idx] = NULL;
		}
	}
	printf("PASSED\n");

	// Reset buffers
	rcp_buffer_pool_init();

	printf("Test: write_to_buffer invalid buffer return false: ");

	buff_ptr = NULL;

	result = rcp_buffer_write(buff_ptr, test_data, TEST_DATA_SIZE);

	if (!result) {
		rcp_buffer_hdl_t *temp_ptr = buff_ptr;
		rcp_buffer_free(&buff_ptr);

		result = rcp_buffer_write(temp_ptr, test_data, TEST_DATA_SIZE);
		if (!result) {
			printf("PASSED\n");
		}
	}

	printf("Test: read_from_buffer without data returns 0: ");
	buff_ptr = rcp_buffer_alloc(TEST_DATA_SIZE);
	uint8_t xfr_size = rcp_buffer_read(test_data, buff_ptr, TEST_DATA_SIZE);
	if (xfr_size == 0) {
		printf("PASSED\n");
	} else {
		printf("FAILED %d <> %d\n", xfr_size, 0);
	}

	printf("Test: read_from_buffer request to get more data than available: ");
	uint16_t data_size = TEST_DATA_SIZE / 2;
	create_test_data(rcp_buffer_data_ptr(buff_ptr), data_size);
	buff_ptr->data_size = data_size;

	xfr_size = rcp_buffer_read(test_data, buff_ptr, TEST_DATA_SIZE);

	if (xfr_size == TEST_DATA_SIZE / 2) {
		printf("PASSED\n");
	} else {
		printf("FAILED %d <> %d\n", xfr_size, (TEST_DATA_SIZE / 2));
	}

#ifdef RCP_OS_LINUX

	printf("Test: Create an external buffer, pass ownership to RCP: ");

	void *allocated_mem = malloc(128);
	buff_ptr = rcp_wrap_external_buffer(RCP_EXT_BUFF_TEST, allocated_mem, 128);

	if (buff_ptr) {
		printf("PASSED\n");
	} else {
		printf("FAILED - did not get buffer\n");
	}

	printf("Test: Write to external Buffer: ");
	create_test_data(test_data, TEST_DATA_SIZE);
	result = rcp_buffer_write(buff_ptr, test_data, TEST_DATA_SIZE);

	if (result) {
		printf("PASSED\n");
	} else {
		printf("FAILED - to write to buffer");
	}

	printf("Test: Read from external Buffer: ");
	uint8_t rx_data[TEST_DATA_SIZE];
	xfr_size = rcp_buffer_read(rx_data, buff_ptr, TEST_DATA_SIZE);

	if (xfr_size == TEST_DATA_SIZE) {
		int correct = 0;
		for (int i = 0; i < TEST_DATA_SIZE; i++) {
			if (rx_data[i] == test_data[i]) {
				correct++;
			}
		}
		if (correct == TEST_DATA_SIZE) {
			printf("PASSED\n");
		} else {
			printf("FAILED Correct data = %d/%d\n", correct, TEST_DATA_SIZE);
		}
	} else {
		printf("Failed got result %d expected %d\n", xfr_size, TEST_DATA_SIZE);
	}

	printf("Test: Release external data buffer: ");
	rcp_buffer_free(&buff_ptr);
	if (buff_ptr == NULL) {
		printf("PASSED\n");
	} else {
		printf("FAILED\n");
	}

#endif

	return 0;
}
