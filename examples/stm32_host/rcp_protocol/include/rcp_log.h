#ifndef __RCP_LOG_H__
#define __RCP_LOG_H__

#include "rcp_config.h"
#include "log.h"
#if defined(RCP_OS_ZEPHYR) && defined(RCP_LOG)

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(qualcomm_rcp, RCP_LOG_LEVEL);

#if !RCP_LOG_IGNORE_WARNINGS
#define RCP_LOG_ERR(...) LOG_ERR(__VA_ARGS__)
#define RCP_LOG_WRN(...) LOG_WRN(__VA_ARGS__)
#else
#define RCP_LOG_ERR(...)
#define RCP_LOG_WRN(...)
#endif

#define RCP_LOG_INF(...) LOG_INF(__VA_ARGS__)
#define RCP_LOG_DBG(...) LOG_DBG(__VA_ARGS__)

#elif (defined(RCP_OS_FREERTOS) || defined(RCP_OS_LINUX)) && defined(RCP_LOG)

#include <stdio.h>

#if RCP_LOG_LEVEL >= 4
#define RCP_LOG_DBG(msg, ...) LOG_DBG(msg, ##__VA_ARGS__)
#else
#define RCP_LOG_DBG(...)
#endif /* RCP_LOG_LEVEL >= 4 */

#if RCP_LOG_LEVEL >= 3
#define RCP_LOG_INF(msg, ...) LOG_INF(msg, ##__VA_ARGS__)
#else
#define RCP_LOG_INF(...)
#endif

#if RCP_LOG_LEVEL >= 2 && !RCP_LOG_IGNORE_WARNINGS
#define RCP_LOG_WRN(msg, ...) LOG_WRN(msg, ##__VA_ARGS__)
#else
#define RCP_LOG_WRN(...)
#endif /* RCP_LOG_LEVEL >= 2 */

#if RCP_LOG_LEVEL >= 1 && !RCP_LOG_IGNORE_WARNINGS
#define RCP_LOG_ERR(msg, ...) LOG_ERR(msg, ##__VA_ARGS__)
#else
#define RCP_LOG_ERR(...)
#endif /* RCP_LOG_LEVEL >= 1 */

#else
#define RCP_LOG_DBG(...)
#define RCP_LOG_INF(...)
#define RCP_LOG_WRN(...)
#define RCP_LOG_ERR(...)
#endif

#endif
