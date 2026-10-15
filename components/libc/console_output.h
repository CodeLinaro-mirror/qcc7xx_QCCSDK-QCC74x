#ifndef CONSOLE_OUTPUT_H
#define CONSOLE_OUTPUT_H

#include <stddef.h>
#include <sys/types.h>

struct qcc74x_device_s;

#ifdef __cplusplus
extern "C" {
#endif

extern struct qcc74x_device_s *console;

ssize_t qcc74x_console_write(const void *data, size_t size);
void qcc74x_uart_set_console(struct qcc74x_device_s *dev);
void qcc74x_wo_set_console(struct qcc74x_device_s *dev);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_OUTPUT_H */
