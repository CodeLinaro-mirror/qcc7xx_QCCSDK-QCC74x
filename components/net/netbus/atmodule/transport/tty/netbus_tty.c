#include <stdio.h>
#include <stdint.h>
#include <utils_list.h>
#include <FreeRTOS.h>
#include <task.h>

#include "qcc74x_clock.h"
#include "qcc74x_uart.h"
#include <qcc74x_gpio.h>
#include <qcc74x_core.h>
#include <qcc74x_dma.h>
#include "netbus_tty.h"

#define NETBUS_TTY_DEBUG (0)

int netbus_tty_dnld(netbus_tty_ctx_t *pctx, uint8_t *data, uint32_t data_size)
{
    //return data_size;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int res;

#if NETBUS_TTY_DEBUG
    printf("----> dnld[%d] ", data_size);
    for (int i = 0; i < data_size; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
#endif

    // FIXME: only support AT
    if (xPortIsInsideInterrupt()) {
        res = xStreamBufferSendFromISR(pctx->rxbuf, data, data_size, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else {
        res = xStreamBufferSend(pctx->rxbuf, data, data_size, 0);// fixme
    }

    return res;
}

int netbus_tty_init(netbus_tty_ctx_t *pctx, uint32_t txbuf_size, uint32_t rxbuf_size)
{
	// pctx->txbuf   = xStreamBufferCreate(txbuf_size, 1);
	pctx->rxbuf   = xStreamBufferCreate(rxbuf_size, 1);
	pctx->w_mutex = xSemaphoreCreateMutex();

	nh_tty_dnld_register(netbus_tty_dnld, pctx);
	return 0;
}

int netbus_tty_send(netbus_tty_ctx_t *pctx, const uint8_t *p_data, uint32_t len, uint32_t timeout)
{
    if (xSemaphoreTake(pctx->w_mutex, timeout) != pdTRUE) {
		return -1;
	}
    nh_tty_upld_send(p_data, len);
    xSemaphoreGive(pctx->w_mutex);

#if NETBUS_TTY_DEBUG
    uint8_t *data = (uint8_t *)p_data;
    uint32_t data_size = len;

    printf("<---- upld[%d] ", data_size);
    for (int i = 0; i < data_size; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
#endif

}

int netbus_tty_receive(netbus_tty_ctx_t *pctx, uint8_t *p_buffer, uint32_t buf_len, uint32_t timeout)
{
	return xStreamBufferReceive(pctx->rxbuf, p_buffer, buf_len, timeout);
}

int netbus_tty_deinit(netbus_tty_ctx_t *ctx)
{
	// vStreamBufferDelete(ctx->txbuf);
	vStreamBufferDelete(ctx->rxbuf);
	vSemaphoreDelete(ctx->w_mutex);
	return 0;
}
