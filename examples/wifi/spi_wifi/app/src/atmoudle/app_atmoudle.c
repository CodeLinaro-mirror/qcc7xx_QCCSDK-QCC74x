#include <stdio.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <mem.h>
#include <shell.h>
//#include <spisync.h>
#include "at_main.h"
#include "qcc74x_dma.h"
#include <qcc74x_gpio.h>
#include <utils_crc.h>

#include <nxspi.h>
#if CONFIG_THREAD
#include <openthread_port.h>
#endif

#if NXSPI_HCI || NXUART_HCI
#include "btblecontroller_port_uart.h"
#endif

#if 0
spisync_t    *at_spisync = NULL;
static void __spisync_gpio_init(void *arg);

const spisync_hw_t spisync_hw = {
    .spi_name = "spi0",
    .spi_tx_dmach = "dma0_ch0",
    .spi_rx_dmach = "dma0_ch1",
#ifdef CONFIG_SPI_3PIN_MODE_ENABLE
    .spi_3pin_mode = 1,
#else
    .spi_3pin_mode = 0,
#endif 
    .data_rdy_pin = GPIO_PIN_20,
    .spi_port = 0,
    .spi_mode = 0,
    .spi_speed = 80000000,

    .spi_dma_req_tx = DMA_REQUEST_SPI0_TX,
    .spi_dma_req_rx = DMA_REQUEST_SPI0_RX,
    .spi_dma_tdr = DMA_ADDR_SPI0_TDR,
    .spi_dma_rdr = DMA_ADDR_SPI0_RDR,
};

const spisync_config_t spisync_config = {
    .hw = &spisync_hw,
    .ops = {
        //{
        //    .cb     = spieth_input_port,
        //    .cb_pri = at_spisync,
        //    .type   = 0
        //},
        {
            .cb     = NULL,
            .cb_pri = NULL,
            .type   = 0
        },
    },
    .reset_cb = __spisync_gpio_init,
    .reset_arg = NULL,
};
#endif

static void __spisync_gpio_init(void *arg)
{
#if CONFIG_SPI_3PIN_MODE_ENABLE
    board_spi0_gpio_3pin_init();
#else
    board_spi0_gpio_init();
#endif
}

#if 0
#define FAKEBUF_SIZE SPISYNC_PAYLOADBUF_LEN
#else
#define FAKEBUF_SIZE 1024
#endif
static void atspisync_fakepush_forpop_cmd(int argc, char **argv)
{
    uint8_t *buf = NULL;
    uint32_t buf_len;
    int cmd_len;
    int compare_len;
    int hascrlf;
    //spisync_msg_t msg;

    if (argc < 2 || argv == NULL) {
        printf("Usage: at_fake <data_str> [hascrlf]\r\n");
        return;
    }
    if (argc == 3) {
        hascrlf = atoi(argv[2]);
    } else {
        /* default add crlf */
        hascrlf = 1;
    }

    buf = pvPortMalloc(FAKEBUF_SIZE);
    if (NULL == buf) {
        printf("mem err len = %d\r\n", FAKEBUF_SIZE);
        goto fakepush_err;
    }

    cmd_len = strlen(argv[1]);
    compare_len = hascrlf?(FAKEBUF_SIZE - 2):(FAKEBUF_SIZE);
    if (cmd_len > compare_len) {
        printf("data too len %d > %d, Usage: ss_fakepush <data>\r\n",
                        cmd_len, compare_len);
        goto fakepush_err;
    }
    memcpy(buf, argv[1], cmd_len);
    if (hascrlf) {
        memcpy(buf + cmd_len, "\r\n", 2);
        cmd_len += 2;
    }
    buf_len = cmd_len;

    extern struct at_struct *at;
    if (!at) {
        printf("[ATMOULE] Error: at_struct is NULL\r\n");
        goto fakepush_err;
    }
    at->fakeoutput = 1;
#if 0
    SPISYNC_MSGINIT(&msg,
                SPISYNC_TYPESTREAM_AT,              /* type */
                0,                                  /* copy */
                0,                                  /* timeout */
                buf, buf_len,                       /* buf buf_len */
                NULL,                               /* cb */
                NULL);                              /* cb_arg */

    buf_len = spisync_fakewrite_forread(at_spisync, &msg, 0);
#else
    buf_len = nxspi_fakewrite_forread(0, buf, buf_len, 0);
#endif

    printf("fakewrite len:%d, hascrlf:%d\r\n", buf_len, hascrlf);
fakepush_err:
    if (buf) {
        vPortFree(buf);
    }
}
SHELL_CMD_EXPORT_ALIAS(atspisync_fakepush_forpop_cmd, atfake, spisync fake push for pop);

#if NXUART_HCI
static void uart_hci_init(void)
{
    const btble_uart_config_t uart_config = {
        .uart_id = 1,
        .tx_pin = GPIO_PIN_0,
        .rx_pin = GPIO_PIN_1,
        .cts_pin = 0xFF,
        .rts_pin = 0xFF,
        .baudrate = 115200,
        .dma_enable = 0,
        .flow_ctrl_enable = 0,
    };

    printf("config hci uart\r\n");
    if (btble_uart_configure(&uart_config) != 0) {
        printf("config hci uart failed\r\n");
        return;
    }
    btble_uart_init(uart_config.uart_id);
    btble_controller_init(configMAX_PRIORITIES - 1);
}
#endif

void app_atmoudle_init(void)
{
    nxspi_init();
#if NXSPI_NET
    spinet_init();
#endif
    at_module_init();
#if NXSPI_HCI
    spihci_init();
#elif NXUART_HCI
    uart_hci_init();
#endif
#if CONFIG_THREAD
    extern void ot_nxspi_notify_rxd(void);
    nxspi_rxd_callback_register(ot_nxspi_notify_rxd, NXSPI_TYPE_OT);
#endif
}

