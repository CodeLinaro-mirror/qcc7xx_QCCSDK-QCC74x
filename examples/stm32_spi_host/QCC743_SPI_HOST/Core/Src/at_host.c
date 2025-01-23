/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"
#include "at_host.h"
/* Private includes ----------------------------------------------------------*/

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static const osThreadAttr_t _rx_task_attr = {
  .name = "at_rx",
  .priority = (osPriority_t) osPriorityRealtime6,
  .stack_size = 4096
};

typedef struct ota_header {
    union {
        struct {
            uint8_t header[16];

            uint8_t type[4]; //RAW XZ
            uint32_t len;    //body len
            uint8_t pad0[8];

            uint8_t ver_hardware[16];
            uint8_t ver_software[16];

            uint8_t sha256[32];
        } s;
        uint8_t _pad[512];
    } u;
} at_ota_header_t;

int countDigits(int number)
{
    if (number < 0) {
        number = -number;
    }

    if (number == 0) {
        return 1;
    }

    int count = 0;
    while (number != 0) {
        number /= 10;
        count++;
    }
    return count;
}

struct _fount_list {
	at_response_t *cmd;
	char *found_ptr;
	uint32_t len;
};

static void at_cmd_find(at_host_handle_t handle, const char *cmd_name, int len, struct _fount_list *found_list, uint8_t list_num)
{
	int i, j, list = 0;
	char *cmd;
	at_response_t *found_cmd = NULL;

	memset(found_list, 0, sizeof(struct _fount_list) * list_num);

#if 0
	for (j = 0; j < len; j++) {
		for (i = 0; i < handle->at_resp_nums; i++) {
			cmd = handle->at_resp_cmd[i]->cmd;
			if (strncmp(cmd_name+j, cmd, strlen(cmd)) == 0) {
				found_list[list].cmd = handle->at_resp_cmd[i];
				found_list[list].found_ptr = cmd_name + j;
				list++;
				found_list[list].found_ptr = cmd_name + len;
				//printf("found %s\r\n", handle->at_resp_cmd[i]->cmd);
			}
		}
	}
	for (list = 0; list < handle->at_resp_nums - 1; list++) {
		if (found_list[list].found_ptr) {
			found_list[list].len = found_list[list+1].found_ptr - found_list[list].found_ptr;
		}
	}
#else
    for (list = 0; list < handle->at_resp_nums; list++) {
		if ((cmd = strnstr(cmd_name, handle->at_resp_cmd[list]->cmd, strlen(handle->at_resp_cmd[list]->cmd))) != NULL) {
            found_list[list].cmd = handle->at_resp_cmd[list];
			found_list[list].found_ptr = cmd;
            found_list[list].len = len - (cmd - cmd_name);
			list++;
            break;
        }
	}
#endif
}

static int _at_read(at_host_handle_t handle, uint8_t *buf, uint32_t len)
{
	int ret = 0;
	if (handle->ops && handle->ops->f_read_data) {
		ret = handle->ops->f_read_data(handle->arg, buf, len);
	}

	if (ret > 0 && handle->rx_cb) {
		ret = handle->rx_cb(buf, ret, handle->receive_cb_arg);
	}
	return ret;
}

static void __at_rx_task(void *arg)
{
	int ret;
	at_host_handle_t handle = (at_host_handle_t)arg;
	static char evt_head[5*1024];
	struct _fount_list found_list[10];

	printf("at rx start\r\n");

	while (1) {

		//memset(evt_head, 0, sizeof(evt_head));
		ret = _at_read(handle, evt_head, sizeof(evt_head));

		if (ret > 0) {

			char *cmd_head = evt_head;

			at_cmd_find(handle, cmd_head, ret, found_list, sizeof(found_list)/sizeof(struct _fount_list));

			for (int i = 0; i < sizeof(found_list)/sizeof(struct _fount_list); i++) {
				if (found_list[i].cmd && found_list[i].cmd->at_response_func) {
					found_list[i].cmd->at_response_func(handle, found_list[i].found_ptr, found_list[i].len);
				}
			}
		}
	}
}

static int at_resp_ok(at_host_handle_t at, const char *cmd, int len)
{
	osEventFlagsSet(at->evt, AT_HOST_RESP_EVT_OK);
	//printf("at_resp_ok\r\n");
	return 0;
}

static int at_resp_wait_data(at_host_handle_t at, const char *cmd, int len)
{
	osEventFlagsSet(at->evt, AT_HOST_RESP_EVT_WAIT_DATA|AT_HOST_RESP_EVT_OK);
	//printf("at_resp_wait_data\r\n");
	return 0;
}

static int at_resp_send_ok(at_host_handle_t at, const char *cmd, int len)
{
	osEventFlagsSet(at->evt, AT_HOST_RESP_EVT_SEND_OK);
	return 0;
}

static int at_resp_recv_btyes(at_host_handle_t at, const char *cmd, int len)
{
	int data_len = 0;

    sscanf(cmd, "Recv %d bytes\r\n", &data_len);

    if (data_len > 0) {
    	osEventFlagsSet(at->evt, AT_HOST_RESP_EVT_RECV_BTYES);
    }

	return 0;
}


