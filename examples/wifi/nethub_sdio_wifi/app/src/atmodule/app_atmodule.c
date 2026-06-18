#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <stream_buffer.h>
#include <shell.h>

#include "at_main.h"
#include "netbus_tty.h"

extern netbus_tty_ctx_t at_tty;

#define FAKEBUF_SIZE 1024

static void atsdio_fakepush_forpop_cmd(int argc, char **argv)
{
    uint8_t *buf = NULL;
    int cmd_len;
    int compare_len;
    int hascrlf;
    size_t pushed;

    if (argc < 2 || argv == NULL) {
        printf("Usage: atfake <data_str> [hascrlf]\r\n");
        return;
    }
    if (argc == 3) {
        hascrlf = atoi(argv[2]);
    } else {
        hascrlf = 1;
    }

    if (!at) {
        printf("[ATMODULE] Error: at_struct is NULL\r\n");
        return;
    }
    if (!at_tty.rxbuf) {
        printf("[ATMODULE] Error: at_tty rxbuf is not ready\r\n");
        return;
    }

    cmd_len = strlen(argv[1]);
    compare_len = hascrlf ? (FAKEBUF_SIZE - 2) : FAKEBUF_SIZE;
    if (cmd_len > compare_len) {
        printf("data too long %d > %d\r\n", cmd_len, compare_len);
        return;
    }

    buf = pvPortMalloc(FAKEBUF_SIZE);
    if (!buf) {
        printf("mem err len = %d\r\n", FAKEBUF_SIZE);
        return;
    }

    memcpy(buf, argv[1], cmd_len);
    if (hascrlf) {
        memcpy(buf + cmd_len, "\r\n", 2);
        cmd_len += 2;
    }

    at->fakeoutput = 1;
    pushed = xStreamBufferSend(at_tty.rxbuf, buf, cmd_len, 0);
    printf("fakewrite len:%d, hascrlf:%d\r\n", (int)pushed, hascrlf);

    vPortFree(buf);
}
SHELL_CMD_EXPORT_ALIAS(atsdio_fakepush_forpop_cmd, atfake, sdio fake push for pop);

void app_atmodule_init(void)
{
    at_module_init();
}
