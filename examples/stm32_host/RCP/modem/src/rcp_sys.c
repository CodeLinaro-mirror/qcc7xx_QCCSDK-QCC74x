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
#include <limits.h>
#include <stdio.h>

#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Private typedef -----------------------------------------------------------*/
/** @addtogroup RCP_System_Types
 * @{
 */

typedef struct {
  uint32_t efuse_addr;
  uint32_t efuse_len;
  uint8_t *data;
  const char *desc;
} RCP_Efuse_t;

typedef struct {
  uint32_t en_addr;
  uint32_t en_offset;
  uint32_t value_addr;
  uint32_t value_offset;
  uint32_t value_len;
  uint32_t parity_addr;
  uint32_t parity_offset;
  uint32_t type;
  const char *desc;
} RCP_Trim_t;

/** @} */

/* Private defines -----------------------------------------------------------*/
/** @addtogroup RCP_System_Constants
 * @{
 */

#define RCP_EFUSE_DEFAULT_MAC_ADDR_ADDR (0x14U)
#define RCP_EFUSE_DEFAULT_MAC_ADDR_LEN (7U)

#define RCP_EFUSE_CUSTOM_MAC_ADDR_1_ADDR (0x64U)
#define RCP_EFUSE_CUSTOM_MAC_ADDR_1_LEN (7U)

#define RCP_EFUSE_CUSTOM_MAC_ADDR_2_ADDR (0x70U)
#define RCP_EFUSE_CUSTOM_MAC_ADDR_2_LEN (7U)

#define RCP_EFUSE_ANTI_ROLL_BACK_EN_ADDR (0x7CU)
#define RCP_EFUSE_ANTI_ROLL_BACK_EN_LEN (4U)

#define RCP_EFUSE_PART_NUMBER_ADDR (0x100U)
#define RCP_EFUSE_PART_NUMBER_LEN (24U)

#define RCP_EFUSE_MANUF_BOM_ADDR (0x118U)
#define RCP_EFUSE_MANUF_BOM_LEN (4U)

#define RCP_EFUSE_BOOT2_ANTI_ROLL_BACK_ADDR (0x170U)
#define RCP_EFUSE_BOOT2_ANTI_ROLL_BACK_LEN (16U)

#define RCP_EFUSE_APP_ANTI_ROLL_BACK_ADDR (0x180U)
#define RCP_EFUSE_APP_ANTI_ROLL_BACK_LEN (32U)

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

static const RCP_Efuse_t rcp_efuse_mac_table[] = {
    {RCP_EFUSE_DEFAULT_MAC_ADDR_ADDR, RCP_EFUSE_DEFAULT_MAC_ADDR_LEN, NULL,
     "Default MAC Address"},
    {RCP_EFUSE_CUSTOM_MAC_ADDR_1_ADDR, RCP_EFUSE_CUSTOM_MAC_ADDR_1_LEN, NULL,
     "Custom MAC Address 1"},
    {RCP_EFUSE_CUSTOM_MAC_ADDR_2_ADDR, RCP_EFUSE_CUSTOM_MAC_ADDR_2_LEN, NULL,
     "Custom MAC Address 2"},
};

static const RCP_Trim_t rcp_trim_table[] = {
    {0xCC, 26, 0xC0, 0, 15, 0xC0, 15, 0, "wifi_hp_poffset0"},
    {0xCC, 27, 0xC0, 16, 15, 0xC0, 31, 0, "wifi_hp_poffset1"},
    {0xCC, 28, 0xC4, 0, 15, 0xC4, 15, 0, "wifi_hp_poffset2"},
    {0xCC, 29, 0xC4, 16, 15, 0xC4, 31, 1, "wifi_lp_poffset0"},
    {0xCC, 30, 0xC8, 0, 15, 0xC8, 15, 1, "wifi_lp_poffset1"},
    {0xCC, 31, 0xC8, 16, 15, 0xC8, 31, 1, "wifi_lp_poffset2"},
    {0xD0, 26, 0xCC, 0, 25, 0xCC, 25, 2, "ble_poffset0"},
    {0xD0, 27, 0xD0, 0, 25, 0xD0, 25, 2, "ble_poffset1"},
    {0xD0, 28, 0xD4, 0, 25, 0xD4, 25, 2, "ble_poffset2"},
    {0xEC, 7, 0XEC, 0, 6, 0XEC, 6, 3, "xtal0"},
    {0xF0, 31, 0XF4, 26, 6, 0XF0, 30, 3, "xtal1"},
    {0xEC, 23, 0XF4, 20, 6, 0XF0, 28, 3, "xtal2"},
};

