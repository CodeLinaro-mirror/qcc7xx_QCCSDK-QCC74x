/**
 * This file is based on ST67W6X_Network_Driver provided by STMicroelectronics.
 * See
 * https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Middlewares/ST/ST67W6X_Network_Driver
 * for more information.
 *
 * Reference source:
 * https://github.com/STMicroelectronics/x-cube-st67w61/blob/main/Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_common.h
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef RCP_COMMON_H
#define RCP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/

#include "rcp_api.h"

/** @defgroup RCP_Common RCP Driver Common
 * @ingroup  RCP
 */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/** @defgroup RCP_Common_Constants RCP Driver Common Constants
 * @ingroup  RCP_Common
 * @{
 */

#ifndef RCP_CMDRSP_STRING_SIZE
/** RCP command response string size */
#define RCP_CMDRSP_STRING_SIZE 192
#endif /* RCP_CMDRSP_STRING_SIZE */

#ifndef RCP_MDM_RX_TASK_STACK_SIZE_BYTES
/** Stack required especially for Log messages */
#define RCP_MDM_RX_TASK_STACK_SIZE_BYTES 2048
#endif /* RCP_MDM_RX_TASK_STACK_SIZE_BYTES */

#ifndef RCP_MDM_RX_TASK_PRIO
/** RCP RCP_Modem_Process task priority, recommended to be higher than
 * application tasks */
#define RCP_MDM_RX_TASK_PRIO 54
#endif /* RCP_MDM_RX_TASK_PRIO */

#ifndef RCP_MAX_LOG_LENGTH
/** Maximum size of log */
#define RCP_MAX_LOG_LENGTH 300
#endif /* RCP_MAX_LOG_LENGTH */

/** Cmd/query default timeouts */
#ifndef RCP_NCP_TIMEOUT
/** Timeout for reply/execute of NCP */
#define RCP_NCP_TIMEOUT 2000
#endif /* RCP_NCP_TIMEOUT */

#ifndef RCP_SYS_TIMEOUT
/** Timeout for special cases like flash write, firmware updates, etc */
#define RCP_SYS_TIMEOUT 2000
#endif /* RCP_SYS_TIMEOUT */

#ifndef RCP_WIFI_TIMEOUT
/** Timeout for remote WIFI device operation (e.g. AP) */
#define RCP_WIFI_TIMEOUT 6000
#endif /* RCP_WIFI_TIMEOUT */

#ifndef RCP_NET_TIMEOUT
/** Timeout for remote network operation (e.g. server) */
#define RCP_NET_TIMEOUT 6000
#endif /* RCP_NET_TIMEOUT */

#ifndef RCP_BLE_TIMEOUT
/** Timeout for remote BLE device operation */
#define RCP_BLE_TIMEOUT 2000
#endif /* RCP_BLE_TIMEOUT */

/** @} */

/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/** @defgroup RCP_Common_Macros RCP Driver Common Macros
 * @ingroup  RCP_Common
 * @{
 */

#ifndef CONTAINER_OF
/** Return the pointer to the structure containing the specified member */
#define CONTAINER_OF(ptr, type, field)                                         \
  ({ ((type *)(((char *)(ptr)) - offsetof(type, field))); })
#endif

/**
 * \def LOG_HOST_OUT(pBuf, len)
 * log trace to announce an command sent to the module
 */

/**
 * \def LOG_HOST_IN(pBuf, len)
 * log trace to announce an message received from the module
 */

#if (RCP_LOG_ENABLE == 1)
#define LOG_HOST_OUT(pBuf, len) RCP_Logger(pBuf, len, ">")
#define LOG_HOST_IN(pBuf, len) RCP_Logger(pBuf, len, "<")
#else
#define LOG_HOST_OUT(pBuf, len)
#define LOG_HOST_IN(pBuf, len)
#endif /* RCP_LOG_ENABLE */

/** @} */

/* Exported functions ------------------------------------------------------- */
/** @defgroup RCP_Common_Functions RCP Driver Common Functions
 * @ingroup  RCP_Common
 * @{
 */

/**
 * @brief  Initialize the RCP modem
 * @param  Obj: pointer to module handle
 * @return Operation status
 */
int32_t RCP_ModemInit(RCP_Object_t *Obj);

/**
 * @brief  Deinitialize the RCP modem
 * @param  Obj: pointer to module handle
 */
void RCP_ModemDeInit(RCP_Object_t *Obj);

/**
 * @brief  Translate modem_cmd status into RCP_Status_t
 * @param  ret status
 * @return Operation status RCP_Status_t
 */
RCP_Status_t RCP_Status(int32_t ret);

/**
 * @brief  Send the command for Set and Execute mode, and check the status
 * response
 * @param  Obj: pointer to module handle
 * @param  p_cmd: pointer to pass command string
 * @param  timeout_ms: timeout for the RCP_SetExecute
 * @return Operation status
 */
RCP_Status_t RCP_Common_SetExecute(RCP_Object_t *Obj, uint8_t *p_cmd,
                                   uint32_t timeout_ms);

/**
 * @brief  Send the command for Queries, wait Query response and check the
 * status response
 * @param  Obj: pointer to module handle
 * @param  p_cmd: pointer to pass command string. Reused as rx buffer to store
 * the argv content
 * @param  p_resp: pointer to pass expected response string. \
 *                 Must be provided as buffer allocated locally to ensure
 * re-entrance
 * @param  argc: pointer to number of found argument
 * @param  argv: pointer to argument table
 * @param  timeout_ms: timeout for the first RCP_Common_Query_Parse
 * @return Operation status
 */
RCP_Status_t RCP_Common_Query_Parse(RCP_Object_t *Obj, char *p_cmd,
                                    char *p_resp, uint16_t *argc, char **argv,
                                    uint32_t timeout_ms);

/**
 * @brief  Send the command for Set and Execute mode and send data
 * @param  Obj: pointer to module handle
 * @param  p_cmd: pointer to pass command string
 * @param  pdata: pointer to data to send
 * @param  len: binary data length
 * @param  timeout_ms: timeout
 * @param  check_resp: if true, check the response status
 * @return Operation status
 */

RCP_Status_t RCP_Common_RequestSendData(RCP_Object_t *Obj, uint8_t *p_cmd,
                                        uint8_t *pdata, uint32_t len,
                                        uint32_t timeout_ms, bool check_resp);

/**
 * @brief Remove the double quotes at the beginning and the end of the string
 * @param inbuf: input string
 */
void RCP_RemoveStrQuotes(char *inbuf);

/**
 * @brief  Log command
 * @param  pBuf: pointer to buffer
 * @param  len: buffer length
 * @param  inOut: pointer to string
 */
void RCP_Logger(uint8_t *pBuf, uint32_t len, char *inOut);

/**
 * @brief  Enable or disable RCP_Logger output
 * @param  enable: true to enable log output, false to disable it
 */
void RCP_Logger_SetEnable(bool enable);

/**
 * @brief  Get RCP_Logger output state
 * @return true if log output is enabled, otherwise false
 */
bool RCP_Logger_IsEnabled(void);

#if defined(__ICCARM__) || defined(__ICCRX__) ||                               \
    defined(__ARMCC_VERSION) /* For IAR/MDK Compiler */
/**
 * @brief  Function locates the first occurrence of the null-terminated string
 * little in the string big
 */
char *strnstr(const char *big, const char *little, size_t len);
#endif /* __ICCARM__ || __ARMCC_VERSION */

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RCP_COMMON_H */