static int at_resp_ciprecvdata(at_host_handle_t at, const char *cmd, int len)
{
	int ret = 0;
	uint8_t head_len = 0;
	uint8_t *data_buf = NULL, *wptr;
	int frame_len, data_len = 0, recv_len = 0;
	at_host_msg_t msg;
	AT_HOST_DEBUG_POINT();
    sscanf(cmd, "+CIPRECVDATA:%d,", &data_len);

	if (!strstr(cmd, ",")) {
		return len;
	}

	if (data_len == 0) {
		osEventFlagsClear(at->evt, AT_HOST_RESP_EVT_IPD);
		goto _end;
	}
	AT_HOST_DEBUG_POINT();
	data_buf = pvPortCalloc(data_len + 2, 1);
	if (!data_buf) {
		printf("malloc fail len:%d\r\n", data_len);
		return 0;
	}

	wptr = data_buf;
	head_len = countDigits(data_len) + 14;
	frame_len = head_len + data_len + strlen("\r\n");

	ret = ((len - head_len) > (data_len + 2)) ? (data_len + 2) : (len - head_len);
	if (ret > 0) {
		memcpy(data_buf, cmd + head_len, ret);
		wptr = data_buf + ret;
	}
	AT_HOST_DEBUG_POINT();
	len = ret;
	while (len < data_len + 2) {
		recv_len = _at_read(at, data_buf + len, data_len + 2 - len);
		if (recv_len > 0) {
			len += recv_len;
		}
	}
	AT_HOST_DEBUG_POINT();
	if (len + head_len > frame_len) {
		printf("frame len:%d len:%d\r\n", frame_len, len);
	}

	if (data_buf[data_len] == '\r' && data_buf[data_len + 1] == '\n') {

	} else {
		//printf("frame tail %s\r\n", &data_buf[data_len]);
		//assert(0);
	}
_end:
	msg.event = AT_HOST_RESP_EVT_CIPRECVDATA;
	msg.msg_buf = data_buf;
	msg.msg_len = data_len;
	ret = osMessageQueuePut(at->queue, &msg, 0, 0);
	if (ret != 0) {
		printf("msg send fail\r\n");
	}
    return 0;
}

static int at_resp_ips(at_host_handle_t at, const char *cmd, int len)
{
	return 0;
}

static int at_resp_ipd(at_host_handle_t at, const char *cmd, int len)
{
	int linkid = 0;
	int ret = 0;
	uint8_t head_len = 0;
	uint8_t *data_buf = NULL;
	int frame_len, data_len = 0, recv_len = 0;
    
    sscanf(cmd, "+IPD:%d,%d", &linkid, &data_len);

    if (at->recv_mode == AT_NET_RECV_MODE_ACTIVE) {

    	// TODO: recv_mode == AT_NET_RECV_MODE_ACTIVE
#if 0
        if (!strstr(cmd, ":")) {
        	return len;
        }

        head_len = countDigits(linkid) + countDigits(data_len) + 9;
        frame_len = head_len + data_len + strlen("\r\n");

        if ((ret = len - head_len) > 0) {
        	memcpy(at->rxbuf, cmd + head_len, ret);
        	data_buf = at->rxbuf + ret;
        }

        while (len < frame_len) {
        	recv_len = _at_read(at, data_buf + recv_len, frame_len - len);
        	if (recv_len > 0) {
        		len += recv_len;
        	}
        }

        if (len > frame_len) {
        	printf("frame len:%d len:%d\r\n", frame_len, len);
        }

        if (at->rxbuf[data_len] == '\r' && at->rxbuf[data_len + 1] == '\n') {

        } else {
        	printf("frame err !!!\r\n");
        }
#endif
    } else {
    	data_buf = NULL;
        osEventFlagsSet(at->evt, AT_HOST_RESP_EVT_IPD);
    }

    at->recv_len_totle += data_len;

    if (at->net_data_recv) {
    	at->net_data_recv(linkid, data_buf, data_len, at->net_data_recv_arg);
    }
	return 0;
}

int at_host_recvmode_set(at_host_handle_t at, uint8_t mode)
{
	at_host_printf(at, AT_HOST_RESP_EVT_OK, 0, "AT+CIPRECVMODE=%d\r\n", mode);
	at->recv_mode = mode;
	return 0;
}

