#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include "rcp_user_config.h"

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#if LOG_LEVEL >= 4
#define LOG_DBG(msg, ...)                                                                      \
	printf(__FILE_NAME__ ":%s: " msg "\n"                                                      \
			     "\x1b[0m",                                                            \
	       __func__, ##__VA_ARGS__)

#define LOG_HEXDUMP_DBG(data, len, msg) \
	do { \
	  printf(__FILE_NAME__ ": " msg "\n"); \
      for (int _i = 0; _i < len; _i++) { \
        printf("%02x ", data[_i]); \
      } \
	  printf("\n\n\x1b[0m"); \
	} while(0)

#else
#define LOG_DBG(...)
#define LOG_HEXDUMP_DBG(data, len, msg)
#endif /* LOG_LEVEL >= 4 */

#if LOG_LEVEL >= 3
#define LOG_INF(msg, ...)                                                                      \
	printf(__FILE_NAME__ ": " msg "\n",                                                         \
	       ##__VA_ARGS__)
#else
#define LOG_INF(...)
#endif /* LOG_LEVEL >= 3 */

#if LOG_LEVEL >= 2
#define LOG_WRN(msg, ...) printf("\x1b[33m" __FILE_NAME__ ": " msg "\x1b[0m\n", ##__VA_ARGS__)
#else
#define LOG_WRN(...)
#endif /* LOG_LEVEL >= 2 */

#if LOG_LEVEL >= 1
#define LOG_ERR(msg, ...) printf("\x1b[31m" __FILE_NAME__ ": " msg "\x1b[0m\n", ##__VA_ARGS__)
#else
#define LOG_ERR(...)
#endif /* LOG_LEVEL >= 1 */

#endif
