/**
 * This file is based on ST67W6X_Network_Driver provided by STMicroelectronics.
 * See
 * https://github.com/STMicroelectronics/x-cube-st67w61/tree/main/Middlewares/ST/ST67W6X_Network_Driver
 * for more information.
 *
 * Reference source:
 * https://github.com/STMicroelectronics/x-cube-st67w61/blob/main/Middlewares/ST/ST67W6X_Network_Driver/Driver/W61_at/w61_at_sys.c
 */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_compiler.h"
#include "common_parser.h"
#include "rcp_api.h"
#include "rcp_common.h"
#include "rcp_internal.h"
#include <inttypes.h>
#include <stdio.h>

#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Private typedef -----------------------------------------------------------*/
/** @addtogroup RCP_System_Types
 * @{
 */

/** @} */

/* Private defines -----------------------------------------------------------*/
/** @addtogroup RCP_System_Constants
 * @{
 */

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @defgroup RCP_System_Variables RCP Driver System Variables
 * @ingroup  RCP_System
 * @{
 */

/** Global RCP context */
RCP_Object_t RCP_Obj = {0};

#if (RCP_ASSERT_ENABLE == 1)
/** RCP context pointer error string */
static const char RCP_Obj_Null_str[] = "Object pointer not initialized";
#endif /* RCP_ASSERT_ENABLE */

static void rcp_trim_spaces(char *str) {
  char *start;
  char *end;

  if (str == NULL) {
    return;
  }

  start = str;
  while ((*start == ' ') || (*start == '\t')) {
    start++;
  }

  if (start != str) {
    memmove(str, start, strlen(start) + 1U);
  }

  end = str + strlen(str);
  while ((end > str) && ((end[-1] == ' ') || (end[-1] == '\t'))) {
    end--;
  }
  *end = '\0';
}

static int8_t rcp_hex_nibble(uint8_t ch) {
  if ((ch >= '0') && (ch <= '9')) {
    return (int8_t)(ch - '0');
  }
  if ((ch >= 'a') && (ch <= 'f')) {
    return (int8_t)(ch - 'a' + 10);
  }
  if ((ch >= 'A') && (ch <= 'F')) {
    return (int8_t)(ch - 'A' + 10);
  }
  return -1;
}

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup RCP_System_Functions
 * @{
 */

/**
 * @brief  Callback function to handle read efuse responses
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of argument (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_read_efuse);

/**
 * @brief  Callback function to handle list files responses
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of argument
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_fs_listfiles);

/**
 * @brief  Callback function to handle read file content responses
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings (unused)
 * @param  argc: number of argument (unused)
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_fs_readfile);

/**
 * @brief  Callback function to handle Flash raw read responses
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_read_flash);

/**
 * @brief  Callback function to handle PART query line responses
 */
MODEM_CMD_DECLARE(on_cmd_part_line);

/**
 * @brief  Callback function to handle streamed minidump hex data
 */
MODEM_CMD_DIRECT_DECLARE(on_cmd_minidump_line);

typedef struct {
  uint8_t *slot0;
  uint8_t *slot1;
  uint8_t *slot2;
} rcp_gmac_ctx_t;

typedef struct {
  RCP_PartInfo_t *parts;
  uint32_t capacity;
  uint32_t count;
} rcp_part_query_ctx_t;

typedef struct {
  uint8_t *dump_data;
  uint32_t capacity;
  uint32_t length;
  bool overflow;
  int8_t pending_nibble;
  bool has_pending_nibble;
} rcp_minidump_ctx_t;

/**
 * @brief  Callback function to handle command responses
 * @param  data: pointer to the modem_cmd_handler_data structure
 * @param  len: length of the data
 * @param  argv: array of argument strings
 * @param  argc: number of argument
 * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_atcmd);

/**
 * @brief  Callback function to handle GMAC SLOT0 response
 */
MODEM_CMD_DECLARE(on_cmd_gmac_slot0);

/**
 * @brief  Callback function to handle GMAC SLOT1 response
 */
MODEM_CMD_DECLARE(on_cmd_gmac_slot1);

/**
 * @brief  Callback function to handle GMAC SLOT2 response
 */
MODEM_CMD_DECLARE(on_cmd_gmac_slot2);

/* Functions Definition ------------------------------------------------------*/
RCP_Object_t *RCP_ObjGet(void) { return &RCP_Obj; }

RCP_Status_t RCP_RegisterULcb(RCP_Object_t *Obj,
                              RCP_UpperLayer_wifi_sta_cb_t UL_wifi_sta_cb,
                              RCP_UpperLayer_wifi_ap_cb_t UL_wifi_ap_cb,
                              RCP_UpperLayer_net_cb_t UL_net_cb,
                              RCP_UpperLayer_mqtt_cb_t UL_mqtt_cb,
                              RCP_UpperLayer_ble_cb_t UL_ble_cb) {
  if (!Obj) {
    return RCP_STATUS_ERROR;
  }

  if (UL_wifi_sta_cb) {
    Obj->ulcbs.UL_wifi_sta_cb = UL_wifi_sta_cb;
  }
  if (UL_wifi_ap_cb) {
    Obj->ulcbs.UL_wifi_ap_cb = UL_wifi_ap_cb;
  }
  if (UL_net_cb) {
    Obj->ulcbs.UL_net_cb = UL_net_cb;
  }
  if (UL_mqtt_cb) {
    Obj->ulcbs.UL_mqtt_cb = UL_mqtt_cb;
  }
  if (UL_ble_cb) {
    Obj->ulcbs.UL_ble_cb = UL_ble_cb;
  }

  return RCP_STATUS_OK;
}

RCP_Status_t RCP_Init(RCP_Object_t *Obj) {
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Initialize the Modem handler
     Start the SPI driver
     Boot the RCP module */
  ret = (RCP_Status_t)RCP_ModemInit(Obj);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Could not init Modem handler\n");
    goto __error;
  }

__error:
  return ret;
}

RCP_Status_t RCP_DeInit(RCP_Object_t *Obj) {
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  /* Deinitialize the Modem handler
     Shutdown the RCP module
     Stop the SPI driver */
  RCP_ModemDeInit(Obj);
  return RCP_STATUS_OK;
}

RCP_Status_t RCP_WaitForReady(RCP_Object_t *Obj, uint32_t time_ms) {
  struct modem *mdm = (struct modem *)&Obj->Modem;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Wait for the module to be ready */
  if (!rcp_os_sem_take(&mdm->sem_if_ready, time_ms)) {
    SYS_LOG_ERROR("sem_if_ready not received\n");
    return RCP_STATUS_ERROR;
  } else {
    /** Need to wait after catching ready event to ensure module is fully
     * operational .*/
    rcp_os_delay_ms(100);
    return RCP_STATUS_OK;
  }
}

RCP_Status_t RCP_ResetToFactoryDefault(RCP_Object_t *Obj) {
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Send the factory reset command */
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)"AT+RESTORE\r\n", 2000);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Restore factory default failed\n");
    goto __err;
  }

  /* Wait for the module to be ready */
  ret = RCP_WaitForReady(Obj, RCP_READY_DEFAULT_TIMEOUT_MS);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("sem_if_ready not received\n");
    goto __err;
  }

  /* Send a command to check the module is available */
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)"AT\r\n", RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("command failed after factory reset\n");
    goto __err;
  }
  rcp_os_delay_ms(100); /* Add mandatory delay */

