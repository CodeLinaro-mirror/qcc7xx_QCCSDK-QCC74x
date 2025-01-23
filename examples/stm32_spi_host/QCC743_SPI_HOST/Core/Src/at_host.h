/*
 * at_host.h
 *
 *  Created on: Jun 3, 2024
 */

#ifndef SRC_AT_HOST_H_
#define SRC_AT_HOST_H_

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

#define AT_HOST_RESP_CMD_MAX 48

#define AT_HOST_RESP_EVT_NONE          (0)
#define AT_HOST_RESP_EVT_OK            (1 << 0)
#define AT_HOST_RESP_EVT_WAIT_DATA     (1 << 1)
#define AT_HOST_RESP_EVT_SEND_OK       (1 << 2)
#define AT_HOST_RESP_EVT_CIPRECVDATA   (1 << 3)
#define AT_HOST_RESP_EVT_RECV_BTYES    (1 << 4)
#define AT_HOST_RESP_EVT_IPD           (1 << 5)

#define AT_HOST_DEBUG_POINT()  //at_gpio_debug();

typedef int (*at_receive_cb_t)(uint8_t *buf, uint32_t size, void *arg);
struct at_host;

typedef struct at_response {
	const char *cmd;
	int (*at_response_func)(struct at_host *at, const char *cmd, int len);
} at_response_t;

typedef struct at_host_drv {
	int (*f_init_device)(void *arg);
	int (*f_deinit_device)(void *arg);
	int (*f_read_data)(void *arg, void *data, int len);
	int (*f_write_data)(void *arg, const void *data, int len);
} at_host_drv_t;

typedef struct at_host_msg {
	int event;
	void *msg_buf;
	uint32_t msg_len;
} at_host_msg_t;

typedef struct at_host {
	const struct at_host_drv *ops;
	void *arg;

	osThreadId_t at_rx_task;
	osEventFlagsId_t evt;
#define AT_HOST_MSG_QUEUE_COUNT_MAX 50
	osMessageQueueId_t queue;

	at_receive_cb_t rx_cb;
	void *receive_cb_arg;

	int (*net_data_recv)(int linkid, uint8_t *buf, uint32_t size, void *arg);
	void *net_data_recv_arg;

	at_response_t *at_resp_cmd[AT_HOST_RESP_CMD_MAX];
	uint32_t at_resp_nums;

#define AT_NET_RECV_MODE_ACTIVE  0
#define AT_NET_RECV_MODE_PASSIVE 1
	uint8_t recv_mode;
	uint32_t recv_len_totle;

	uint16_t last_recvdata_len;
	//char rxbuf[1024*3];

} *at_host_handle_t;

at_host_handle_t at_host_init(const struct at_host_drv *ops, void *arg);

void at_host_receive_register(at_host_handle_t at, at_receive_cb_t cb, void *arg);

int at_host_send(at_host_handle_t at, int wait_evt, uint8_t *data, int len, uint32_t timeout);

int at_host_printf(at_host_handle_t at, int wait_evt, uint32_t timeout, const char *fmt, ...);

void at_host_net_recv_register(at_host_handle_t at, int (*net_data_recv)(int linkid, uint8_t *buf, uint32_t size, void *arg), void *arg);

int at_host_wait(at_host_handle_t at, int wait_evt, uint32_t timeout);

int at_host_recvmode_set(at_host_handle_t at, uint8_t mode);

int at_host_recvdata(at_host_handle_t at, int linkid, uint8_t *buf, uint32_t buf_size, uint32_t timeout);

int at_host_read(at_host_handle_t at, uint8_t *buf, uint32_t buf_size);

#endif /* SRC_AT_HOST_H_ */