int at_host_recvdata(at_host_handle_t at, int linkid, uint8_t *buf, uint32_t buf_size, uint32_t timeout)
{
	at_host_msg_t msg;
	int ret = 0;

	if (at->last_recvdata_len == 0) {
		if (!(at_host_wait(at, AT_HOST_RESP_EVT_IPD, 1000) & AT_HOST_RESP_EVT_IPD)) {
			return -1;
		}
	}

	AT_HOST_DEBUG_POINT();
	at_host_printf(at, 0, timeout, "AT+CIPRECVDATA=%d,%d\r\n", linkid, buf_size);
	AT_HOST_DEBUG_POINT();
	if (osMessageQueueGet(at->queue, &msg, 0, timeout) != 0) {
		return -1;
	}
	AT_HOST_DEBUG_POINT();
	//at_host_wait(at, AT_HOST_RESP_EVT_OK, 1000);
	//AT_HOST_DEBUG_POINT();
	if (msg.msg_len) {
		ret = buf_size > msg.msg_len ? msg.msg_len : buf_size;
		memcpy(buf, msg.msg_buf, ret);
	}
	at->last_recvdata_len = msg.msg_len;

	if (msg.msg_buf) {
		vPortFree(msg.msg_buf);
	}
	return ret;
}

int at_host_wait(at_host_handle_t at, int wait_evt, uint32_t timeout)
{
	int rflags = osEventFlagsWait(at->evt, wait_evt, osFlagsWaitAll, timeout);

	if (rflags >= osErrorISR && rflags <= osOK) {
		return 0;
	}
	return rflags;
}

int at_host_send(at_host_handle_t at, int wait_evt, uint8_t *data, int len, uint32_t timeout)
{
	int ret = 0;
	osEventFlagsClear(at->evt, wait_evt);

	if (at && at->ops->f_write_data) {
		ret = at->ops->f_write_data(at->arg, data, len);
	}

	//AT_HOST_DEBUG_POINT();

	if (wait_evt) {
		if (!(at_host_wait(at, wait_evt, timeout) & wait_evt)) {
			ret = -1;
		}
	}
	return ret;
}

int at_host_read(at_host_handle_t at, uint8_t *buf, uint32_t buf_size)
{
	return at->ops->f_read_data(at->arg, buf, buf_size);
}

int at_host_printf(at_host_handle_t at, int wait_evt, uint32_t timeout, const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	int len, ret;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	ret = (len > sizeof(buf)) ? sizeof(buf) : len;

	if (at_host_send(at, wait_evt, buf, len, timeout) < 0) {
		ret = -1;
	}
	//AT_HOST_DEBUG_POINT();
	return ret;
}

void at_host_receive_register(at_host_handle_t at, at_receive_cb_t cb, void *arg)
{
	at->rx_cb = cb;
	at->receive_cb_arg = arg;
}

void at_host_net_recv_register(at_host_handle_t at, int (*net_data_recv)(int linkid, uint8_t *buf, uint32_t size, void *arg), void *arg)
{
	at->net_data_recv = net_data_recv;
	at->net_data_recv_arg = arg;
}

static int at_resp_register(at_host_handle_t at, const at_response_t *cmd)
{
	int i;

	if (!cmd->cmd) {
		return -1;
	}

	if (at->at_resp_nums < AT_HOST_RESP_CMD_MAX) {
		for (i = 0; i < at->at_resp_nums; i++) {
			if (at->at_resp_cmd[i] == cmd) {
				return 0;
			}
		}
		at->at_resp_cmd[at->at_resp_nums++] = (at_response_t *)cmd;
		return 0;
	}
	return -1;
}

int at_host_response_register(at_host_handle_t at, const at_response_t *cmd, int nums)
{
	int i, err;

	for (i = 0; i < nums; i++) {
		if ((err = at_resp_register(at, cmd++)) != 0) {
			return err;
		}
	}
	return 0;
}

static const at_response_t at_response_cmd[] = {
	{"\r\nOK\r\n\r\n>", at_resp_wait_data},
	{"\r\nOK\r\n", at_resp_ok},
	//{"\r\n+CIP", at_resp_ips},
	{"+IPD", at_resp_ipd},
	{"\r\nSEND OK\r\n", at_resp_send_ok},
	{"+CIPRECVDATA:", at_resp_ciprecvdata},
	{"Recv ", at_resp_recv_btyes},
};

at_host_handle_t at_host_init(const struct at_host_drv *ops, void *arg)
{
	at_host_handle_t handle = pvPortMalloc(sizeof(struct at_host));

	memset(handle, 0, sizeof(struct at_host));

	handle->arg = arg;
	handle->ops = ops;
	handle->at_rx_task = osThreadNew(__at_rx_task, handle, &_rx_task_attr);
	if (!handle->at_rx_task) {
		printf("failed to create rx task\r\n");
		vPortFree(handle);
		return NULL;
	}

	handle->queue = osMessageQueueNew(AT_HOST_MSG_QUEUE_COUNT_MAX, sizeof(at_host_msg_t), NULL);
	if (!handle->queue) {
		printf("failed to create sem\r\n");
		vPortFree(handle);
		return NULL;
	}

	handle->evt = osEventFlagsNew(NULL);
	if (!handle->evt) {
		printf("failed to create sem\r\n");
		vPortFree(handle);
		return NULL;
	}

	handle->last_recvdata_len = 0;
	at_host_response_register(handle, at_response_cmd, sizeof(at_response_cmd)/sizeof(at_response_cmd[0]));

	return handle;
}

