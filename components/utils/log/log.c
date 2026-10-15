#if defined(CONFIG_NEWLIB) && CONFIG_NEWLIB
#include <stdio.h>
#endif

#include "console_output.h"

#ifdef CONFIG_QCC74xLOG
#include "qcc74xlog.h"

#ifndef CONFIG_LOG_POOL_SIZE
#define CONFIG_LOG_POOL_SIZE 1024
#endif

qcc74xlog_t __qcc74xlog_recorder;
void *__qcc74xlog_recorder_pointer = &__qcc74xlog_recorder;
static uint8_t qcc74xlog_pool[CONFIG_LOG_POOL_SIZE];
qcc74xlog_direct_stream_t qcc74xlog_direct_stream;

static uint16_t console_output(void *ptr, uint16_t size)
{
    return qcc74x_console_write(ptr, size) < 0 ? 0 : size;
}

#endif

void log_start(void)
{
#if defined(CONFIG_NEWLIB) && CONFIG_NEWLIB
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

#ifdef CONFIG_QCC74xLOG
    void *record = (void *)&__qcc74xlog_recorder;
    void *direct = (void *)&qcc74xlog_direct_stream;

    /*!< create recorder */
    qcc74xlog_create(record, qcc74xlog_pool, CONFIG_LOG_POOL_SIZE, QCC74xLOG_MODE_SYNC);

    /*!< create stream direct */
    qcc74xlog_direct_create(direct, QCC74xLOG_DIRECT_TYPE_STREAM, QCC74xLOG_DIRECT_COLOR_ENABLE, NULL, NULL);
    qcc74xlog_direct_init_stream((void *)direct, console_output);

    /*!< connect direct and recorder */
    qcc74xlog_append(record, direct);

    /*!< resume direct */
    qcc74xlog_direct_resume(direct);

    /*!< resume record */
    qcc74xlog_resume(record);
#endif
}

__attribute__((weak)) void error_handler(void)
{
    volatile unsigned char dummy = 0;
    while (dummy == 0) {
    }
}
