/**
 * This file is based on ST67W6X_Network_Driver provided by STMicroelectronics.
 * See
 * https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Middlewares/ST/ST67W6X_Network_Driver
 * for more information.
 *
 * Reference source:
 * https://github.com/STMicroelectronics/x-cube-st67w61/blob/main/Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_common.c
 */

/* Includes ------------------------------------------------------------------*/
#include "rcp_common.h"
#include "buffer_mgmt_ctrl.h"
#include "modem_cmd_handler.h"
#include "rcp_bus.h"
#include "rcp_interface.h"
#include "rcp_internal.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @defgroup RCP_Common_Defines RCP Driver Common Defines
 * @ingroup RCP_Common
 * @{
 */

/** Timeout for io send operation */
#define IO_SEND_TIMEOUT 2000

/** Timeout for io receive operation */
#define IO_RECEIVE_TIMEOUT RCP_OS_WAIT_FOREVER

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/** @addtogroup RCP_Common_Functions
 * @{
 */

/**
 * @brief  Initialize the IO interface
 * @param iface: pointer to the modem interface
 * @return 0 on success, negative value on error
 */
static int32_t io_init(struct modem_iface *iface);

/**
 * @brief  Deinitialize the IO interface
 * @param iface: pointer to the modem interface
 * @return 0 on success, negative value on error
 */
static int32_t io_deinit(struct modem_iface *iface);

/**
 * @brief  Modem process task
 * @param arg: pointer to the task argument
 */
static void RCP_Modem_Process_task(void *arg);

/**
 * @brief  Write data to the modem interface
 * @param iface: pointer to the modem interface
 * @param buf: pointer to the data buffer
 * @param size: size of the data to write
 * @return 0 on success, negative value on error
 */
static int32_t modem_iface_spi_write(struct modem_iface *iface,
                                     const uint8_t *buf, size_t size);

/**
 * @brief  Read data from the modem interface
 * @param iface: pointer to the modem interface
 * @param buf: pointer to the buffer to store read data
 * @param size: size of the buffer
 * @param bytes_read: pointer to store the number of bytes read
 * @return 0 on success, negative value on error
 */
static int32_t modem_iface_spi_read(struct modem_iface *iface, uint8_t *buf,
                                    size_t size, size_t *bytes_read);

/**
 * @brief  Callback function to handle query events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of argument
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_query);

/**
 * @brief  Callback function to handle "OK" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_ok);

/**
 * @brief  Callback function to handle "ERROR" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_error);

/**
 * @brief  Callback function to handle "ready" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_ready);

/**
 * @brief  Callback function to handle "+CW" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of arguments
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_wifi_event);

/**
 * @brief  Callback function to handle "+BLE" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of arguments
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_ble_event);

/**
 * @brief  Callback function to handle "+CIP" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_net_event);

/**
 * @brief  Callback function to handle "+MQTT" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of arguments
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_mqtt_event);

/**
 * @brief  Callback function to handle ">" events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_tx_ready);

/**
 * @brief  Callback function to handle "RECV " events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_recv);

/**
 * @brief  Callback function to handle MQTT data events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_mqtt_data_event);

/**
 * @brief  Callback function to handle Net data events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of arguments
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_net_data_event);

/**
 * @brief  Callback function to handle BLE Write data events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_ble_write_data_event);

/**
 * @brief  Callback function to handle BLE Read data events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_ble_read_data_event);

/**
 * @brief  Callback function to handle BLE Notification data events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_ble_noti_data_event);

/**
 * @brief  Callback function to handle ping events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_net_ping_event);

/**
 * @brief  Callback function to handle Wi-Fi scan events
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of arguments (unused)
 * @return process length on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_wifi_scan_event);

/** @} */

/* Private variables ---------------------------------------------------------*/
/** @defgroup RCP_Common_Variables RCP Driver Common Variables
 * @ingroup RCP_Common
 * @{
 */

/** List of response commands for the modem */
static const struct modem_cmd response_cmds[] = {
    MODEM_CMD("OK", on_cmd_ok, 0U, ""),
    MODEM_CMD("ERROR", on_cmd_error, 0U, ""),
};

/** List of unsolicited commands for the modem */
static const struct modem_cmd unsol_cmds[] = {
    MODEM_CMD("ready", on_cmd_ready, 0U, ""),
    MODEM_CMD("+CWLAP:", on_cmd_wifi_scan_event, 8U, ","),
    MODEM_CMD_DIRECT("+BLE:GATTWRITE:", on_cmd_ble_write_data_event),
    MODEM_CMD_DIRECT("+BLE:GATTREAD:", on_cmd_ble_read_data_event),
    MODEM_CMD_DIRECT("+BLE:NOTIDATA:", on_cmd_ble_noti_data_event),
    MODEM_CMD_DIRECT("+MQTT:SUBRECV:", on_cmd_mqtt_data_event),
    MODEM_CMD_ARGS_MAX("+IPD:", on_cmd_net_data_event, 1U, 10U, ","),
    MODEM_CMD_ARGS_MAX("+CW:", on_cmd_wifi_event, 1U, 10U, ", "),
    MODEM_CMD_ARGS_MAX("+BLE:", on_cmd_ble_event, 1U, 11U, ",:()"),
    MODEM_CMD_ARGS_MAX("+CIP:", on_cmd_net_event, 1U, 10U, ","),
    MODEM_CMD_ARGS_MAX("+MQTT:", on_cmd_mqtt_event, 1U, 10U, ","),
    MODEM_CMD("+PING:", on_cmd_net_ping_event, 1U, ""),
};

/** @} */

/* Functions Definition ------------------------------------------------------*/
/** @addtogroup RCP_Common_Functions
 * @{
 */

int32_t RCP_ModemInit(RCP_Object_t *Obj) {
  int32_t ret = -1;
  struct modem *mdm = (struct modem *)&Obj->Modem;

  /* Cmd handler */
  const struct modem_cmd_handler_config cmd_handler_config = {
      .match_buf = (char *)&mdm->cmd_match_buf[0],
      .match_buf_len = sizeof(mdm->cmd_match_buf),
      .eol = "", /* CRLF sent within the command string to avoid 2 successive
                    write */
      .user_data = mdm,
      .response_cmds = response_cmds,
      .response_cmds_len = ARRAY_SIZE(response_cmds),
      .unsol_cmds = unsol_cmds,
      .unsol_cmds_len = ARRAY_SIZE(unsol_cmds),
  };

  memset(&mdm->sem_response, 0, sizeof(mdm->sem_response));
  memset(&mdm->sem_if_ready, 0, sizeof(mdm->sem_if_ready));
  memset(&mdm->sem_tx_ready, 0, sizeof(mdm->sem_tx_ready));
  memset(&mdm->modem_task_handle, 0, sizeof(mdm->modem_task_handle));
  memset(&mdm->modem_cmd_handler_data.sem_tx_lock, 0,
         sizeof(mdm->modem_cmd_handler_data.sem_tx_lock));
  memset(&mdm->modem_cmd_handler_data.sem_parse_lock, 0,
         sizeof(mdm->modem_cmd_handler_data.sem_parse_lock));

  if (!rcp_os_sem_init(&mdm->sem_response, 0, 1)) {
    goto __err;
  }
  if (!rcp_os_sem_init(&mdm->sem_if_ready, 0, 1)) {
    goto __err;
  }
  if (!rcp_os_sem_init(&mdm->sem_tx_ready, 0, 1)) {
    goto __err;
  }

  /* Assign rx_buff */
  /** Linear RX buffer to process */
  mdm->modem_cmd_handler_data.rx_buf = rcp_os_malloc(RX_BUF_SIZE);
  if (mdm->modem_cmd_handler_data.rx_buf == NULL) {
    goto __err;
  }

  ret =
      modem_cmd_handler_init(&mdm->modem_cmd_handler,
                             &mdm->modem_cmd_handler_data, &cmd_handler_config);
  if (ret < 0) {
    goto __err;
  }

  ret = io_init(&mdm->iface);
  if (ret < 0) {
    goto __err;
  }

  if (!rcp_os_thread_create(
          &mdm->modem_task_handle, RCP_Modem_Process_task, mdm, "Modem_Process",
          RCP_MDM_RX_TASK_STACK_SIZE_BYTES, RCP_MDM_RX_TASK_PRIO)) {
    SYS_LOG_ERROR("xTaskCreate failed to create\n");
    ret = -1;
    goto __err;
  }
  /* Wait for the module to be ready */
  /*
    if (RCP_WaitForReady(Obj, 500) != RCP_STATUS_OK)
    {
      printf("sem_if_ready not received\n");
      ret = -1;
      goto __err;
    }
  */
  RCP_Common_SetExecute(Obj, (uint8_t *)"AT\r\n", RCP_NCP_TIMEOUT);

  return RCP_Status(ret);
__err:

  if (mdm->modem_cmd_handler_data.rx_buf != NULL) {
    rcp_os_free(mdm->modem_cmd_handler_data.rx_buf);
    mdm->modem_cmd_handler_data.rx_buf = NULL;
  }
  if (mdm->sem_response._sem != NULL) {
    rcp_os_sem_deinit(&mdm->sem_response);
  }
  if (mdm->sem_if_ready._sem != NULL) {
    rcp_os_sem_deinit(&mdm->sem_if_ready);
  }
  if (mdm->sem_tx_ready._sem != NULL) {
    rcp_os_sem_deinit(&mdm->sem_tx_ready);
  }
  if (mdm->modem_task_handle._thread != NULL) {
    rcp_os_thread_delete(&mdm->modem_task_handle);
  }
  if (mdm->modem_cmd_handler_data.sem_parse_lock._mutex != NULL) {
    rcp_os_mutex_deinit(&mdm->modem_cmd_handler_data.sem_parse_lock);
  }
  if (mdm->modem_cmd_handler_data.sem_tx_lock._mutex != NULL) {
    rcp_os_mutex_deinit(&mdm->modem_cmd_handler_data.sem_tx_lock);
  }
  return RCP_Status(ret);
}

void RCP_ModemDeInit(RCP_Object_t *Obj) {
  struct modem *mdm = (struct modem *)&Obj->Modem;
  io_deinit(&mdm->iface);
  if (mdm->modem_cmd_handler_data.rx_buf != NULL) {
    rcp_os_free(mdm->modem_cmd_handler_data.rx_buf);
    mdm->modem_cmd_handler_data.rx_buf = NULL;
  }
  if (mdm->sem_response._sem != NULL) {
    rcp_os_sem_deinit(&mdm->sem_response);
  }
  if (mdm->sem_if_ready._sem != NULL) {
    rcp_os_sem_deinit(&mdm->sem_if_ready);
  }
  if (mdm->sem_tx_ready._sem != NULL) {
    rcp_os_sem_deinit(&mdm->sem_tx_ready);
  }
  if (mdm->modem_task_handle._thread != NULL) {
    rcp_os_thread_delete(&mdm->modem_task_handle);
  }
  if (mdm->modem_cmd_handler_data.sem_parse_lock._mutex != NULL) {
    rcp_os_mutex_deinit(&mdm->modem_cmd_handler_data.sem_parse_lock);
  }
  if (mdm->modem_cmd_handler_data.sem_tx_lock._mutex != NULL) {
    rcp_os_mutex_deinit(&mdm->modem_cmd_handler_data.sem_tx_lock);
  }
}

RCP_Status_t RCP_Status(int32_t ret) {
  RCP_Status_t status;

  switch (ret) {
  case 0:
    status = RCP_STATUS_OK;
    break;
  case -ETIMEDOUT:
    status = RCP_STATUS_TIMEOUT;
    break;
  case -EIO:
    status = RCP_STATUS_IO_ERROR;
    break;
  default:
    status = RCP_STATUS_ERROR;
    break;
  }
  return status;
}

RCP_Status_t RCP_Common_SetExecute(RCP_Object_t *Obj, uint8_t *p_cmd,
                                   uint32_t timeout_ms) {
  struct modem *mdm = &Obj->Modem;
  return RCP_Status(modem_cmd_send(&mdm->iface, &mdm->modem_cmd_handler, NULL,
                                   0, p_cmd, &mdm->sem_response, timeout_ms));
}

RCP_Status_t RCP_Common_Query_Parse(RCP_Object_t *Obj, char *p_cmd,
                                    char *p_resp, uint16_t *argc, char **argv,
                                    uint32_t timeout_ms) {
  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;
  RCP_Status_t ret;

  if (data == NULL) {
    return RCP_STATUS_ERROR;
  }
  (void)rcp_os_mutex_lock(&data->sem_tx_lock, RCP_OS_WAIT_FOREVER);
  /* **argv reference p_cmd to ensure re-entrance */
  mdm->rx_data = p_cmd;
  mdm->argc = argc;
  mdm->argv = argv;

  struct modem_cmd handlers[] = {{
      .cmd = p_resp,
      .cmd_len = (uint16_t)strlen(p_resp),
      .func = on_cmd_query,
      .arg_count_min = 1,
      .arg_count_max = CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT,
      .delim = ",:",
      .direct = false,
  }};

  ret = RCP_Status(
      modem_cmd_send_ext(&mdm->iface, &mdm->modem_cmd_handler, handlers,
                         ARRAY_SIZE(handlers), (const uint8_t *)p_cmd,
                         &mdm->sem_response, timeout_ms, MODEM_NO_TX_LOCK));
  (void)rcp_os_mutex_unlock(&data->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_Common_RequestSendOtaData(RCP_Object_t *Obj, uint8_t *p_cmd,
                                           uint8_t *pdata, uint32_t len,
                                           uint32_t timeout_ms,
                                           bool check_resp) {
  struct modem *mdm = (struct modem *)&Obj->Modem;
  int32_t ret;
  int32_t bytes_consumed_by_the_bus = 0;
  int32_t bytes_to_send;

  static const struct modem_cmd cmds[] = {
      // MODEM_CMD_DIRECT("OK", on_cmd_recv),
      MODEM_CMD_ARGS_MAX("Recv ", on_cmd_recv, 1U, 2U, " "),
      MODEM_CMD("SEND OK", on_cmd_ok, 0U, ""),
      MODEM_CMD("SEND FAIL", on_cmd_error, 0U, ""),
      MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
  };

  (void)rcp_os_mutex_lock(&mdm->modem_cmd_handler_data.sem_tx_lock,
                          RCP_OS_WAIT_FOREVER);
  /*reset mdm->sem_tx_read */
  (void)rcp_os_sem_take(&mdm->sem_tx_ready, 0);

  ret = modem_cmd_send_ext(
      &mdm->iface, &mdm->modem_cmd_handler, cmds, ARRAY_SIZE(cmds), p_cmd,
      &mdm->sem_response,
      check_resp ? timeout_ms
                 : 0, /* If check_resp is false don't wait for OK */
      MODEM_NO_TX_LOCK | MODEM_NO_UNSET_CMDS);
  if (ret < 0) {
    printf("Failed to send command\n");
    goto out;
  }

  /* Reset semaphore that will be released by "Recv " */
  /*reset mdm->sem_response */
  (void)rcp_os_sem_take(&mdm->sem_response, 0);

  /* Set rx_data_len to be checked during "Recv " event */
  mdm->rx_data_len = len;

  /* Wait for "OK" */
  if (!rcp_os_sem_take(&mdm->sem_tx_ready, 5000)) {
    printf("Timeout waiting for tx\n");
    ret = -ETIMEDOUT;
    goto out;
  }

  ret = modem_cmd_send_data_nolock(&mdm->iface, pdata, len);
  if (ret < 0) {
    printf("calling rcp_wifi_send_data error:%d\n", ret);
  }
  /* Wait for "Recv " */
  if (!rcp_os_sem_take(&mdm->sem_response, timeout_ms)) {
    printf("No send response\n");
    ret = -ETIMEDOUT;
    goto out;
  }

  ret = modem_cmd_handler_get_error(&mdm->modem_cmd_handler_data);
  if (ret != 0) {
    printf("Failed to send data\n");
  }

out:
  (void)modem_cmd_handler_update_cmds(&mdm->modem_cmd_handler_data, NULL, 0U,
                                      false);
  (void)rcp_os_mutex_unlock(&mdm->modem_cmd_handler_data.sem_tx_lock);

  return RCP_Status(ret);
}

RCP_Status_t RCP_Common_RequestSendData(RCP_Object_t *Obj, uint8_t *p_cmd,
                                        uint8_t *pdata, uint32_t len,
                                        uint32_t timeout_ms, bool check_resp) {
  struct modem *mdm = (struct modem *)&Obj->Modem;
  int32_t ret;

  static const struct modem_cmd cmds[] = {
      MODEM_CMD_DIRECT("OK", on_cmd_tx_ready),
      MODEM_CMD_ARGS_MAX("Recv ", on_cmd_recv, 1U, 2U, " "),
      MODEM_CMD("SEND OK", on_cmd_ok, 0U, ""),
      MODEM_CMD("SEND FAIL", on_cmd_error, 0U, ""),
  };

  (void)rcp_os_mutex_lock(&mdm->modem_cmd_handler_data.sem_tx_lock,
                          RCP_OS_WAIT_FOREVER);
  /*reset mdm->sem_tx_read */
  (void)rcp_os_sem_take(&mdm->sem_tx_ready, 0);

  ret = modem_cmd_send_ext(
      &mdm->iface, &mdm->modem_cmd_handler, cmds, ARRAY_SIZE(cmds), p_cmd,
      &mdm->sem_response,
      check_resp ? timeout_ms
                 : 0, /* If check_resp is false don't wait for OK */
      MODEM_NO_TX_LOCK | MODEM_NO_UNSET_CMDS);
  if (ret < 0) {
    printf("failed to send command\n");
    goto out;
  }

  /* Reset semaphore that will be released by "Recv " */
  /*reset mdm->sem_response */
  (void)rcp_os_sem_take(&mdm->sem_response, 0);

  /* Set rx_data_len to be checked during "Recv " event */
  mdm->rx_data_len = len;

  /* Wait for "OK" */
  if (!rcp_os_sem_take(&mdm->sem_tx_ready, 5000)) {
    printf("Timeout waiting for tx\n");
    ret = -ETIMEDOUT;
    goto out;
  }

  ret = modem_cmd_send_data_nolock(&mdm->iface, pdata, len);
  if (ret < 0) {
    printf("calling rcp_wifi_send_data error:%d\n", ret);
  }
  /* Wait for "Recv " */
  if (!rcp_os_sem_take(&mdm->sem_response, timeout_ms)) {
    printf("No send response\n");
    ret = -ETIMEDOUT;
    goto out;
  }

  ret = modem_cmd_handler_get_error(&mdm->modem_cmd_handler_data);
  if (ret != 0) {
    printf("Failed to send data\n");
  }

out:
  (void)modem_cmd_handler_update_cmds(&mdm->modem_cmd_handler_data, NULL, 0U,
                                      false);
  (void)rcp_os_mutex_unlock(&mdm->modem_cmd_handler_data.sem_tx_lock);

  return RCP_Status(ret);
}

void RCP_RemoveStrQuotes(char *inbuf) {
  int32_t len = strlen(inbuf);

  if (len < 2) {
    return; /* Nothing to do */
  }
  inbuf[len - 1] = '\0'; /* Ensure the last character is null-terminated */
  /* Ensure the first character is not a double quote */
  memmove(inbuf, inbuf + 1, len);
}

void RCP_Logger(uint8_t *pBuf, uint32_t len, char *inOut) {
  char log_message[RCP_MAX_LOG_LENGTH];
  uint32_t message_len = RCP_MAX_LOG_LENGTH - 1;
  if (len < RCP_MAX_LOG_LENGTH - 1) {
    message_len = len;
  }
  memcpy(log_message, pBuf, message_len);
  log_message[message_len] = 0;
  if (message_len == RCP_MAX_LOG_LENGTH - 1) {
    log_message[message_len - 1] = '.';
    log_message[message_len - 2] = '.';
    log_message[message_len - 3] = '.';
  }
  //SYS_LOG_DEBUG("RCP%s %s\n", inOut, log_message);
}

#if defined(__ICCARM__) || defined(__ICCRX__) ||                               \
    defined(__ARMCC_VERSION) /* For IAR/MDK Compiler */
char *strnstr(const char *big, const char *little, size_t len) {
  size_t i;
  size_t j;

  if (little[0] == '\0') {
    return ((char *)big);
  }
  j = 0;
  while (j < len && big[j]) {
    i = 0;
    while (j < len && little[i] && big[j] && little[i] == big[j]) {
      ++i;
      ++j;
    }
    if (little[i] == '\0') {
      return ((char *)&big[j - i]);
    }
    j = j - i + 1;
  }
  return (0);
}
#endif /* __ICCARM__ || __ARMCC_VERSION */

/* Private Functions Definition ----------------------------------------------*/
static int32_t modem_iface_sdio_write(struct modem_iface *iface,
                                      const uint8_t *buf, size_t size) {
  if (size == 0) {
    return 0;
  }

  extern int rcp_wifi_send_bytes(const uint8_t *data, uint16_t len,
                                 uint32_t timeout_ms);
  return rcp_wifi_send_bytes((const uint8_t *)buf, size, IO_SEND_TIMEOUT);
}

static int32_t modem_iface_sdio_read(struct modem_iface *iface, uint8_t *buf,
                                     size_t size, size_t *bytes_read) {
  buffer_mgmt_elem_t *rx_buffer = NULL;
  link_result_t ret;

  ret = link_receive(RCP_MSG_TYPE_TTY, &rx_buffer, IO_RECEIVE_TIMEOUT);
  if (ret != RCP_LNK_SUCCESS || rx_buffer == NULL) {
    *bytes_read = 0;
    return (ret == RCP_LNK_TIMEOUT) ? -ETIMEDOUT : -EIO;
  }

  const uint8_t *data = buffer_mgmt_data_ptr_const(rx_buffer);
  uint16_t data_len = buffer_mgmt_data_size(rx_buffer);
  uint16_t copy_len = (data_len < size) ? data_len : (uint16_t)size;

  memcpy(buf, data, copy_len);
  *bytes_read = copy_len;

  LOG_HOST_IN(buf, copy_len);
  buffer_mgmt_free_elem(rx_buffer);
  return 0;
}

static int32_t io_init(struct modem_iface *iface) {
  if (iface == NULL) {
    printf("iface is NULL");
    return -EINVAL;
  }

  iface->read = modem_iface_sdio_read;
  iface->write = modem_iface_sdio_write;
  return 0;
}

static int32_t io_deinit(struct modem_iface *iface) {
  if (iface == NULL) {
    return -EINVAL;
  }

  iface->read = NULL;
  iface->write = NULL;
  return 0;
}

static void RCP_Modem_Process_task(void *arg) {
  struct modem *mdm = (struct modem *)arg;

  while (1) {
    modem_cmd_handler_process(&mdm->modem_cmd_handler, &mdm->iface);
  }
}

MODEM_CMD_DEFINE(on_cmd_query) {
  struct modem *mdm = (struct modem *)data->user_data;
  int32_t offset = ((uint8_t *)mdm->rx_data) - mdm->cmd_match_buf;
  /*len of the current mdm->cmd_match_buf + '\0'*/
  int32_t mlen = (argv[argc - 1] - mdm->cmd_match_buf) +
                 strlen((char *)argv[argc - 1]) + 1;

  memcpy(mdm->rx_data, (char *)mdm->cmd_match_buf, mlen);
  /* Record and offset argv to mdm->rx_data */
  for (int32_t i = 0; i < argc; i++) {
    mdm->argv[i] = (char *)(argv[i] + offset);
  }
  *mdm->argc = argc;

  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_tx_ready) {
  struct modem *mdm = (struct modem *)data->user_data;

  /* Some staged write commands wait inside modem_cmd_send_ext() for the first
   * command response and then wait on sem_tx_ready before pushing payload.
   * Release both semaphores so either "OK" or ">" can complete that first
   * phase without depending on command table ordering.
   */
  (void)rcp_os_sem_give(&mdm->sem_response);
  (void)rcp_os_sem_give(&mdm->sem_tx_ready);
  return len;
}

MODEM_CMD_DEFINE(on_cmd_recv) {
  struct modem *mdm = (struct modem *)data->user_data;
  int32_t recv_len = 0;
  if (argc > 0) {
    recv_len = atoi((char *)argv[0]);
    /*check if length received by modem is matching the sent value */
    if (recv_len != mdm->rx_data_len) {
      modem_cmd_handler_set_error(data, -EIO);
    } else {
      modem_cmd_handler_set_error(data, 0);
    }
  } else {
    modem_cmd_handler_set_error(data, -EIO);
  }

  (void)rcp_os_sem_give(&mdm->sem_response);

  return 0;
}

/* Handler: OK */
MODEM_CMD_DEFINE(on_cmd_ok) {
  struct modem *mdm = (struct modem *)data->user_data;

  modem_cmd_handler_set_error(data, 0);

  (void)rcp_os_sem_give(&mdm->sem_response);

  return 0;
}

/* Handler: ERROR */
MODEM_CMD_DEFINE(on_cmd_error) {
  struct modem *mdm = (struct modem *)data->user_data;

  modem_cmd_handler_set_error(data, -EIO);

  (void)rcp_os_sem_give(&mdm->sem_response);

  return 0;
}

MODEM_CMD_DEFINE(on_cmd_ready) {
  struct modem *mdm = (struct modem *)data->user_data;

  (void)rcp_os_sem_give(&mdm->sem_if_ready);

  return 0;
}

MODEM_CMD_DEFINE(on_cmd_wifi_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.WiFi_event_cb != NULL) {
    Obj->Callbacks.WiFi_event_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_wifi_scan_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.WiFi_event_scan_cb != NULL) {
    Obj->Callbacks.WiFi_event_scan_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_ble_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Ble_event_cb != NULL) {
    Obj->Callbacks.Ble_event_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_ble_write_data_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Ble_event_data_cb != NULL) {
    return Obj->Callbacks.Ble_event_data_cb(RCP_BLE_EVT_WRITE_ID, data, len);
  }
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_ble_read_data_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Ble_event_data_cb != NULL) {
    return Obj->Callbacks.Ble_event_data_cb(RCP_BLE_EVT_READ_ID, data, len);
  }
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_ble_noti_data_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Ble_event_data_cb != NULL) {
    return Obj->Callbacks.Ble_event_data_cb(RCP_BLE_EVT_NOTIFICATION_DATA_ID,
                                            data, len);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_net_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Net_event_cb != NULL) {
    Obj->Callbacks.Net_event_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_net_ping_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Net_event_ping_cb != NULL) {
    Obj->Callbacks.Net_event_ping_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_net_data_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.Net_event_data_cb != NULL) {
    Obj->Callbacks.Net_event_data_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_mqtt_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.MQTT_event_cb != NULL) {
    Obj->Callbacks.MQTT_event_cb(Obj, &argc, (char **)argv);
  }
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_mqtt_data_event) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (Obj->Callbacks.MQTT_event_data_cb != NULL) {
    return Obj->Callbacks.MQTT_event_data_cb(0, data, len);
  }
  return 0;
}

/** @} */
