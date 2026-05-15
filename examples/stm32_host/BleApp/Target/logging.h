/**
  ******************************************************************************
  * @file    logging.h
  * @brief   Simplified logging interface for BLE Stack - printf based
  ******************************************************************************
  * @attention
  * This is a simplified implementation using printf instead of the full
  * FreeRTOS logging system. Suitable for development and debugging.
  ******************************************************************************
  */

#ifndef LOGGING_H
#define LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include "logging_config.h"
#include "logging_levels.h"

/* Macro to remove extra parentheses */
#define REMOVE
#define STR_( ... )             REM ## __VA_ARGS__
#define STR( ... )              STR_( __VA_ARGS__ )
#define OVE( ... )              OVE __VA_ARGS__
#define REMOVE_PARENS( ... )    STR( OVE __VA_ARGS__ )

#if defined(__ICCARM__) || defined(__ICCRX__)
#ifndef __FILE_NAME__
#define __FILE_NAME__    __FILE__
#endif
#endif

/* Logging macros based on LOG_LEVEL */
#if (LOG_LEVEL >= LOG_ERROR)
#define LogError( ... )     vLoggingPrintf( LOG_ERROR, __LINE__, __FILE_NAME__, REMOVE_PARENS( __VA_ARGS__ ) )
#else
#define LogError( ... )
#endif

#if (LOG_LEVEL >= LOG_WARN)
#define LogWarn( ... )      vLoggingPrintf( LOG_WARN, __LINE__, __FILE_NAME__, REMOVE_PARENS( __VA_ARGS__ ) )
#else
#define LogWarn( ... )
#endif

#if (LOG_LEVEL >= LOG_INFO)
#define LogInfo( ... )      vLoggingPrintf( LOG_INFO, 0, NULL, REMOVE_PARENS( __VA_ARGS__ ) )
#else
#define LogInfo( ... )
#endif

#if (LOG_LEVEL >= LOG_DEBUG)
#define LogDebug( ... )     vLoggingPrintf( LOG_DEBUG, 0, NULL, REMOVE_PARENS( __VA_ARGS__ ) )
#else
#define LogDebug( ... )
#endif

/**
  * @brief  Printf-like logging function (simplified)
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
                       ...);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_H */