__err:
  return ret;
}

RCP_Status_t RCP_SendAT(RCP_Object_t *Obj) {
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  ret = RCP_Common_SetExecute(Obj, (uint8_t *)"AT\r\n", RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("command failed after reset\n");
    goto __err;
  }

  /* Add mandatory delay */
  rcp_os_delay_ms(100);

__err:
  return ret;
}

RCP_Status_t RCP_Reset(RCP_Object_t *Obj) {
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  ret = RCP_Common_SetExecute(Obj, (uint8_t *)"AT+RST\r\n", 2000);
  if (ret != RCP_STATUS_OK) {
    printf("Reset command failed\n");
    goto __err;
  }

  /* Wait for the module to be ready */
  ret = RCP_WaitForReady(Obj, RCP_READY_DEFAULT_TIMEOUT_MS);
  if (ret != RCP_STATUS_OK) {
    printf("sem_if_ready not received\n");
    goto __err;
  }

  ret = RCP_Common_SetExecute(Obj, (uint8_t *)"AT\r\n", RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    printf("command failed after reset\n");
    goto __err;
  }

  /* Add mandatory delay */
  rcp_os_delay_ms(100);

__err:
  return ret;
}

RCP_Status_t RCP_GetNcpHeapState(RCP_Object_t *Obj, uint32_t *RemainingHeap,
                                 uint32_t *LwipHeap) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Query the remaining heap and LWIP heap sizes. The response is in the form
     of +SYSRAM:<remaining RAM size>,<lwip heap size> */
  strncpy(cmd, "AT+SYSRAM?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+SYSRAM:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 2) {
    return RCP_STATUS_ERROR;
  }

  *RemainingHeap = (uint32_t)atoi(argv[0]);
  *LwipHeap = (uint32_t)atoi(argv[1]);

  return ret;
}

RCP_Status_t RCP_GetStoreMode(RCP_Object_t *Obj, uint32_t *mode) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Query the parameter store mode. The response is in the form of
     +SYSSTORE:<store_mode> */
  strncpy(cmd, "AT+SYSSTORE?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+SYSSTORE:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 1) {
    return RCP_STATUS_ERROR;
  }

  *mode = (uint32_t)atoi(argv[0]);

  return ret;
}

RCP_Status_t RCP_RestoreGPIO(RCP_Object_t *Obj, uint32_t pin) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  /* Restore the specified GPIO to its default state */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+IORST=%" PRIu32 "\r\n", pin);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_EFuseConfirm(RCP_Object_t *Obj) {
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+EFUSE-CFM\r\n",
                               RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_EFuseWrite(RCP_Object_t *Obj, uint32_t nbytes,
                            const char *addr, uint8_t *data) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(data);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+EFUSE-W=%" PRIu32 ",\"%s\"\r\n",
           nbytes, addr);
  return RCP_Common_RequestSendData(Obj, (uint8_t *)cmd, data, nbytes,
                                    RCP_SYS_TIMEOUT, true);
}

RCP_Status_t RCP_EFuseWriteHex(RCP_Object_t *Obj, uint32_t nbytes,
                               const char *addr, const char *hex_data) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  uint32_t hex_len = nbytes * 2U;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(hex_data);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+EFUSE-W-HEX=%" PRIu32 ",\"%s\"\r\n",
           nbytes, addr);
  return RCP_Common_RequestSendData(Obj, (uint8_t *)cmd, (uint8_t *)hex_data,
                                    hex_len, RCP_SYS_TIMEOUT, true);
}

