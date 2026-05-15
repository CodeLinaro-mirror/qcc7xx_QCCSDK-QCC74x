/**
  ******************************************************************************
  * @file    logging_levels.h
  * @brief   Simplified logging levels for BLE Stack
  ******************************************************************************
  */

#ifndef LOGGING_LEVELS_H
#define LOGGING_LEVELS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  No log messages
  */
#define LOG_NONE     0

/**
  * @brief  Represents erroneous application state or event
  */
#define LOG_ERROR    1

/**
  * @brief  Message about an abnormal event
  */
#define LOG_WARN     2

/**
  * @brief  A helpful, informational message
  */
#define LOG_INFO     3

/**
  * @brief  Detailed and excessive debug information
  */
#define LOG_DEBUG    4

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_LEVELS_H */