static const RCP_ModuleID_t rcp_module_id_table[] = {
    {RCP_MODULE_ID_B, "C6AFDBD111400004"},
    {RCP_MODULE_ID_U, "72AFDBD110400005"},
    {RCP_MODULE_ID_P, "A6AFDBD110400006"},
};

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
 * @brief  Callback function to handle Module AT version responses
 */
MODEM_CMD_DECLARE(on_cmd_at_version);

/**
 * @brief  Callback function to handle Module Wi-Fi MAC SW version responses
 */
MODEM_CMD_DECLARE(on_cmd_mac_sw_version);

/**
 * @brief  Callback function to handle Module SDK version responses
 */
MODEM_CMD_DECLARE(on_cmd_sdk_version);

/**
 * @brief  Callback function to handle Module BLE Controller version responses
 */
MODEM_CMD_DECLARE(on_cmd_bt_controller_version);

/**
 * @brief  Callback function to handle Module BLE Stack version responses
 */
MODEM_CMD_DECLARE(on_cmd_bt_stack_version);

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

  ret = RCP_GetModuleInfo(Obj);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Get RCP Info failed\n");
    goto __error;
  }

  RCP_Net_DhcpType_e dhcp_state = RCP_NET_DHCP_STA_AP_ENABLED;
  uint32_t dhcp_operate = 0U;
  ret = RCP_Net_SetDhcpConfig(Obj, &dhcp_state, &dhcp_operate);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Disable device DHCP failed\n");
    goto __error;
  }
  RCP_Logger_SetEnable(true);

__error:
  return ret;
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

RCP_Status_t RCP_SdkMinVersion(RCP_Object_t *Obj, uint32_t major,
                               uint32_t sub1, uint32_t sub2) {
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  if ((Obj->ModuleInfo.SDK_Version.Major > major) ||
      ((Obj->ModuleInfo.SDK_Version.Major == major) &&
       (Obj->ModuleInfo.SDK_Version.Sub1 > sub1)) ||
      ((Obj->ModuleInfo.SDK_Version.Major == major) &&
       (Obj->ModuleInfo.SDK_Version.Sub1 == sub1) &&
       (Obj->ModuleInfo.SDK_Version.Sub2 >= sub2))) {
    return RCP_STATUS_OK;
  }

  return RCP_STATUS_ERROR;
}