RCP_Status_t RCP_FlashWrite(RCP_Object_t *Obj, uint32_t nbytes,
                            const char *addr, uint8_t *data) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(data);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+FLASH-W=%" PRIu32 ",\"%s\"\r\n",
           nbytes, addr);
  return RCP_Common_RequestSendData(Obj, (uint8_t *)cmd, data, nbytes,
                                    RCP_SYS_TIMEOUT, true);
}

RCP_Status_t RCP_FlashWriteHex(RCP_Object_t *Obj, uint32_t nbytes,
                               const char *addr, const char *hex_data) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  uint32_t hex_len = nbytes * 2U;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(hex_data);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+FLASH-W-HEX=%" PRIu32 ",\"%s\"\r\n",
           nbytes, addr);
  return RCP_Common_RequestSendData(Obj, (uint8_t *)cmd, (uint8_t *)hex_data,
                                    hex_len, RCP_SYS_TIMEOUT, true);
}

RCP_Status_t RCP_FlashErase(RCP_Object_t *Obj, uint32_t nbytes,
                            const char *addr) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+FLASH-E=%" PRIu32 ",\"%s\"\r\n",
           nbytes, addr);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_GPIOPullUpDn(RCP_Object_t *Obj, uint32_t pin, uint32_t state) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+IOPUPD=%" PRIu32 ",%" PRIu32 "\r\n",
           pin, state);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_GPIOOutput(RCP_Object_t *Obj, uint32_t pin, uint32_t state) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+IOOUT=%" PRIu32 ",%" PRIu32 "\r\n",
           pin, state);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_GPIOInputConfig(RCP_Object_t *Obj, uint32_t pin,
                                 uint32_t pull_state) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+IOIN=%" PRIu32 ",%" PRIu32 "\r\n",
           pin, pull_state);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_SYS_TIMEOUT);
}

RCP_Status_t RCP_GPIOInputRead(RCP_Object_t *Obj, uint32_t pin,
                               uint32_t *state) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(state);

  snprintf(cmd, sizeof(cmd), "AT+IOIN=%" PRIu32 "?\r\n", pin);
  ret =
      RCP_Common_Query_Parse(Obj, cmd, "+IOIN=", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }

  if (argc < 2) {
    return RCP_STATUS_ERROR;
  }

  *state = (uint32_t)strtoul(argv[1], NULL, 10);
  return ret;
}

