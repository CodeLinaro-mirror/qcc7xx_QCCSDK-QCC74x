/**
  ******************************************************************************
  * @file    logging.c
  * @brief   Simplified logging implementation using printf
  ******************************************************************************
  * @attention
  * This is a simplified implementation using printf instead of the full
  * FreeRTOS logging system. Suitable for development and debugging.
  ******************************************************************************
  */

#include "logging.h"
#include <stdarg.h>
#include <string.h>

/* Log level strings */
static const char *logLevelStrings[] =
{
  "NONE",
  "ERROR",
  "WARN",
  "INFO",
  "DEBUG"
};

/**
  * @brief  Simplified logging function using printf
  * @param  log_level: Log level (LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG)
  * @param  line_number: Line number (0 to skip)
  * @param  p_file_name: File name (NULL to skip)
  * @param  p_format: Printf format string
  * @param  ...: Variable arguments
  * @return Number of characters written, or negative on error
  */
int32_t vLoggingPrintf(uint32_t log_level,
                       const uint32_t line_number,
                       const char *const p_file_name,
                       const char *const p_format,
                       ...)
{
  int32_t ret = 0;
  va_list args;

  /* Print log level */
  if (log_level <= LOG_DEBUG) {
    printf("[%s] ", logLevelStrings[log_level]);
  }

  /* Print file and line if provided */
  if ((p_file_name != NULL) && (line_number != 0)) {
    /* Extract filename from path */
    const char *p_file = p_file_name;
    const char *lastSlash = strrchr(p_file, '/');
    if (lastSlash != NULL) {
      p_file = lastSlash + 1;
    } else {
      lastSlash = strrchr(p_file, '\\');
      if (lastSlash != NULL) {
        p_file = lastSlash + 1;
      }
    }
    printf("(%s:%lu) ", p_file, (unsigned long)line_number);
  }

  /* Print the actual message */
  va_start(args, p_format);
  ret = vprintf(p_format, args);
  va_end(args);

  /* Add newline */
  printf("\n");

  return ret;
}

/**
  * @brief  Initialize logging (stub - not needed for printf version)
  * @param  LogOutput: Callback function (not used in simplified version)
  * @return NULL (always succeeds without creating resources)
  */
void *vLoggingInit(void (*LogOutput)(const char *message))
{
  /* Not needed for printf-based implementation */
  (void)LogOutput;
  return (void *)1;  /* Return non-NULL to indicate success */
}

/**
  * @brief  Deinitialize logging (stub - not needed for printf version)
  */
void vLoggingDeInit(void)
{
  /* Not needed for printf-based implementation */
}

/**
  * @brief  Set log verbosity level
  * @param  level: New log level (LOG_NONE, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG)
  */
void vLoggingSetVerbosity(uint32_t level)
{
  /* Note: In this simplified implementation, LOG_LEVEL is compile-time only */
  /* To support runtime changes, you would need to make LOG_LEVEL a variable */
  (void)level;
}