RCP_Status_t RCP_ResetToFactoryDefault(RCP_Object_t *Obj) {
  RCP_Status_t ret;
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  struct modem *mdm = (struct modem *)&Obj->Modem;
  rcp_os_sem_take(&mdm->sem_if_ready, 0);

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

  struct modem *mdm = (struct modem *)&Obj->Modem;
  rcp_os_sem_take(&mdm->sem_if_ready, 0);

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

RCP_Status_t RCP_ReadEFuse(RCP_Object_t *Obj, uint32_t addr, uint32_t nbytes,
                           uint8_t *data) {
  char addr_str[12];
  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);
  RCP_NULL_ASSERT(data);

  snprintf(addr_str, sizeof(addr_str), "0x%03" PRIx32, addr);
  return RCP_EFuseRead(Obj, nbytes, addr_str, 1U, data);
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

RCP_Status_t RCP_GetModuleInfo(RCP_Object_t *Obj) {
  RCP_Status_t ret;
  uint32_t data[8] = {0};
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  struct modem *mdm;

  RCP_NULL_ASSERT_STR(Obj, RCP_Obj_Null_str);

  mdm = (struct modem *)&Obj->Modem;

  memset(&Obj->ModuleInfo, 0, sizeof(RCP_ModuleInfo_t));

  struct modem_cmd handlers[] = {
      MODEM_CMD_ARGS_MAX("AT version:", on_cmd_at_version, 1U, 10U, ".()"),
      MODEM_CMD_ARGS_MAX("component_version_macsw_", on_cmd_mac_sw_version, 1U,
                         10U, "."),
      MODEM_CMD_ARGS_MAX("component_version_sdk_", on_cmd_sdk_version, 1U, 10U,
                         "."),
      MODEM_CMD_ARGS_MAX("lib_version_btblecontroller_",
                         on_cmd_bt_controller_version, 1U, 10U, "."),
      MODEM_CMD_ARGS_MAX("component_version_btble_", on_cmd_bt_stack_version, 1U,
                         10U, "."),
  };

  ret = RCP_Status(modem_cmd_send(&mdm->iface, &mdm->modem_cmd_handler,
                                  handlers, ARRAY_SIZE(handlers),
                                  (const uint8_t *)"AT+GMR\r\n",
                                  &mdm->sem_response, RCP_NCP_TIMEOUT));
  if (ret != RCP_STATUS_OK) {
    goto _err;
  }

  strncpy(cmd, "AT+VBAT?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+VBAT:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 1U) {
    return RCP_STATUS_ERROR;
  }
  Obj->ModuleInfo.BatteryVoltage = (uint32_t)atoi(argv[0]);

  for (uint32_t i = 0; i < ARRAY_SIZE(rcp_trim_table); i++) {
    ret = RCP_ReadEFuse(Obj, rcp_trim_table[i].en_addr, 4U, (uint8_t *)data);
    if (ret != RCP_STATUS_OK) {
      SYS_LOG_ERROR("Unable to read %s\n", rcp_trim_table[i].desc);
      goto _err;
    }

    if (((data[0] >> rcp_trim_table[i].en_offset) & 0x1U) != 0U) {
      uint32_t cnt = 0U;
      uint32_t k;
      int32_t step = 0;
      int8_t pwr_offset[14];
      int8_t pwr_offset_tmp[3];
      uint32_t trim_parity;
      uint32_t trim_value;

      ret =
          RCP_ReadEFuse(Obj, rcp_trim_table[i].parity_addr, 4U, (uint8_t *)&data[0]);
      if (ret != RCP_STATUS_OK) {
        SYS_LOG_ERROR("Unable to read %s\n", rcp_trim_table[i].desc);
        goto _err;
      }

      ret =
          RCP_ReadEFuse(Obj, rcp_trim_table[i].value_addr, 4U, (uint8_t *)&data[1]);
      if (ret != RCP_STATUS_OK) {
        SYS_LOG_ERROR("Unable to read %s\n", rcp_trim_table[i].desc);
        goto _err;
      }

      trim_parity = (data[0] >> rcp_trim_table[i].parity_offset) & 0x1U;
      trim_value = (data[1] >> rcp_trim_table[i].value_offset) &
                   ((1U << rcp_trim_table[i].value_len) - 1U);

      for (k = 0; k < rcp_trim_table[i].value_len; k++) {
        if ((trim_value & (1UL << k)) != 0U) {
          cnt++;
        }
      }

      if ((cnt & 0x1U) != trim_parity) {
        continue;
      }

      if (rcp_trim_table[i].type == 2U) {
        for (k = 0; k < 5U; k++) {
          uint32_t ble_val = (trim_value >> (k * 5U)) & 0x1FU;
          Obj->ModuleInfo.trim_ble[k] = (int8_t)ble_val;

          if (Obj->ModuleInfo.trim_ble[k] >= 16) {
            Obj->ModuleInfo.trim_ble[k] -= 32;
          }
        }
      }

      if ((rcp_trim_table[i].type == 0U) || (rcp_trim_table[i].type == 1U)) {
        for (k = 0; k < 3U; k++) {
          uint32_t wifi_val = (trim_value >> (k * 5U)) & 0x1FU;
          pwr_offset_tmp[k] = (int8_t)wifi_val;

          if (pwr_offset_tmp[k] >= 16) {
            pwr_offset_tmp[k] -= 32;
          }
        }

        pwr_offset[0] = pwr_offset_tmp[0];
        pwr_offset[6] = pwr_offset_tmp[1];
        pwr_offset[12] = pwr_offset_tmp[2];

        step = (pwr_offset_tmp[1] - pwr_offset_tmp[0]) * 100 / 6;
        for (k = 1; k < 6U; k++) {
          pwr_offset[k] = ((step * k) + 50) / 100 + pwr_offset_tmp[0];
        }

        step = (pwr_offset_tmp[2] - pwr_offset_tmp[1]) * 100 / 6;
        for (k = 7; k < 12U; k++) {
          pwr_offset[k] =
              ((step * (int32_t)(k - 6U)) + 50) / 100 + pwr_offset_tmp[1];
        }

        pwr_offset[13] = (step * 7 + 50) / 100 + pwr_offset_tmp[1];

        if (rcp_trim_table[i].type == 0U) {
          memcpy(Obj->ModuleInfo.trim_wifi_hp, pwr_offset,
                 sizeof(Obj->ModuleInfo.trim_wifi_hp));
        } else {
          memcpy(Obj->ModuleInfo.trim_wifi_lp, pwr_offset,
                 sizeof(Obj->ModuleInfo.trim_wifi_lp));
        }
      }

      if (rcp_trim_table[i].type == 3U) {
        Obj->ModuleInfo.trim_xtal = (int8_t)trim_value;
      }
    }
  }

  ret = RCP_ReadEFuse(Obj, RCP_EFUSE_PART_NUMBER_ADDR,
                      RCP_EFUSE_PART_NUMBER_LEN, (uint8_t *)data);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Unable to read Part number\n");
    goto _err;
  }

  if ((data[0] != 0U) || (data[1] != 0U) || (data[2] != 0U)) {
    uint32_t char_cnt = 0U;
    uint8_t *byte = (uint8_t *)data;
    char part_number[RCP_EFUSE_PART_NUMBER_LEN + 1U] = {0};

    for (char_cnt = 0U; char_cnt < RCP_EFUSE_PART_NUMBER_LEN; char_cnt++) {
      if ((byte[char_cnt] == 0U) || (byte[char_cnt] == 0x03U)) {
        break;
      }
      snprintf(part_number + char_cnt, 2U, "%c", byte[char_cnt]);
    }

    strncpy(Obj->ModuleInfo.ModuleID.ModuleName, part_number,
            sizeof(Obj->ModuleInfo.ModuleID.ModuleName) - 1U);
    Obj->ModuleInfo.ModuleID.ModuleName
        [sizeof(Obj->ModuleInfo.ModuleID.ModuleName) - 1U] = '\0';

    for (uint32_t i = 0; i < ARRAY_SIZE(rcp_module_id_table); i++) {
      if (strncmp(Obj->ModuleInfo.ModuleID.ModuleName,
                  rcp_module_id_table[i].ModuleName,
                  sizeof(rcp_module_id_table[i].ModuleName)) == 0) {
        Obj->ModuleInfo.ModuleID.ModuleID = rcp_module_id_table[i].ModuleID;
        break;
      }
    }
  }

  ret = RCP_ReadEFuse(Obj, RCP_EFUSE_MANUF_BOM_ADDR, RCP_EFUSE_MANUF_BOM_LEN,
                      (uint8_t *)data);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Unable to read BOM ID & Manufacturing Year/Week\n");
    goto _err;
  }

  if (data[0] > 0U) {
    Obj->ModuleInfo.BomID = ((data[0] >> 8) & 0xFFU) | ((data[0] & 0xFFU) << 8);
    Obj->ModuleInfo.Manufacturing_Year = (data[0] >> 16) & 0xFFU;
    Obj->ModuleInfo.Manufacturing_Week = (data[0] >> 24) & 0xFFU;
  }

  for (uint32_t i = 0; i < ARRAY_SIZE(rcp_efuse_mac_table); i++) {
    uint32_t cnt = 0U;
    uint32_t byte_cnt;
    uint32_t bit_cnt;
    uint8_t *byte;

    ret = RCP_ReadEFuse(Obj, rcp_efuse_mac_table[i].efuse_addr,
                        rcp_efuse_mac_table[i].efuse_len, (uint8_t *)data);
    if (ret != RCP_STATUS_OK) {
      SYS_LOG_ERROR("Unable to read %s\n", rcp_efuse_mac_table[i].desc);
      goto _err;
    }

    byte = (uint8_t *)data;
    for (byte_cnt = 0U; byte_cnt < 6U; byte_cnt++) {
      for (bit_cnt = 0U; bit_cnt < 8U; bit_cnt++) {
        if ((byte[byte_cnt] & (1U << bit_cnt)) == 0U) {
          cnt++;
        }
      }
    }

    if ((cnt & 0x3fU) == ((data[1] >> 16U) & 0x3FU)) {
      for (byte_cnt = 0U; byte_cnt < 6U; byte_cnt++) {
        Obj->ModuleInfo.Mac_Address[byte_cnt] = byte[5U - byte_cnt];
      }
    }
  }

  ret = RCP_ReadEFuse(Obj, RCP_EFUSE_ANTI_ROLL_BACK_EN_ADDR,
                      RCP_EFUSE_ANTI_ROLL_BACK_EN_LEN, (uint8_t *)data);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Unable to read Anti-rollback enable\n");
    goto _err;
  }

  if (((data[0] >> 12U) & 0x1U) != 0U) {
    RCP_Efuse_t efuse_antirollback_table[] = {
        {RCP_EFUSE_BOOT2_ANTI_ROLL_BACK_ADDR,
         RCP_EFUSE_BOOT2_ANTI_ROLL_BACK_LEN,
         &Obj->ModuleInfo.AntiRollbackBootloader, "Bootloader"},
        {RCP_EFUSE_APP_ANTI_ROLL_BACK_ADDR, RCP_EFUSE_APP_ANTI_ROLL_BACK_LEN,
         &Obj->ModuleInfo.AntiRollbackApp, "Application"},
    };

    for (uint32_t i = 0; i < ARRAY_SIZE(efuse_antirollback_table); i++) {
      uint8_t half_index = efuse_antirollback_table[i].efuse_len / 8U;

      ret = RCP_ReadEFuse(Obj, efuse_antirollback_table[i].efuse_addr,
                          efuse_antirollback_table[i].efuse_len,
                          (uint8_t *)data);
      if (ret != RCP_STATUS_OK) {
        SYS_LOG_ERROR("Unable to read Anti-rollback %s\n",
                      efuse_antirollback_table[i].desc);
        goto _err;
      }

      for (uint32_t j = 0; j < half_index; j++) {
        *efuse_antirollback_table[i].data +=
            32U - __CLZ(data[j] | data[j + half_index]);
      }
    }
  }

