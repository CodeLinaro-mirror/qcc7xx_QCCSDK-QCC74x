#include <stdio.h>
#include <string.h>

#include "stdarg.h"
#include "console_output.h"

#if CONFIG_ATMOUDLE 
#include "FreeRTOS.h"
#endif

int putchar(int c)
{
    return qcc74x_console_write(&c, 1) == 1 ? c : EOF;
}

int puts(const char *c)
{
    int len;

    if (c == NULL) {
        return 0;
    }

    len = strlen(c);

    return (int)qcc74x_console_write(c, (size_t)len);
}

int putstring(const char *c)
{
    int len;

    if (c == NULL) {
        return 0;
    }

    len = strlen(c);

    return (int)qcc74x_console_write(c, (size_t)len);
}

#ifdef CONFIG_VSNPRINTF_NANO
int printf(const char *fmt, ...)
{
    char print_buf[512];
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = vsnprintf(print_buf, sizeof(print_buf), fmt, ap);
    va_end(ap);

    len = (len > sizeof(print_buf)) ? sizeof(print_buf) : len;

#if CONFIG_ATMOUDLE 
    if (!xPortIsInsideInterrupt() && at_output_is_redirect()) {
        at_write_data((uint8_t *)print_buf, len);
    }
#endif 

    (void)qcc74x_console_write(print_buf, (size_t)len);

    return len;
}
#else
extern int console_vsnprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...)
{
    int len;
    va_list ap;

    va_start(ap, fmt);
    
#if CONFIG_ATMOUDLE
    if (!xPortIsInsideInterrupt() && at_output_is_redirect()) {
        at_write_string(fmt, ap);
    }
#endif 

    len = console_vsnprintf(fmt, ap);
    va_end(ap);

    return len;
}
#endif

#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
void qcc74x_hexdump(const void *ptr, uint32_t buflen)
{
    unsigned char *buf = (unsigned char *)ptr;
    int i, j;

    for (i = 0; i < buflen; i += 16) {
        printf("%08X:", i);

        for (j = 0; j < 16; j++)
            if (i + j < buflen) {
                if ((j % 8) == 0) {
                    printf("  ");
                }

                printf("%02X ", buf[i + j]);
            } else
                printf("   ");
        printf(" ");

        for (j = 0; j < 16; j++)
            if (i + j < buflen)
                printf("%c", __is_print(buf[i + j]) ? buf[i + j] : '.');
        printf("\n");
    }
}

void qcc74x_dump(uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    for (i = 0; i < len; i++) {
        if (i % 16 == 0) {
            printf("\r\n");
        }

        printf("%02x ", data[i]);
    }

    printf("\r\n");
}

void qcc74x_regdump(uint32_t addr)
{
    printf("%08lx[31:0]=%08lx\r\n", addr, *(volatile uint32_t *)(addr));
}