RCP_Status_t RCP_SetStoreMode(RCP_Object_t *Obj, uint32_t mode) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SYSSTORE=%" PRIu32 "\r\n", mode);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_FlashRead(RCP_Object_t *Obj, uint32_t nbytes, const char *addr,
                           uint8_t *data) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_Status_t ret;
  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data_mdm =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;
  struct modem_cmd handlers[] = {
      MODEM_CMD_DIRECT("+FLASH-R:", on_cmd_read_flash),
  };

  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(data);
  if (data_mdm == NULL) {
    return RCP_STATUS_ERROR;
  }

  (void)rcp_os_mutex_lock(&data_mdm->sem_tx_lock, RCP_OS_WAIT_FOREVER);
  mdm->rx_data = (void *)data;

  snprintf(cmd, sizeof(cmd), "AT+FLASH-R=%" PRIu32 ",\"%s\"\r\n", nbytes, addr);
  ret = RCP_Status(modem_cmd_send_ext(&mdm->iface, &mdm->modem_cmd_handler,
                                      handlers, ARRAY_SIZE(handlers),
                                      (const uint8_t *)cmd, &mdm->sem_response,
                                      RCP_NCP_TIMEOUT, MODEM_NO_TX_LOCK));

  (void)rcp_os_mutex_unlock(&data_mdm->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_FlashReadHex(RCP_Object_t *Obj, uint32_t nbytes,
                              const char *addr, char *hex_data) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(hex_data);

  snprintf(cmd, sizeof(cmd), "AT+FLASH-R-HEX=%" PRIu32 ",\"%s\"\r\n", nbytes,
           addr);
  ret = RCP_Common_Query_Parse(Obj, cmd, "+FLASH-R-HEX:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }

  if (argc < 2) {
    return RCP_STATUS_ERROR;
  }

  strncpy(hex_data, argv[1], 2U * nbytes + 1U);
  hex_data[2U * nbytes] = '\0';
  return ret;
}
RCP_Status_t RCP_EFuseRead(RCP_Object_t *Obj, uint32_t nbytes, const char *addr,
                           uint32_t reload, uint8_t *data) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_Status_t ret;
  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data_mdm =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;
  struct modem_cmd handlers[] = {
      MODEM_CMD_DIRECT("+EFUSE-R:", on_cmd_read_efuse),
  };

  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(data);
  if (data_mdm == NULL) {
    return RCP_STATUS_ERROR;
  }

  (void)rcp_os_mutex_lock(&data_mdm->sem_tx_lock, RCP_OS_WAIT_FOREVER);
  mdm->rx_data = (void *)data;

  snprintf(cmd, sizeof(cmd), "AT+EFUSE-R=%" PRIu32 ",\"%s\",%" PRIu32 "\r\n",
           nbytes, addr, reload);
  ret = RCP_Status(modem_cmd_send_ext(&mdm->iface, &mdm->modem_cmd_handler,
                                      handlers, ARRAY_SIZE(handlers),
                                      (const uint8_t *)cmd, &mdm->sem_response,
                                      RCP_NCP_TIMEOUT, MODEM_NO_TX_LOCK));

  (void)rcp_os_mutex_unlock(&data_mdm->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_EFuseReadHex(RCP_Object_t *Obj, uint32_t nbytes,
                              const char *addr, uint32_t reload,
                              char *hex_data) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(addr);
  RCP_NULL_ASSERT(hex_data);

  snprintf(cmd, sizeof(cmd),
           "AT+EFUSE-R-HEX=%" PRIu32 ",\"%s\",%" PRIu32 "\r\n", nbytes, addr,
           reload);
  ret = RCP_Common_Query_Parse(Obj, cmd, "+EFUSE-R-HEX:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }

  if (argc < 2) {
    return RCP_STATUS_ERROR;
  }

  strncpy(hex_data, argv[1], 2U * nbytes + 1U);
  hex_data[2U * nbytes] = '\0';
  return ret;
}

RCP_Status_t RCP_PartQuery(RCP_Object_t *Obj, RCP_PartInfo_t *parts,
                           uint32_t *nb_parts) {
  RCP_Status_t ret;
  rcp_part_query_ctx_t part_ctx = {0};
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data_mdm =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;

  struct modem_cmd handlers[] = {
      MODEM_CMD("+PART=", on_cmd_part_line, 0U, ""),
  };

  if (data_mdm == NULL) {
    return RCP_STATUS_ERROR;
  }

  if (((parts == NULL) && (nb_parts != NULL)) ||
      ((parts != NULL) && (nb_parts == NULL))) {
    return RCP_STATUS_ERROR;
  }

  if ((parts != NULL) && (nb_parts != NULL)) {
    part_ctx.parts = parts;
    part_ctx.capacity = *nb_parts;
  }
  (void)rcp_os_mutex_lock(&data_mdm->sem_tx_lock, RCP_OS_WAIT_FOREVER);

  mdm->rx_data = &part_ctx;

  ret = RCP_Status(modem_cmd_send_ext(
      &mdm->iface, &mdm->modem_cmd_handler, handlers, ARRAY_SIZE(handlers),
      (const uint8_t *)"AT+PART?\r\n", &mdm->sem_response, RCP_NCP_TIMEOUT,
      MODEM_NO_TX_LOCK));

  mdm->rx_data = NULL;
  if ((parts != NULL) && (nb_parts != NULL)) {
    *nb_parts = part_ctx.count;
  }

  (void)rcp_os_mutex_unlock(&data_mdm->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_Minidump(RCP_Object_t *Obj, uint8_t *dump_data,
                          uint32_t *dump_len) {
  RCP_Status_t ret;
  rcp_minidump_ctx_t dump_ctx = {0};
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data_mdm =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;

  struct modem_cmd handlers[] = {
      MODEM_CMD_DIRECT("0", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("1", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("2", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("3", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("4", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("5", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("6", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("7", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("8", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("9", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("A", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("B", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("C", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("D", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("E", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("F", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("a", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("b", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("c", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("d", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("e", on_cmd_minidump_line),
      MODEM_CMD_DIRECT("f", on_cmd_minidump_line),
  };

  if (data_mdm == NULL) {
    return RCP_STATUS_ERROR;
  }

  if (((dump_data == NULL) && (dump_len != NULL)) ||
      ((dump_data != NULL) && (dump_len == NULL))) {
    return RCP_STATUS_ERROR;
  }

  if ((dump_data != NULL) && (dump_len != NULL)) {
    dump_ctx.dump_data = dump_data;
    dump_ctx.capacity = *dump_len;
    if (dump_ctx.capacity == 0U) {
      return RCP_STATUS_ERROR;
    }
  }

  (void)rcp_os_mutex_lock(&data_mdm->sem_tx_lock, RCP_OS_WAIT_FOREVER);

  mdm->rx_data = &dump_ctx;

  ret = RCP_Status(modem_cmd_send_ext(
      &mdm->iface, &mdm->modem_cmd_handler, handlers, ARRAY_SIZE(handlers),
      (const uint8_t *)"AT+MINIDUMP\r\n", &mdm->sem_response, RCP_SYS_TIMEOUT,
      MODEM_NO_TX_LOCK));

  mdm->rx_data = NULL;

  if ((dump_data != NULL) && (dump_len != NULL)) {
    *dump_len = dump_ctx.length;
    if ((ret == RCP_STATUS_OK) &&
        (dump_ctx.overflow || dump_ctx.has_pending_nibble)) {
      ret = RCP_STATUS_ERROR;
    }
  }

  (void)rcp_os_mutex_unlock(&data_mdm->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_FS_DeleteFile(RCP_Object_t *Obj, char *filename) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(filename);

  /* Delete the specified file in the filesystem. The parameters are:
     - <type>: 0
     - <operation>: 0 (Delete a file)
     - <filename>: The name of the file to delete */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+FS=0,0,\"%s\"\r\n", filename);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_FS_CreateFile(RCP_Object_t *Obj, char *filename) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(filename);

  /* Create a new file in the filesystem. The parameters are:
     - <type>: 0
     - <operation>: 1 (Create a file)
     - <filename>: The name of the file to create */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+FS=0,1,\"%s\"\r\n", filename);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_FS_WriteFile(RCP_Object_t *Obj, char *filename,
                              uint32_t offset, uint8_t *data, uint32_t len) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(filename);
  RCP_NULL_ASSERT(data);

  /* Write the specified file in the filesystem. The parameters are:
    - <type>: 0
    - <operation>: 2 (Write data to a file)
    - <filename>: The name of the file to write
    - <offset>: The offset in the file to start writing
    - <len>: The length of data to write
    The data to write is sent in the next request part */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE,
           "AT+FS=0,2,\"%s\",%" PRIu32 ",%" PRIu32 "\r\n", filename, offset,
           len);
  return RCP_Common_RequestSendData(Obj, (uint8_t *)cmd, data, len,
                                    RCP_SYS_TIMEOUT, true);
}

RCP_Status_t RCP_FS_ReadFile(RCP_Object_t *Obj, char *filename, uint32_t offset,
                             uint8_t *data, uint32_t len) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(filename);
  RCP_NULL_ASSERT(data);

  RCP_Status_t ret;
  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data_mdm =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;

  struct modem_cmd handlers[] = {
      MODEM_CMD_DIRECT("+FS:READ,", on_cmd_fs_readfile),
  };

  if (data_mdm == NULL) {
    return RCP_STATUS_ERROR;
  }
  (void)rcp_os_mutex_lock(&data_mdm->sem_tx_lock, RCP_OS_WAIT_FOREVER);

  mdm->rx_data = (void *)data;

  /* Read the specified file in the filesystem. The parameters are:
    - <type>: 0
    - <operation>: 3 (Read data from a file)
    - <filename>: The name of the file to read
    - <offset>: The offset in the file to start reading
    - <len>: The length of data to read
    The response is in the form of
    +FS:READ,<nbytes>,[data] */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE,
           "AT+FS=0,3,\"%s\",%" PRIu32 ",%" PRIu32 "\r\n", filename, offset,
           len);
  ret = RCP_Status(modem_cmd_send_ext(&mdm->iface, &mdm->modem_cmd_handler,
                                      handlers, ARRAY_SIZE(handlers),
                                      (const uint8_t *)cmd, &mdm->sem_response,
                                      RCP_NCP_TIMEOUT, MODEM_NO_TX_LOCK));

  (void)rcp_os_mutex_unlock(&data_mdm->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_FS_GetSizeFile(RCP_Object_t *Obj, char *filename,
                                uint32_t *size) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(filename);
  RCP_NULL_ASSERT(size);

  /* Get the size of the specified file in the filesystem. The parameters are:
    - <type>: 0
    - <operation>: 4 (Get the size of a file)
    - <filename>: The name of the file to read
    The response is in the form of
    +FS:SIZE,<size> */
  snprintf(cmd, RCP_CMD_MATCH_BUFF_SIZE, "AT+FS=0,4,\"%s\"\r\n", filename);
  ret = RCP_Common_Query_Parse(Obj, cmd, "+FS:SIZE", &argc, argv,
                               RCP_SYS_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 2) {
    return RCP_STATUS_ERROR;
  }

  *size = (uint32_t)atoi(argv[1]);

  return ret;
}

RCP_Status_t RCP_FS_ListFiles(RCP_Object_t *Obj,
                              RCP_FS_FilesList_t *files_list) {
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT_STR(files_list, "File list pointer is NULL");

  struct modem *mdm = (struct modem *)&Obj->Modem;
  struct modem_cmd_handler_data *data =
      (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;

  struct modem_cmd handlers[] = {
      MODEM_CMD("", on_cmd_fs_listfiles, 1U, ""),
  };

  if (data == NULL) {
    return RCP_STATUS_ERROR;
  }
  (void)rcp_os_mutex_lock(&data->sem_tx_lock, RCP_OS_WAIT_FOREVER);

  mdm->rx_data = (void *)files_list;

  files_list->nb_files = 0; /* Reset the number of files */

  /* Get the files list in the filesystem. The parameters are:
    - <type>: 0
    - <operation>: 5 (List files in a directory)
    - <dirname>: The name of the directory to list files from
    The multiline responses are in the form of
    +FS:LIST,<nfiles>,[filename] */
  ret = RCP_Status(modem_cmd_send_ext(
      &mdm->iface, &mdm->modem_cmd_handler, handlers, ARRAY_SIZE(handlers),
      (const uint8_t *)"AT+FS=0,5,\".\"\r\n", &mdm->sem_response,
      RCP_NCP_TIMEOUT, MODEM_NO_TX_LOCK));

  (void)rcp_os_mutex_unlock(&data->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_GetVBAT(RCP_Object_t *Obj, uint32_t *Voltage) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  strncpy(cmd, "AT+VBAT?\r\n", sizeof(cmd));
  ret =
      RCP_Common_Query_Parse(Obj, cmd, "+VBAT:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 1) {
    return RCP_STATUS_ERROR;
  }

  *Voltage = (uint32_t)atoi(argv[0]);

  return ret;
}

RCP_Status_t RCP_GetPN(RCP_Object_t *Obj, char *PartNumber,
                       uint32_t PartNumberSize) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(PartNumber);

  if (PartNumberSize == 0U) {
    return RCP_STATUS_ERROR;
  }

  strncpy(cmd, "AT+PN?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+PN:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 1) {
    return RCP_STATUS_ERROR;
  }

  snprintf(PartNumber, PartNumberSize, "%s", argv[0]);

  return ret;
}

RCP_Status_t RCP_GetGMR(RCP_Object_t *Obj) {

  return RCP_ExeCommand(Obj, "AT+GMR");
}

RCP_Status_t RCP_GetGMAC(RCP_Object_t *Obj, uint8_t slot0_mac[6],
                         uint8_t slot1_mac[6], uint8_t slot2_mac[6]) {
  RCP_Status_t ret;
  rcp_gmac_ctx_t ctx;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(slot0_mac);
  RCP_NULL_ASSERT(slot1_mac);
  RCP_NULL_ASSERT(slot2_mac);

  struct modem *mdm = (struct modem *)&Obj->Modem;

  memset(slot0_mac, 0, 6);
  memset(slot1_mac, 0, 6);
  memset(slot2_mac, 0, 6);

  ctx.slot0 = slot0_mac;
  ctx.slot1 = slot1_mac;
  ctx.slot2 = slot2_mac;

  struct modem_cmd handlers[] = {
      MODEM_CMD("+GMACSLOT0:", on_cmd_gmac_slot0, 1, ""),
      MODEM_CMD("+GMACSLOT1:", on_cmd_gmac_slot1, 1, ""),
      MODEM_CMD("+GMACSLOT2:", on_cmd_gmac_slot2, 1, ""),
  };

  mdm->rx_data = (void *)&ctx;

  ret = RCP_Status(modem_cmd_send(
      &mdm->iface, &mdm->modem_cmd_handler, handlers, ARRAY_SIZE(handlers),
      (const uint8_t *)"AT+GMAC?\r\n", &mdm->sem_response, RCP_NCP_TIMEOUT));

  mdm->rx_data = NULL;
  return ret;
}

RCP_Status_t RCP_SetEcho(RCP_Object_t *Obj, uint32_t enable) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "ATE%" PRIu32 "\r\n", enable);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_GetCmdList(RCP_Object_t *Obj) {
  struct modem_cmd handlers[] = {
      MODEM_CMD("+CMD:", on_cmd_atcmd, 1U, ""),
  };
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  struct modem *mdm = (struct modem *)&Obj->Modem;
  return RCP_Status(modem_cmd_send(
      &mdm->iface, &mdm->modem_cmd_handler, handlers, ARRAY_SIZE(handlers),
      (const uint8_t *)"AT+CMD?\r\n", &mdm->sem_response, RCP_NCP_TIMEOUT));
}

RCP_Status_t RCP_GetSysMsg(RCP_Object_t *Obj, uint32_t *state) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(state);

  strncpy(cmd, "AT+SYSMSG?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+SYSMSG:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if ((ret == RCP_STATUS_OK) && (argc >= 1)) {
    *state = (uint32_t)strtoul(argv[0], NULL, 10);
  }
  return ret;
}

RCP_Status_t RCP_SetSysMsg(RCP_Object_t *Obj, uint32_t state) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SYSMSG=%" PRIu32 "\r\n", state);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_SYS_TIMEOUT);
}

RCP_Status_t RCP_GetSysLog(RCP_Object_t *Obj, uint32_t *state) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(state);

  strncpy(cmd, "AT+SYSLOG?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+SYSLOG:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if ((ret == RCP_STATUS_OK) && (argc >= 1)) {
    *state = (uint32_t)strtoul(argv[0], NULL, 10);
  }
  return ret;
}

RCP_Status_t RCP_SetSysLog(RCP_Object_t *Obj, uint32_t state) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SYSLOG=%" PRIu32 "\r\n", state);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_SYS_TIMEOUT);
}

RCP_Status_t RCP_GetTemp(RCP_Object_t *Obj, float *temp) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(temp);

  strncpy(cmd, "AT+TEMP?\r\n", sizeof(cmd));
  ret =
      RCP_Common_Query_Parse(Obj, cmd, "+TEMP:", &argc, argv, RCP_NCP_TIMEOUT);
  if ((ret == RCP_STATUS_OK) && (argc >= 1)) {
    *temp = strtof(argv[0], NULL);
  }
  return ret;
}

RCP_Status_t RCP_FWU_starts(RCP_Object_t *Obj, uint32_t enable) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Start or stop the firmware update process. The parameter is:
     - <enable>: 1 to start the firmware update process, 0 to stop it */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+OTASTART=%" PRIu32 "\r\n", enable);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_SYS_TIMEOUT);
}

RCP_Status_t RCP_FWU_Finish(RCP_Object_t *Obj) {
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* Finish the firmware update and reboot the RCP */
  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+OTAFIN\r\n",
                               RCP_SYS_TIMEOUT);
}

RCP_Status_t RCP_FWU_Send(RCP_Object_t *Obj, uint8_t *buff, uint32_t len) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(buff);

  /* Send a chunk of data for the firmware update. The parameter is:
     - <len>: The length of data to send
     The data to send is sent in the next request part */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+OTASEND=%" PRIu32 "\r\n", len);
  return RCP_Common_RequestSendOtaData(Obj, (uint8_t *)cmd, buff, len,
                                       RCP_SYS_TIMEOUT, true);
}

RCP_Status_t RCP_LowPowerConfig(RCP_Object_t *Obj, uint32_t WakeUpPinIn,
                                uint32_t ps_mode) {
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  /* In Hibernate Mode, only WakeUpPin 16 can be used */
  if ((ps_mode == 1U) && (WakeUpPinIn != 16U)) {
    return RCP_STATUS_ERROR;
  }

  Obj->LowPowerCfg.WakeUpPinIn = WakeUpPinIn;
  Obj->LowPowerCfg.PSMode = ps_mode;
  Obj->LowPowerCfg.WiFi_DTIM_Factor = 0U;
  Obj->LowPowerCfg.WiFi_DTIM_Interval = 0U;

  return RCP_STATUS_OK;
}

RCP_Status_t RCP_SetPowerMode(RCP_Object_t *Obj, uint32_t ps_mode,
                              uint32_t hbn_level) {
  RCP_Status_t ret;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  if (ps_mode == 1U) {
    snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+PWR=%" PRIu32 ",%" PRIu32 "\r\n",
             ps_mode, hbn_level);
    /* Hibernate mode does not return a response. */
    ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, 0U);
  } else {
    snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+PWR=%" PRIu32 "\r\n", ps_mode);
    ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
  }

  if (ret == RCP_STATUS_OK) {
    Obj->LowPowerCfg.PSMode = ps_mode;
  }

  return ret;
}

RCP_Status_t RCP_GetPowerMode(RCP_Object_t *Obj, uint32_t *ps_mode) {
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(ps_mode);

  *ps_mode = Obj->LowPowerCfg.PSMode;
  return RCP_STATUS_OK;
}

RCP_Status_t RCP_SetWakeUpPin(RCP_Object_t *Obj, uint32_t wakeup_pin) {
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SLWKIO=%" PRIu32 ",0\r\n",
           wakeup_pin);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_ExeCommand(RCP_Object_t *Obj, char *at_cmd) {
  RCP_Status_t ret;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  struct modem *mdm = (struct modem *)&Obj->Modem;

  struct modem_cmd handlers[] = {
      MODEM_CMD("", on_cmd_atcmd, 1, ""),
  };

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "%s\r\n", at_cmd);
  ret = RCP_Status(modem_cmd_send(
      &mdm->iface, &mdm->modem_cmd_handler, handlers, ARRAY_SIZE(handlers),
      (const uint8_t *)cmd, &mdm->sem_response, RCP_NCP_TIMEOUT));

  if (ret == RCP_STATUS_OK) {
    SYS_LOG_INFO("OK\n");
  } else {
    SYS_LOG_INFO("ERROR\n");
  }

  return ret;
}

/* Private Functions Definition ----------------------------------------------*/
static int gmac_parse_mac(const char *str, uint8_t *mac) {
  unsigned int b[6];
  if (sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4],
             &b[5]) != 6) {
    return -1;
  }
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t)b[i];
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_gmac_slot0) {
  struct modem *mdm = (struct modem *)data->user_data;
  rcp_gmac_ctx_t *ctx = (rcp_gmac_ctx_t *)mdm->rx_data;

  if ((argc >= 1) && (ctx != NULL) && (ctx->slot0 != NULL)) {
    gmac_parse_mac((const char *)argv[0], ctx->slot0);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_gmac_slot1) {
  struct modem *mdm = (struct modem *)data->user_data;
  rcp_gmac_ctx_t *ctx = (rcp_gmac_ctx_t *)mdm->rx_data;

  if ((argc >= 1) && (ctx != NULL) && (ctx->slot1 != NULL)) {
    gmac_parse_mac((const char *)argv[0], ctx->slot1);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_gmac_slot2) {
  struct modem *mdm = (struct modem *)data->user_data;
  rcp_gmac_ctx_t *ctx = (rcp_gmac_ctx_t *)mdm->rx_data;

  if ((argc >= 1) && (ctx != NULL) && (ctx->slot2 != NULL)) {
    gmac_parse_mac((const char *)argv[0], ctx->slot2);
  }
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_read_efuse) {
  struct modem *mdm = (struct modem *)data->user_data;

  uint8_t *ptr;
  uint32_t offset;
  uint8_t *endptr;
  uint16_t rx_data_len;

  if (data->rx_buf == NULL) {
    return -EINVAL;
  }
  ptr = data->rx_buf + len;
  data->rx_buf[data->rx_buf_len] = 0;

  rx_data_len = strtol((char *)ptr, (char **)&endptr, 10);
  if ((endptr == ptr) || (*endptr != ',')) {
    SYS_LOG_ERROR("Invalid EFUSE read response format");
    return -EINVAL;
  }
  offset = endptr - data->rx_buf + 1;

  if (data->rx_buf_len >= offset + rx_data_len) {
    memcpy(mdm->rx_data, endptr + 1, rx_data_len);
    modem_cmd_handler_set_error(data, 0);
    (void)rcp_os_sem_give(&mdm->sem_response);
    return offset + rx_data_len;
  } else {
    return -EAGAIN;
  }
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_read_flash) {
  struct modem *mdm = (struct modem *)data->user_data;

  uint8_t *ptr;
  uint32_t offset;
  uint8_t *endptr;
  uint16_t rx_data_len;

  if (data->rx_buf == NULL) {
    return -EINVAL;
  }
  ptr = data->rx_buf + len;
  data->rx_buf[data->rx_buf_len] = 0;

  rx_data_len = strtol((char *)ptr, (char **)&endptr, 10);
  if ((endptr == ptr) || (*endptr != ',')) {
    SYS_LOG_ERROR("Invalid FLASH-R response format");
    return -EINVAL;
  }
  offset = (uint32_t)(endptr - data->rx_buf) + 1U;

  if (data->rx_buf_len >= (offset + rx_data_len)) {
    memcpy(mdm->rx_data, endptr + 1, rx_data_len);
    modem_cmd_handler_set_error(data, 0);
    (void)rcp_os_sem_give(&mdm->sem_response);
    return (int)(offset + rx_data_len);
  } else {
    return -EAGAIN;
  }
}

MODEM_CMD_DEFINE(on_cmd_part_line) {
  struct modem *mdm = (struct modem *)data->user_data;
  rcp_part_query_ctx_t *ctx = (rcp_part_query_ctx_t *)mdm->rx_data;
  RCP_PartInfo_t part = {0};
  unsigned long active_idx;
  unsigned long age;
  unsigned long addr0;
  unsigned long addr1;
  unsigned long len0;
  unsigned long len1;
  int matched;

  (void)argc;
  (void)argv;

  if ((data->match_buf == NULL) || (ctx == NULL)) {
    return 0;
  }

  matched = sscanf((char *)data->match_buf,
                   "+PART=%lu,%lu,\"%15[^\"]\",0x%lx,0x%lx,%lu,%lu",
                   &active_idx, &age, part.name, &addr0, &addr1, &len0, &len1);
  if (matched != 7) {
    return -EINVAL;
  }

  part.active_idx = (uint32_t)active_idx;
  part.age = (uint32_t)age;
  part.addr0 = (uint32_t)addr0;
  part.addr1 = (uint32_t)addr1;
  part.len0 = (uint32_t)len0;
  part.len1 = (uint32_t)len1;
  rcp_trim_spaces(part.name);

  if ((ctx->parts != NULL) && (ctx->count < ctx->capacity)) {
    ctx->parts[ctx->count] = part;
  }
  ctx->count++;
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_minidump_line) {
  struct modem *mdm = (struct modem *)data->user_data;
  rcp_minidump_ctx_t *ctx = (rcp_minidump_ctx_t *)mdm->rx_data;
  uint32_t consumed = 0U;

  (void)len;
  (void)argv;
  (void)argc;

  if ((data->rx_buf == NULL) || (data->rx_buf_len == 0U) || (ctx == NULL)) {
    return -EINVAL;
  }

  while (consumed < data->rx_buf_len) {
    int8_t nibble = rcp_hex_nibble(data->rx_buf[consumed]);

    if (nibble < 0) {
      break;
    }

    consumed++;

    if (!ctx->has_pending_nibble) {
      ctx->pending_nibble = nibble;
      ctx->has_pending_nibble = true;
      continue;
    }

    if ((ctx->dump_data != NULL) && (ctx->capacity > 0U) &&
        (ctx->length < ctx->capacity)) {
      ctx->dump_data[ctx->length] =
          (uint8_t)((ctx->pending_nibble << 4) | nibble);
      ctx->length++;
    } else {
      ctx->overflow = true;
    }

    ctx->has_pending_nibble = false;
  }

  if ((consumed < data->rx_buf_len) && ctx->has_pending_nibble) {
    return -EINVAL;
  }

  if (consumed == 0U) {
    return -EINVAL;
  }

  return (int32_t)consumed;
}

MODEM_CMD_DEFINE(on_cmd_fs_listfiles) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_FS_FilesList_t *files_list = (RCP_FS_FilesList_t *)mdm->rx_data;

  if ((files_list != NULL) && (argc >= 1) &&
      (files_list->nb_files < RCP_SYS_FS_MAX_FILES) && (argv[0][0] != '.') &&
      (strcmp((char *)argv[0], "+FS:LIST") != 0)) {
    /* Copy the filename */
    snprintf(files_list->filename[files_list->nb_files],
             RCP_SYS_FS_FILENAME_SIZE, "%s", argv[0]);

    files_list->nb_files++; /* Increment the number of files */
  }
  return 0;
}

MODEM_CMD_DIRECT_DEFINE(on_cmd_fs_readfile) {
  struct modem *mdm = (struct modem *)data->user_data;

  uint8_t *ptr;
  uint32_t offset;
  uint8_t *endptr;
  uint16_t rx_data_len;

  if (data->rx_buf == NULL) {
    return -EINVAL;
  }
  ptr = data->rx_buf + len;
  data->rx_buf[data->rx_buf_len] = 0;

  rx_data_len = strtol((char *)ptr, (char **)&endptr, 10);
  if ((endptr == ptr) || (*endptr != ',')) {
    SYS_LOG_ERROR("Invalid FS read response format");
    return -EINVAL;
  }
  offset = endptr - data->rx_buf + 1;

  if (data->rx_buf_len >= (offset + rx_data_len)) {
    memcpy(mdm->rx_data, endptr + 1, rx_data_len);
    modem_cmd_handler_set_error(data, 0);
    (void)rcp_os_sem_give(&mdm->sem_response);
    return offset + rx_data_len;
  } else {
    return -EAGAIN;
  }
}

MODEM_CMD_DEFINE(on_cmd_atcmd) {
  (void)argc;
  (void)argv;

  if ((data->match_buf != NULL) && (data->match_buf[0] != '\0')) {
    SYS_LOG_INFO("%s\n", data->match_buf);
    /*for AT+CMD?, log needs some time to push */
    rcp_os_delay_ms(5);
  }
  return 0;
}

/** @} */
