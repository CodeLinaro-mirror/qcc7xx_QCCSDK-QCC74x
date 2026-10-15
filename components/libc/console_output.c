#include "console_output.h"

#ifdef CONFIG_BSP_CONSOLE_USB_CDC
#include "usb_console.h"
#elif defined(CONFIG_CONSOLE_WO)
#include "qcc74x_wo.h"
#else
#include "qcc74x_uart.h"
#endif

struct qcc74x_device_s *console = NULL;
static char console_previous_char;

void qcc74x_uart_set_console(struct qcc74x_device_s *dev)
{
    console = dev;
}

void qcc74x_wo_set_console(struct qcc74x_device_s *dev)
{
    console = dev;
}

static ssize_t console_backend_write(const void *data, size_t size)
{
#ifdef CONFIG_BSP_CONSOLE_USB_CDC
    return usb_console_write(data, size);
#elif defined(CONFIG_CONSOLE_WO)
    qcc74x_wo_uart_put(console, (uint8_t *)data, (uint32_t)size);
#else
    (void)qcc74x_uart_put(console, (uint8_t *)data, (uint32_t)size);
#endif
    return (ssize_t)size;
}

ssize_t qcc74x_console_write(const void *data, size_t size)
{
    const char *bytes = data;
    size_t start = 0U;

    if (size == 0U) {
        return 0;
    }
    if (data == NULL) {
        return -1;
    }

#ifndef CONFIG_BSP_CONSOLE_USB_CDC
    if (console == NULL) {
        return -1;
    }

    return console_backend_write(data, size);
#endif

    for (size_t i = 0U; i < size; i++) {
        if ((bytes[i] == '\n') && (console_previous_char != '\r')) {
            if ((i > start) && (console_backend_write(&bytes[start], i - start) < 0)) {
                return -1;
            }
            if (console_backend_write("\r", 1U) < 0) {
                return -1;
            }
            start = i;
        }
        console_previous_char = bytes[i];
    }

    if ((start < size) && (console_backend_write(&bytes[start], size - start) < 0)) {
        return -1;
    }

    return (ssize_t)size;
}