_err:
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
  return RCP_Common_RequestSendData(Obj, (uint8_t *)cmd, buff, len,
                                       RCP_SYS_TIMEOUT, true);
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
MODEM_CMD_DEFINE(on_cmd_at_version) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (argc < 5U) {
    return 0;
  }

  Obj->ModuleInfo.FW_Version.Major = (uint8_t)atoi((char *)argv[0]);
  Obj->ModuleInfo.FW_Version.Sub1 = (uint8_t)atoi((char *)argv[1]);
  Obj->ModuleInfo.FW_Version.Sub2 = (uint8_t)atoi((char *)argv[2]);
  Obj->ModuleInfo.FW_Version.Patch = (uint8_t)atoi((char *)argv[3]);
  strncpy((char *)Obj->ModuleInfo.Build_Date, (char *)argv[4],
          sizeof(Obj->ModuleInfo.Build_Date) - 1U);
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_mac_sw_version) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (argc < 3U) {
    return 0;
  }

  Obj->ModuleInfo.WiFi_MAC_Version.Major = (uint8_t)atoi((char *)argv[0]);
  Obj->ModuleInfo.WiFi_MAC_Version.Sub1 = (uint8_t)atoi((char *)argv[1]);
  Obj->ModuleInfo.WiFi_MAC_Version.Sub2 = (uint8_t)atoi((char *)argv[2]);
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_sdk_version) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (argc < 3U) {
    return 0;
  }

  Obj->ModuleInfo.SDK_Version.Major = (uint8_t)atoi((char *)argv[0]);
  Obj->ModuleInfo.SDK_Version.Sub1 = (uint8_t)atoi((char *)argv[1]);
  Obj->ModuleInfo.SDK_Version.Sub2 = (uint8_t)atoi((char *)argv[2]);
  if (argc > 3U) {
    Obj->ModuleInfo.SDK_Version.Patch = (uint8_t)atoi((char *)argv[3]);
  }
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_bt_controller_version) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (argc < 3U) {
    return 0;
  }

  Obj->ModuleInfo.BT_Controller_Version.Major = (uint8_t)atoi((char *)argv[0]);
  Obj->ModuleInfo.BT_Controller_Version.Sub1 = (uint8_t)atoi((char *)argv[1]);
  Obj->ModuleInfo.BT_Controller_Version.Sub2 = (uint8_t)atoi((char *)argv[2]);
  return 0;
}

