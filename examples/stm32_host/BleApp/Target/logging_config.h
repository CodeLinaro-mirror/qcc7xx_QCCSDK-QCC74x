/**
  ******************************************************************************
  * @file    logging_config.h
  * @brief   Simplified logging configuration for BLE Stack
  ******************************************************************************
  */

#ifndef LOGGING_CONFIG_H
#define LOGGING_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logging_levels.h"

/**
  * @brief Global verbosity level
  * @note Set to LOG_INFO for normal operation, LOG_DEBUG for detailed debugging
  */
#define LOG_LEVEL    LOG_INFO

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_CONFIG_H */
