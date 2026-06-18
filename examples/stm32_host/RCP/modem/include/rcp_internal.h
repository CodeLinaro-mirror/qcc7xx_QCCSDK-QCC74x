/**
 * This file is based on ST67W6X_Network_Driver provided by STMicroelectronics.
 * See
 * https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Middlewares/ST/ST67W6X_Network_Driver
 * for more information.
 *
 * Reference source:
 * https://github.com/STMicroelectronics/x-cube-st67w61/blob/main/Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_internal.h
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef RCP_INTERNAL_H
#define RCP_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "rcp_default_config.h"
#include <string.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/** @addtogroup RCP_Common_Macros
 * @{
 */

/**
 * \def RCP_ASSERT(p)
 * Macro to check if the expression is true and call the assert function if it
 * is false
 */

/**
 * \def RCP_NULL_ASSERT(p)
 * Macro to check if the pointer is NULL and return the error code
 */

/**
 * \def RCP_NULL_ASSERT_VOID(p)
 * Macro to check if the pointer is NULL does not return the error code
 */

/**
 * \def RCP_NULL_ASSERT_STR(p, s)
 * Macro to check if the pointer is NULL and return the error code with error
 * string log
 */

#if (RCP_ASSERT_ENABLE == 1)
/** Macro to check if the pointer is NULL and return the error code with error
 * string log */
#define RCP_ASSERT(expr)                                                       \
  if (!(expr)) {                                                               \
    LOG_ERR("Assert failed: file: %s, line: %d", __FILE__, __LINE__);          \
    while (1)                                                                  \
      ;                                                                        \
  }
#define RCP_NULL_ASSERT(p)                                                     \
  if ((p) == NULL) {                                                           \
    return RCP_STATUS_ERROR;                                                   \
  }
#define RCP_NULL_ASSERT_VOID(p)                                                \
  if ((p) == NULL) {                                                           \
    return;                                                                    \
  }
#define RCP_NULL_ASSERT_STR(p, s)                                              \
  if ((p) == NULL) {                                                           \
    LOG_ERR("%s\n", (s));                                                      \
    return RCP_STATUS_ERROR;                                                   \
  }
#else
#define RCP_ASSERT(expr)
#define RCP_NULL_ASSERT(p)
#define RCP_NULL_ASSERT_VOID(p)
#define RCP_NULL_ASSERT_STR(p, s)
#endif /* RCP_ASSERT_ENABLE */

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*RCP_INTERNAL_H */