MODEM_CMD_DEFINE(on_cmd_bt_stack_version) {
  struct modem *mdm = (struct modem *)data->user_data;
  RCP_Object_t *Obj = CONTAINER_OF(mdm, RCP_Object_t, Modem);

  if (argc < 3U) {
    return 0;
  }

  Obj->ModuleInfo.BT_Stack_Version.Major = (uint8_t)atoi((char *)argv[0]);
  Obj->ModuleInfo.BT_Stack_Version.Sub1 = (uint8_t)atoi((char *)argv[1]);
  Obj->ModuleInfo.BT_Stack_Version.Sub2 = (uint8_t)atoi((char *)argv[2]);
  return 0;
}

static int gmac_parse_mac(const char *str, uint8_t *mac) {
  unsigned int b[6];
  if (sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4],
             &b[5]) != 6) {
    return -1;
  }
  for (int i = 0; i < 6; i++) {
    if (b[i] > UINT8_MAX) {
      return -1;
    }
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
  long parsed_len;

  if (data->rx_buf == NULL) {
    return -EINVAL;
  }
  ptr = data->rx_buf + len;
  data->rx_buf[data->rx_buf_len] = 0;

  parsed_len = strtol((char *)ptr, (char **)&endptr, 10);
  if ((endptr == ptr) || (*endptr != ',') ||
      parsed_len < 0 || parsed_len > UINT16_MAX) {
    SYS_LOG_ERROR("Invalid EFUSE read response format");
    return -EINVAL;
  }
  rx_data_len = (uint16_t)parsed_len;
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
      if ((data->rx_buf[consumed] == '\r') || (data->rx_buf[consumed] == '\n')) {
        break;
      }
      return -EINVAL;
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
