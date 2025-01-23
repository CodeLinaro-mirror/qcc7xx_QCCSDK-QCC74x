/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"
#include "app_atmodule.h"
#include "semphr.h"
#include "stream_buffer.h"
/* Private includes ----------------------------------------------------------*/

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "spi.h"
/* USER CODE END Includes */

const osThreadAttr_t at_tx_task_attr = {
  .name = "at_tx",
  .priority = (osPriority_t) osPriorityRealtime7,
  .stack_size = 4096
};

const osThreadAttr_t at_rx_task_attr = {
  .name = "at_rx",
  .priority = (osPriority_t) osPriorityRealtime7,
  .stack_size = 4096
};

static const osThreadAttr_t _app_task_attr = {
  .name = "app_task",
  .priority = (osPriority_t) osPriorityRealtime7,
  .stack_size = 4096
};

static int _at_to_console(uint8_t *buf, uint32_t len, void *arg);
static int _read_data(void *arg, void *data, int len);
static int _net_data_recv(int linkid, uint8_t *buf, uint32_t size, void *arg);
static void __app_task(void *arg);

void at_gpio_debug()
{
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_7);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_7);
}

static int __at_iperf_statis(uint8_t *buf, uint32_t size, void *arg)
{
//	int linkid, data_len = 0;
	static uint32_t last = 0;
	static uint32_t totle_len = 0;

//	if (strncmp("\r\n+IPD:", buf, strlen("\r\n+IPD:")) != 0) {
//		return 0;
//	}
//    sscanf(buf, "\r\n+IPD:%d,%d", &linkid, &data_len);

	totle_len += size;
	if (!last) {
		last = osKernelGetTickCount();
	}
	if (osKernelGetTickCount() - last >= 1000) {
		last = osKernelGetTickCount();
		printf("RX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
		totle_len = 0;
	}
	return 0;
}
#define TEST_BUFFER_SIZE    (6000)
#define TEST_RX_BUFFER_SIZE (6000)
int at_iperf_udp_tx_start(at_host_handle_t at, char ip_addr[20], int port)
{
	static uint32_t last = 0;
	static uint32_t totle_len = 0;
	int ret, run_count = 0;
	uint8_t *tx_buf;
	uint8_t cmd[32];
	int cmd_len;

	at_host_receive_register(at, NULL, NULL);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(100);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSTART=0,\"UDP\",\"%s\",%d\r\n", ip_addr, port);
    osDelay(100);

    tx_buf = pvPortMalloc(TEST_BUFFER_SIZE);
	memset(tx_buf, 0x55, TEST_BUFFER_SIZE);

	last = osKernelGetTickCount();

	cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%d\r\n", TEST_BUFFER_SIZE);

    while (run_count <= 10) {
    	at_host_send(at, AT_HOST_RESP_EVT_OK|AT_HOST_RESP_EVT_WAIT_DATA, cmd, cmd_len, 1000);

    	ret = at_host_send(at, AT_HOST_RESP_EVT_RECV_BTYES, tx_buf, TEST_BUFFER_SIZE, 100);
    	if (ret > 0) {
    		totle_len += ret;
    	}
    	if (osKernelGetTickCount() - last >= 1000) {
    		last = osKernelGetTickCount();
    		printf("TX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
    		totle_len = 0;
    		run_count++;
    	}
    }
    printf("iperf exit\r\n");
    osDelay(200);
    vPortFree(tx_buf);
    at_iperf_stop(at, 1);
    return 0;
}

#define AT_RESP_OK        "\r\nOK\r\n"
#define AT_RESP_WAIT_DATA "\r\n>"
#define AT_RESP_RECV_CFM  "Recv 1024 bytes\r\n"
#define AT_RESP_SEND_OK   "\r\nSEND OK\r\n"

int at_iperf_tcp_tx_start(at_host_handle_t at, char ip_addr[20], int port)
{
	static uint32_t last = 0;
	static uint32_t totle_len = 0;
	int ret, run_count = 0;
	uint8_t *tx_buf;
	uint8_t cmd[32];
	int cmd_len;

	at_host_receive_register(at, NULL, NULL);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(100);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSTART=0,\"TCP\",\"%s\",%d\r\n", ip_addr, port);
    osDelay(100);

    tx_buf = pvPortMalloc(TEST_BUFFER_SIZE);
	memset(tx_buf, 0x55, TEST_BUFFER_SIZE);

	last = osKernelGetTickCount();

	cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%d\r\n", TEST_BUFFER_SIZE);

    while (run_count <= 10) {
    	at_host_send(at, AT_HOST_RESP_EVT_OK|AT_HOST_RESP_EVT_WAIT_DATA, cmd, cmd_len, 1000);

    	ret = at_host_send(at, AT_HOST_RESP_EVT_RECV_BTYES, tx_buf, TEST_BUFFER_SIZE, 1000);
    	if (ret > 0) {
    		totle_len += ret;
    	}
    	if (osKernelGetTickCount() - last >= 1000) {
    		last = osKernelGetTickCount();
    		printf("TX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
    		totle_len = 0;
    		run_count++;
    	}
    }

    printf("iperf exit\r\n");
    vPortFree(tx_buf);
    osDelay(200);
    at_iperf_stop(at, 1);
    return 0;
}

int at_iperf_udp_rx_start(at_host_handle_t at, char ip_addr[20], int port)
{
	static uint32_t last = 0;
	static uint32_t totle_len = 0;
	int ret, run_count = 0;
	uint8_t *rx_buf;

	at_host_recvmode_set(at, AT_NET_RECV_MODE_PASSIVE);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(10);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPRECVBUF=0,%d\r\n",12*1024);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CWEVT=0\r\n");
    osDelay(10);
//    at_host_printf(at, AT_HOST_RESP_EVT_OK, "AT+CIPEVT=0\r\n");
//    osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSERVER=1,%d,\"UDP\"\r\n", port);
    osDelay(100);

    rx_buf = pvPortMalloc(TEST_RX_BUFFER_SIZE);
	memset(rx_buf, 0x55, TEST_RX_BUFFER_SIZE);

    //at_host_net_recv_register(at, _net_data_recv, evt);
	at_host_receive_register(at, NULL, NULL);

	last = osKernelGetTickCount();

    while (run_count <= 10) {
    	ret = at_host_recvdata(at, 0, rx_buf, TEST_RX_BUFFER_SIZE, 1000);
    	if (ret <= 0) {
    		continue;
    	}
    	if (ret > 0) {
    		totle_len += ret;
    	}
    	if (osKernelGetTickCount() - last >= 1000) {
    		last = osKernelGetTickCount();
    		printf("RX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
    		totle_len = 0;
    		run_count++;
    	}
    }
    printf("iperf exit\r\n");
    vPortFree(rx_buf);
    osDelay(200);
    at_iperf_stop(at, 0);
    return 0;
}

int at_iperf_tcp_rx_start(at_host_handle_t at, char ip_addr[20], int port)
{
	static uint32_t last = 0;
	static uint32_t totle_len = 0;
	int ret, run_count = 0;
	uint8_t *rx_buf;

	at_host_recvmode_set(at, AT_NET_RECV_MODE_PASSIVE);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(10);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPRECVBUF=0,%d\r\n",12*1024);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CWEVT=0\r\n");
    osDelay(10);
//    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPEVT=0\r\n");
//    osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSERVER=1,%d,\"TCP\"\r\n", port);
    osDelay(100);

    rx_buf = pvPortMalloc(TEST_RX_BUFFER_SIZE);
	memset(rx_buf, 0x55, TEST_RX_BUFFER_SIZE);

    //at_host_net_recv_register(at, _net_data_recv, evt);
	at_host_receive_register(at, NULL, NULL);

	last = osKernelGetTickCount();

    while (run_count <= 10) {
    	ret = at_host_recvdata(at, 0, rx_buf, TEST_RX_BUFFER_SIZE, 1000);
    	if (ret <= 0) {
    		continue;
    	}
    	if (ret > 0) {
    		totle_len += ret;
    	}
    	if (osKernelGetTickCount() - last >= 1000) {
    		last = osKernelGetTickCount();
    		printf("RX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
    		totle_len = 0;
    		run_count++;
    	}
    }

    printf("iperf exit\r\n");
    vPortFree(rx_buf);
    osDelay(200);
    at_iperf_stop(at, 0);
    return 0;
}

int at_iperf_stop(at_host_handle_t at, int is_cli)
{
    //at_host_printf(at, 0, "+++");
    //osDelay(100);

    if (is_cli) {
        at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPCLOSE=0\r\n");
        osDelay(100);
    } else {
        at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSERVER=0,1\r\n");
        osDelay(100);
    }
	at_host_receive_register(at, _at_to_console, NULL);

	return 0;
}

#define RESP_IPD                     (1)
#define RESP_CIPRECVDATA             (2)

static int wait_response(at_host_handle_t at, int *type, uint8_t *buf, uint32_t bufsize)
{
	int linkid, ret = 0;
	//struct spi_buffer *buffer = NULL;

	while (1) {
		//ret = spi_read_buffer(&buffer, portMAX_DELAY);
		ret = at_host_read(at, buf, bufsize);

    	if (ret > 0) {
    		if (ret < bufsize) {
    			buf[ret] = 0;
    		}

    		if (strncmp("+CIPRECVDATA:", buf, 13) == 0) {
    			sscanf(buf, "+CIPRECVDATA:%d", &ret);
    			*type = RESP_CIPRECVDATA;
    			break;
    		} else if (strncmp("+IPD:", buf, 5) == 0) {
				sscanf(buf, "+IPD:%d,%d", &linkid, &ret);
				*type = RESP_IPD;
				break;
			} else {
				//spi_buffer_free(buffer);
				//printf("xx %s\r\n",data);
			}
    	}
	}
	//*buf = buffer;
	return ret;
}

static int wait_ciprecvbuf(at_host_handle_t at, uint8_t *buf, int bufsize)
{
	int ret = 0, type = 0;
	//struct spi_buffer *buffer;

	do {
		ret = wait_response(at, &type, buf, bufsize);
//		if (ret > 0) {
//			if (buf) {
//				memcpy(buf, buffer->data, (bufsize > buffer->len) ? buffer->len : bufsize);
//			}
//			spi_buffer_free(buffer);
//		}
	} while(type != RESP_CIPRECVDATA);
	return ret;
}

static int wait_ipd(at_host_handle_t at)
{
	int ret = 0, type = 0;
	uint8_t data[32];
	//struct spi_buffer *buffer;

	do {
		ret = wait_response(at, &type, data, sizeof(data));
//		if (ret > 0) {
//			spi_buffer_free(buffer);
//		}
	} while(type != RESP_IPD);
	return ret;
}

int at_iperf_udp_rx_fast_start(at_host_handle_t at, char ip_addr[20], int port)
{
	uint32_t last = 0;
	uint32_t totle_len = 0;
	int ret, run_count = 0;
	uint8_t *rx_buf;
	uint8_t cmd[32];
	uint32_t cmd_len, recv_len = 0;
	int type;
	//struct spi_buffer *buf;

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(10);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPRECVBUF=0,%d\r\n",12*1024);
	osDelay(10);

	at_host_recvmode_set(at, AT_NET_RECV_MODE_PASSIVE);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CWEVT=0\r\n");
    osDelay(10);
//    at_host_printf(at, AT_HOST_RESP_EVT_OK, "AT+CIPEVT=0\r\n");
//    osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSERVER=1,%d,\"UDP\"\r\n", port);
    osDelay(100);

    rx_buf = pvPortMalloc(TEST_RX_BUFFER_SIZE + 128);
	memset(rx_buf, 0x55, TEST_RX_BUFFER_SIZE + 128);

    //at_host_net_recv_register(at, _net_data_recv, evt);
	at_host_receive_register(at, NULL, NULL);

	cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPRECVDATA=%d,%d\r\n", 0, TEST_RX_BUFFER_SIZE);

	osThreadSuspend(at->at_rx_task);

    while (run_count < 10) {
    	/* Wait the data to be cached TEST_RX_BUFFER_SIZE */
    	recv_len += wait_ipd(at);

    	if (!last) {
    		last = osKernelGetTickCount();
    	}

    	if (recv_len >= TEST_RX_BUFFER_SIZE) {

    		/* Request cached data */
        	at_host_send(at, 0, cmd, cmd_len, 1000);

        	do {
            	ret = wait_response(at, &type, rx_buf, TEST_RX_BUFFER_SIZE + 128);
            	if (ret <= 0) {
            		continue;
            	}
            	if (type == RESP_IPD) {
            		recv_len += ret;
//            		/* Free Buffer after completion */
//            		spi_buffer_free(buf);
            		continue;
            	} else if (type == RESP_CIPRECVDATA) {
            		/* Received the requested data here */
            		/* Do something buf->data, buf->len */
            		totle_len += ret;
            		recv_len -= ret;
//            		/* Free Buffer after completion */
//            		spi_buffer_free(buf);
//            		buf = NULL;
            		break;
            	}
        	} while (1);

    	}

    	if (osKernelGetTickCount() - last >= 1000) {
    		last = osKernelGetTickCount();
    		printf("RX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
    		totle_len = 0;
    		run_count++;
    	}
    }

    at_iperf_stop(at, 0);

    /* clear the rx buffer*/
    while(1) {
    	at_host_printf(at, 0, 1000, "AT+CIPRECVDATA=%d,%d\r\n", 0, TEST_RX_BUFFER_SIZE);
    	ret = wait_ciprecvbuf(at, rx_buf, TEST_RX_BUFFER_SIZE + 128);
    	if (ret <= 0) {
    		break;
    	}
    }
    printf("iperf exit\r\n");

    osThreadResume(at->at_rx_task);
    vPortFree(rx_buf);
    osDelay(200);

    return 0;
}

int at_iperf_tcp_rx_fast_start(at_host_handle_t at, char ip_addr[20], int port)
{
	uint32_t last = 0;
	uint32_t totle_len = 0;
	int type, ret, run_count = 0;
	uint8_t *rx_buf;
	uint8_t cmd[32];
	uint32_t cmd_len, recv_len = 0;
	//struct spi_buffer *buf;

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(10);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPRECVBUF=0,%d\r\n",12*1024);
	osDelay(10);

	at_host_recvmode_set(at, AT_NET_RECV_MODE_PASSIVE);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CWEVT=0\r\n");
    osDelay(10);
//    at_host_printf(at, AT_HOST_RESP_EVT_OK, "AT+CIPEVT=0\r\n");
//    osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSERVER=1,%d,\"TCP\"\r\n", port);
    osDelay(100);

    rx_buf = pvPortMalloc(TEST_RX_BUFFER_SIZE + 128);
	memset(rx_buf, 0x55, TEST_RX_BUFFER_SIZE + 128);

    //at_host_net_recv_register(at, _net_data_recv, evt);
	at_host_receive_register(at, NULL, NULL);

	cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPRECVDATA=%d,%d\r\n", 0, TEST_RX_BUFFER_SIZE);

	osThreadSuspend(at->at_rx_task);

    while (run_count < 10) {
    	/* Wait the data to be cached TEST_RX_BUFFER_SIZE */
    	recv_len += wait_ipd(at);

    	if (!last) {
    		last = osKernelGetTickCount();
    	}

    	if (recv_len >= TEST_RX_BUFFER_SIZE) {

    		/* Request cached data */
        	at_host_send(at, 0, cmd, cmd_len, 1000);

        	do {
            	ret = wait_response(at, &type, rx_buf, TEST_RX_BUFFER_SIZE + 128);
            	if (ret <= 0) {
            		continue;
            	}
            	if (type == RESP_IPD) {
            		recv_len += ret;
//            		/* Free Buffer after completion */
//            		spi_buffer_free(buf);
            		continue;
            	} else if (type == RESP_CIPRECVDATA) {
            		/* Received the requested data here */
            		/* Do something buf->data, buf->len */
            		totle_len += ret;
            		recv_len -= ret;
//            		/* Free Buffer after completion */
//            		spi_buffer_free(buf);
//            		buf = NULL;
            		break;
            	}
        	} while (1);

    	}

    	if (osKernelGetTickCount() - last >= 1000) {
    		last = osKernelGetTickCount();
    		printf("RX Bandwidth: %f Mbps\r\n", (float)totle_len*8/1000/1000);
    		totle_len = 0;
    		run_count++;
    	}
    }

    at_iperf_stop(at, 0);

    /* clear the rx buffer*/
    while(1) {
    	at_host_printf(at, 0, 1000, "AT+CIPRECVDATA=%d,%d\r\n", 0, TEST_RX_BUFFER_SIZE);
    	ret = wait_ciprecvbuf(at, rx_buf, TEST_RX_BUFFER_SIZE + 128);
    	if (ret <= 0) {
    		break;
    	}
    }
    printf("iperf exit\r\n");

    osThreadResume(at->at_rx_task);
    vPortFree(rx_buf);
    osDelay(200);

    return 0;
}

SemaphoreHandle_t g_recvsem;

static int _at_to_console(uint8_t *buf, uint32_t len, void *arg)
{
    for (int i = 0; i < len; i++) {
        putchar(buf[i]);
    }
    fflush(stdout);
    return len;
}

static int _net_data_recv(int linkid, uint8_t *buf, uint32_t size, void *arg)
{
	osEventFlagsId_t evt = (osEventFlagsId_t)arg;

	if (linkid == 0) {
		if (buf == NULL) {
			//recv_size = at_host_recvdata(at, linkid, recvdata, size);
			//at_ota_update(at, recvdata, recv_size);
			//xSemaphoreGive(g_recvsem);
			//osEventFlagsSet(evt, 1);
		}
	}
	return 0;
}

static int _read_data(void *arg, void *data, int len)
{
	int ret;
	struct spi_msg m;

	SPI_MSG_INIT(m, data, len, NULL, 0);
	ret = spi_read(&m, pdMS_TO_TICKS(10000));
	if (ret <= 0) {
		//printf("=====at failed to read from spi, %d\r\n", ret);
	} else {
		//printf("=====spi read %d\r\n", ret);
	}
#if 0
	if (ret > 0) {
		int max = ret > 25 ? 25 : ret;
		printf("[read] %d===>", ret);
		for (int i = 0; i < max; i++) {
			printf("%c", ((char *)data)[i]);
		}
		printf("\r\n");
	}
#endif
	return ret;
}

static int _write_data(void *arg, const void *data, int len)
{
	int ret;
	struct spi_msg m;

	SPI_MSG_INIT(m, data, len, NULL, 0);
	ret = spi_write(&m, pdMS_TO_TICKS(10000));
	if (ret <= 0) {
		//printf("at failed to write to spi, %d\r\n", ret);
	}
#if 0
	int max = ret > 25 ? 25 : ret;
	printf("[write] %d===>", ret);
	for (int i = 0; i < max; i++) {
		printf("%c", ((char *)data)[i]);
	}
	printf("\r\n");
#endif
	return ret;
}


static uint8_t ota_started = 0;
static uint8_t ota_exit = 1;

int at_ota_start(at_host_handle_t at, char ip_addr[20], int port)
{
	if (ota_started || (!ota_exit)) {
		return -1;
	}
	at_host_recvmode_set(at, AT_NET_RECV_MODE_PASSIVE);
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPMUX=1\r\n");
    osDelay(10);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPRECVBUF=0,8192\r\n");
	osDelay(10);

    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CWEVT=0\r\n");
    osDelay(10);

//    at_host_printf(at, AT_HOST_RESP_EVT_OK, "AT+CIPEVT=0\r\n");
//    osDelay(10);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+OTASTART=1\r\n");
	osDelay(10);

	at_host_net_recv_register(at, _net_data_recv, at);
	at_host_receive_register(at, NULL, NULL);

	at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPSTART=0,\"TCP\",\"%s\",%d\r\n", ip_addr, port);
	osDelay(100);

    ota_started = 1;
	osThreadNew(__app_task, at, &_app_task_attr);

	return 0;
}

static uint32_t file_size, ota_size = 0;
static uint8_t ota_ishead = 0;

int at_ota_finish(at_host_handle_t at)
{
	ota_ishead = 0;
	file_size = 0;
	ota_size = 0;

    at_host_printf(at, AT_HOST_RESP_EVT_NONE, 100, "AT+CIPCLOSE=0\r\n");
    osDelay(10);
    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CWEVT=1\r\n");
    osDelay(10);
    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+CIPEVT=1\r\n");
    osDelay(10);
    at_host_printf(at, AT_HOST_RESP_EVT_OK, 100, "AT+OTAFIN\r\n");
    osDelay(500);
	ota_started = 0;

    at_host_net_recv_register(at, NULL, NULL);
	at_host_receive_register(at, _at_to_console, NULL);

    return 0;
}

int at_ota_update(at_host_handle_t at, uint8_t *buf, uint32_t len)
{
	uint8_t *ota_buffer = buf;
    at_ota_header_t *ota_header;
    uint32_t write_len = 0;
    static uint32_t cnt = 0;

	if (!ota_ishead) {
		/*
		 * Here's the echo after a successful TCP connection in a single connection scenario.
		 * This test sends the OTA bin immediately after the TCP connection is successful.
		 */
		ota_buffer = strstr(buf, "QCC74X_OTA");
		if (!ota_buffer) {
			return -1;
		}
        ota_header = (at_ota_header_t *)ota_buffer;

        /* If the size of the OTA bin is already known,
         * there is no need to parse the bin size using the ota_head_t type.
         */
        file_size = ota_header->u.s.len + sizeof(at_ota_header_t);

		len = len - (uint32_t)(ota_buffer - buf);
		ota_ishead = 1;
        ota_size = 0;
        cnt = 1;
        printf("OTA start file_size:%d\r\n", file_size);
	}


    at_host_printf(at, AT_HOST_RESP_EVT_OK|AT_HOST_RESP_EVT_WAIT_DATA, (uint32_t)-1, "AT+OTASEND=%d\r\n", len);
    //osDelay(10);

    if (ota_size < file_size) {
        write_len = (ota_size + len > file_size) ? (file_size - ota_size) : len;
        at_host_send(at, AT_HOST_RESP_EVT_SEND_OK, ota_buffer, write_len, (uint32_t)-1);
        //osDelay(10);
    }
    ota_size += write_len;
    printf("0x%02x 0x%02x 0x%02x 0x%02x size:%d, len:%d count:%d\r\n", ota_buffer[0], ota_buffer[1], ota_buffer[2], ota_buffer[3],
    		ota_size, len, cnt++);
    if (ota_size == file_size) {

        at_ota_finish(at);
        return 1;
    }
    return 0;
}

static void __app_task(void *arg)
{
	uint8_t *recvdata = NULL;
	int recv_size;
	at_host_handle_t at = (at_host_handle_t)arg;

	ota_exit = 0;
	recvdata = pvPortMalloc(4096);
	while (ota_started) {
		recv_size = at_host_recvdata(at, 0, recvdata, 4096, (uint32_t)-1);
		if (recv_size <= 0) {
			continue;
		}
		if (at_ota_update(at, recvdata, recv_size) == 1) {
			break;
		}
		//osDelay(10);
	}
	ota_exit = 1;
	printf("OTA process exit\r\n");
	vPortFree(recvdata);
	vTaskDelete(NULL);
}

at_host_handle_t at_spisync_init(void)
{
	static const at_host_drv_t host_drv = {
		.f_init_device = NULL,
		.f_deinit_device = NULL,
		.f_read_data = _read_data,
		.f_write_data = _write_data,
	};
	at_host_handle_t at;

	at = at_host_init(&host_drv, NULL);

	at_host_receive_register(at, _at_to_console, NULL);

	return at;
}

