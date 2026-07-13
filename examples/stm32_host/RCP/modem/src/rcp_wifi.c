/**
  ******************************************************************************
  * @file    rcp_wifi.c
  * @author  GPM Application Team
  * @brief   This file provides code for RCP WiFi AT module
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rcp_api.h"
#include "rcp_common.h"
#include "rcp_internal.h"
#include "common_parser.h" /* Common Parser functions */

#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Global variables ----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67RCP_AT_WiFi_Constants
  * @{
  */

#define RCP_WIFI_RECONNECTION_INTERVAL            7200                    /*!< Reconnection interval in seconds */
#define RCP_WIFI_RECONNECTION_ATTEMPTS            1000                    /*!< Reconnection attempts */
#define RCP_WIFI_CONNECT_TIMEOUT                  3000                    /*!< Connect command status timeout */

#define RCP_WIFI_COUNTRY_CODE_MAX                 5                       /*!< Maximum number of country codes */
#define RCP_WIFI_MAC_LENGTH                       17                      /*!< Length of a complete MAC address string */
#define RCP_WIFI_POWER_PERIOD_MAX                 3600000U                /*!< Maximum low-power period value */

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67RCP_AT_WiFi_Variables ST67RCP AT Driver Wi-Fi Variables
  * @ingroup  ST67RCP_AT_WiFi
  * @{
  */

/** @brief  Scan options structure */
RCP_WiFi_Scan_Opts_t ScanOptions = {"\0", "\0", RCP_WIFI_SCAN_STATE_ACTIVE, 0, 50};

/** @brief  Array of Alpha-2 country codes */
static const char *const Country_code_str[] = {"CN", "JP", "US", "EU", "00"};

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67RCP_AT_WiFi_Functions
  * @{
  */

/**
  * @brief  Callback function to handle Wi-Fi AP get list of stations responses
  * @param  data: pointer to the modem_cmd_handler_data structure
  * @param  len: length of the data
  * @param  argv: array of argument strings
  * @param  argc: number of argument
  * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_ap_liststa);

/**
  * @brief  Callback function to handle Wi-Fi TWT get status responses
  * @param  data: pointer to the modem_cmd_handler_data structure
  * @param  len: length of the data
  * @param  argv: array of argument strings
  * @param  argc: number of argument
  * @return 0 on success, negative value on error
 */
MODEM_CMD_DECLARE(on_cmd_twt_status);

/**
  * @brief  Parses WiFi event and call related callback
  * @param  hObj: pointer to module handle
  * @param  argc: pointer to argument count
  * @param  argv: pointer to argument values
  */
static void RCP_WiFi_AT_Event(void *hObj, uint16_t *argc, char **argv);

/**
  * @brief  Callback function to handle scan results
  * @param  hObj: pointer to module handle
  * @param  argc: pointer to argument count
  * @param  argv: pointer to argument values
  */
static void RCP_WiFi_scan_result(void *hObj, uint16_t *argc, char **argv);

/* Functions Definition ------------------------------------------------------*/
RCP_Status_t RCP_WiFi_Init(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  Obj->WifiCtx.ScanResults.AP = NULL;
  Obj->WifiCtx.ScanResults.Count = 0;

  Obj->Callbacks.WiFi_event_cb = RCP_WiFi_AT_Event;
  Obj->Callbacks.WiFi_event_scan_cb = RCP_WiFi_scan_result;
  Obj->Callbacks.WiFi_csi_report_cb = NULL;

  return RCP_STATUS_OK;
}

RCP_Status_t RCP_WiFi_DeInit(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  if (Obj->WifiCtx.ScanResults.AP != NULL)
  {
    rcp_os_free(Obj->WifiCtx.ScanResults.AP);
    Obj->WifiCtx.ScanResults.AP = NULL;
  }
  Obj->WifiCtx.ScanResults.Count = 0;

  Obj->Callbacks.WiFi_event_cb = NULL;
  Obj->Callbacks.WiFi_event_scan_cb = NULL;
  Obj->Callbacks.WiFi_csi_report_cb = NULL;

  return RCP_STATUS_OK;
}

RCP_Status_t RCP_WiFi_SetAutoConnect(RCP_Object_t *Obj, uint32_t OnOff)
{
  RCP_Status_t ret = RCP_STATUS_ERROR;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if (!((OnOff == 0) || (OnOff == 1)))
  {
    WIFI_LOG_ERROR("Invalid value passed to Autoconnect function\n");
    return ret;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWAUTOCONN=%" PRIu32 "\r\n", OnOff);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);

  if (ret == RCP_STATUS_OK)
  {
    Obj->WifiCtx.STASettings.AutoConnect = OnOff;
  }
  return ret;
}

RCP_Status_t RCP_WiFi_GetAutoConnect(RCP_Object_t *Obj, uint32_t *OnOff)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(OnOff);

  strncpy(cmd, "AT+CWAUTOCONN?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWAUTOCONN:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *OnOff = atoi(argv[0]);

  return ret;
}

RCP_Status_t RCP_WiFi_Station_Start(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+CWMODE=1,0\r\n", RCP_WIFI_TIMEOUT);
}

RCP_Status_t RCP_WiFi_SetScanOpts(RCP_Object_t *Obj, RCP_WiFi_Scan_Opts_t *ScanOpts)
{
  uint32_t max_cnt = 50;
  RCP_WiFi_scan_type_e type = RCP_WIFI_SCAN_STATE_ACTIVE;
  uint8_t SSID[RCP_WIFI_MAX_SSID_SIZE + 1] = {'\0'};
  uint8_t MAC[6] = {'\0'};
  uint8_t channel = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(ScanOpts);

  if ((ScanOpts->MaxCnt > 0) && (ScanOpts->MaxCnt < 50))
  {
    max_cnt = ScanOpts->MaxCnt;
  }
  if ((ScanOpts->scan_type == 0) || (ScanOpts->scan_type == 1))
  {
    ScanOptions.scan_type = ScanOpts->scan_type;
  }
  else
  {
    ScanOptions.scan_type = type;
  }
  if (ScanOpts->SSID[0] != '\0')
  {
    memcpy(ScanOptions.SSID, ScanOpts->SSID, RCP_WIFI_MAX_SSID_SIZE + 1);
  }
  else
  {
    memcpy(ScanOptions.SSID, SSID, RCP_WIFI_MAX_SSID_SIZE + 1);
  }
  if (ScanOpts->MAC[0] != '\0')
  {
    memcpy(ScanOptions.MAC, ScanOpts->MAC, 6);
  }
  else
  {
    memcpy(ScanOptions.MAC, MAC, 6);
  }
  if ((ScanOpts->Channel > 0) && (ScanOpts->Channel < 13))
  {
    ScanOptions.Channel = ScanOpts->Channel;
  }
  else
  {
    ScanOptions.Channel = channel;
  }

  /* The config of the scan options must be made before every scan
   * The second parameter is a bit mask to select the fields to display,
   * with the bytes 5,6 and 8 unused */
  snprintf((char *)cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWLAPOPT=1,1695,-100,255,%" PRIu32 "\r\n", max_cnt);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_Scan(RCP_Object_t *Obj)
{
  char MAC[20 + 1];
  char SSID[RCP_WIFI_MAX_SSID_SIZE + 3] = {'\0'}; /* Size + 2 if contains double-quote characters */
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if (Obj->WifiCtx.ScanResults.AP == NULL)
  {
    Obj->WifiCtx.ScanResults.AP = rcp_os_malloc(sizeof(RCP_WiFi_AP_t) * RCP_WIFI_MAX_DETECTED_AP);
    if ((Obj->WifiCtx.ScanResults.AP == NULL) && (RCP_WIFI_MAX_DETECTED_AP != 0))
    {
      WIFI_LOG_ERROR("Error: Unable to allocate memory for scan results\n");
      return RCP_STATUS_ERROR;
    }
  }

  if (Obj->WifiCtx.ScanResults.Count > 0)
  {
    memset(Obj->WifiCtx.ScanResults.AP, 0, sizeof(RCP_WiFi_AP_t)* RCP_WIFI_MAX_DETECTED_AP);
    Obj->WifiCtx.ScanResults.Count = 0;
  }
  Obj->WifiCtx.ScanResults.More = 0;

  snprintf(MAC, 20, "\"" MACSTR "\"", MAC2STR(ScanOptions.MAC));
  if (ScanOptions.SSID[0] == '\0')
  {
    snprintf(SSID, RCP_WIFI_MAX_SSID_SIZE + 3, "%s", ScanOptions.SSID);
  }
  else
  {
    snprintf(SSID, RCP_WIFI_MAX_SSID_SIZE + 3, "\"%s\"", ScanOptions.SSID);
  }

  snprintf((char *)cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWLAP=%" PRIu32 ",%s,%s,%" PRIu16 "\r\n",
           (uint32_t)ScanOptions.scan_type, SSID,
           strcmp(MAC, "\"00:00:00:00:00:00\"") == 0 ? "" : MAC, ScanOptions.Channel);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_SetReconnectionOpts(RCP_Object_t *Obj, RCP_WiFi_Connect_Opts_t *ConnectOpts)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(ConnectOpts);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWRECONNCFG=%" PRIu16 ",%" PRIu16 "\r\n",
           ConnectOpts->Reconnection_interval, ConnectOpts->Reconnection_nb_attempts);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_Connect(RCP_Object_t *Obj, RCP_WiFi_Connect_Opts_t *ConnectOpts)
{
  RCP_Status_t ret = RCP_STATUS_ERROR;
  uint32_t pos = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(ConnectOpts);

  if (ConnectOpts->WPS)
  {
    return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+WPS=1\r\n", RCP_WIFI_TIMEOUT);
  }

  if (ConnectOpts->SSID[0] == '\0')
  {
    WIFI_LOG_ERROR("SSID cannot be NULL\n");
    return ret;
  }

  /* The config of the connect options must be made before every connect */
  if (ConnectOpts->Reconnection_interval > RCP_WIFI_RECONNECTION_INTERVAL)
  {
    WIFI_LOG_ERROR("Reconnection interval must be between [0;7200], parameter set to default value : 0\n");
    ConnectOpts->Reconnection_interval = 0;
  }
  if (ConnectOpts->Reconnection_nb_attempts > RCP_WIFI_RECONNECTION_ATTEMPTS)
  {
    WIFI_LOG_ERROR("Number of reconnection attempts must be between [0;1000], parameter set to default value : 0\n");
    ConnectOpts->Reconnection_nb_attempts = 0;
  }

  /* Setup the SSID and Password as required parameters */
  pos += snprintf((char *)cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWJAP=\"%s\",\"%s\",",
                  ConnectOpts->SSID, ConnectOpts->Password);

  /* Add optional BSSID parameters if defined */
  if (ConnectOpts->MAC[0] != '\0')
  {
    pos += snprintf((char *)&cmd[pos], RCP_CMDRSP_STRING_SIZE - pos,
                    "\"" MACSTR "\"", MAC2STR(ConnectOpts->MAC));
  }

  /* Add optional WEP mode */
  if (!((ConnectOpts->WEP == 0) || (ConnectOpts->WEP == 1)))
  {
    WIFI_LOG_ERROR("WEP value is out of range [0;1]\n");
    return ret;
  }
  if (ConnectOpts->PMF > 2)
  {
    WIFI_LOG_ERROR("PMF value is out of range [0;2]\n");
    return ret;
  }

  snprintf((char *)&cmd[pos], RCP_CMDRSP_STRING_SIZE - pos, ",%" PRIu32 ",,,,%" PRIu32 "\r\n",
           ConnectOpts->WEP, ConnectOpts->PMF);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_CONNECT_TIMEOUT);

  if ((ret == RCP_STATUS_OK) && (RCP_WiFi_SetReconnectionOpts(Obj, ConnectOpts) != RCP_STATUS_OK))
  {
    WIFI_LOG_ERROR("Connection configuration command issued\n");
    return RCP_STATUS_ERROR;
  }

  return ret;
}

RCP_Status_t RCP_WiFi_GetConnectInfo(RCP_Object_t *Obj, int32_t *Rssi, RCP_WiFi_SecurityType_e *Security,
                                     RCP_WiFi_Protocol_e *Protocol)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  uint8_t index = 0;

  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Rssi);
  RCP_NULL_ASSERT(Security);
  RCP_NULL_ASSERT(Protocol);

  RCP_Status_t ret;
  RCP_WiFi_StaStateType_e state;

  strncpy(cmd, "AT+CWJAP?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWJAP:", &argc, argv, RCP_WIFI_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 4)
  {
    return RCP_STATUS_ERROR;
  }
  /* Parse the response */
  if (argc >= 5)
  {
    RCP_RemoveStrQuotes(argv[index]);
    strncpy((char *)Obj->WifiCtx.SSID, argv[index], RCP_WIFI_MAX_SSID_SIZE);
    Obj->WifiCtx.SSID[RCP_WIFI_MAX_SSID_SIZE] = '\0'; /* Ensure null termination */
    index++;
  }

  RCP_RemoveStrQuotes(argv[index]);
  Parser_StrToMAC(argv[index++], Obj->WifiCtx.APSettings.MAC_Addr);
  Obj->WifiCtx.STASettings.Channel = atoi(argv[index++]);
  *Rssi = atoi(argv[index++]);

  if (RCP_SdkMinVersion(Obj, 2, 0, 97) == RCP_STATUS_OK)
  {
    /* Current device returns +CWSTATE:<state>,"<ssid>",<mode>,<auth>. */
    strncpy(cmd, "AT+CWSTATE?\r\n", sizeof(cmd));
    ret = RCP_Common_Query_Parse(Obj, cmd, "+CWSTATE:", &argc, argv, RCP_WIFI_TIMEOUT);
    if (ret != RCP_STATUS_OK)
    {
      return ret;
    }
    if (argc < 4)
    {
      return RCP_STATUS_ERROR;
    }
    state = (RCP_WiFi_StaStateType_e)atoi(argv[0]);
    if ((state == RCP_WIFI_STATE_STA_CONNECTED) || (state == RCP_WIFI_STATE_STA_GOT_IP))
    {
      int32_t protocol = atoi(argv[2]);

      *Security = (RCP_WiFi_SecurityType_e)atoi(argv[3]);
      switch (protocol)
      {
        case 15:
          *Protocol = RCP_WIFI_PROTOCOL_11AX;
          break;
        case 7:
          *Protocol = RCP_WIFI_PROTOCOL_11N;
          break;
        case 3:
          *Protocol = RCP_WIFI_PROTOCOL_11G;
          break;
        case 1:
          *Protocol = RCP_WIFI_PROTOCOL_11B;
          break;
        default:
          *Protocol = RCP_WIFI_PROTOCOL_UNKNOWN;
          break;
      }
    }
    else
    {
      *Security = RCP_WIFI_SECURITY_UNKNOWN;
      *Protocol = RCP_WIFI_PROTOCOL_UNKNOWN;
    }
  }
  return ret;
}

RCP_Status_t RCP_WiFi_Disconnect(RCP_Object_t *Obj, uint32_t restore)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWQAP=%" PRIu32 "\r\n", restore);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);
}

RCP_Status_t RCP_WiFi_Station_GetMACAddress(RCP_Object_t *Obj, uint8_t *Mac)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Mac);

  strncpy(cmd, "AT+CIPSTAMAC?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CIPSTAMAC:", &argc, argv, RCP_WIFI_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  RCP_RemoveStrQuotes(argv[0]);
  Parser_StrToMAC(argv[0], Mac);

  return ret;
}

RCP_Status_t RCP_WiFi_GetCountryCode(RCP_Object_t *Obj, uint32_t *Policy, char *CountryString)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Policy);
  RCP_NULL_ASSERT(CountryString);

  CountryString[0] = '\0';
  strncpy(cmd, "AT+CWCOUNTRY?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWCOUNTRY:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 2)
  {
    return RCP_STATUS_ERROR;
  }

  *Policy = atoi(argv[0]);
  RCP_RemoveStrQuotes(argv[1]);
  strncpy(CountryString, argv[1], 3);

  return ret;
}

RCP_Status_t RCP_WiFi_SetCountryCode(RCP_Object_t *Obj, uint32_t *Policy, char *CountryString)
{
  int32_t code = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Policy);
  RCP_NULL_ASSERT(CountryString);

  for (code = 0; code < RCP_WIFI_COUNTRY_CODE_MAX; code++)
  {
    if (strcasecmp(CountryString, Country_code_str[code]) == 0)
    {
      break;
    }
  }
  if (code >= RCP_WIFI_COUNTRY_CODE_MAX)
  {
    WIFI_LOG_ERROR("Incorrect country code string\n");
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWCOUNTRY=%" PRIu32 ",\"%s\"\r\n", *Policy, CountryString);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);
}

RCP_Status_t RCP_Net_SetDhcpConfig(RCP_Object_t *Obj, RCP_Net_DhcpType_e *State, uint32_t *Operate)
{
  RCP_Status_t ret = RCP_STATUS_ERROR;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(State);
  RCP_NULL_ASSERT(Operate);

  if (!((*Operate == 0U) || (*Operate == 1U)) || (*State > RCP_NET_DHCP_STA_AP_ENABLED))
  {
    NET_LOG_ERROR("Incorrect DHCP parameters\n");
    return ret;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWDHCP=%" PRIu32 ",%" PRIu32 "\r\n",
           *Operate, (uint32_t)*State);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);

  if (ret == RCP_STATUS_OK)
  {
    if (((uint32_t)*State & (uint32_t)RCP_NET_DHCP_STA_ENABLED) != 0U)
    {
      Obj->NetCtx.DHCP_STA_IsEnabled = (uint8_t)*Operate;
    }
    if (((uint32_t)*State & (uint32_t)RCP_NET_DHCP_AP_ENABLED) != 0U)
    {
      Obj->NetCtx.DHCP_AP_IsEnabled = (uint8_t)*Operate;
    }
  }

  return ret;
}

/* Soft-AP related functions definition ----------------------------------------------*/
RCP_Status_t RCP_WiFi_SetDualMode(RCP_Object_t *Obj)
{
  RCP_Status_t ret;
  uint32_t sta_state = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  /* Check if station is connected */
  if ((Obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_CONNECTED) || (Obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_GOT_IP))
  {
    sta_state = 1;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWMODE=3,%" PRIu32 "\r\n", sta_state);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);

  if (ret == RCP_STATUS_OK)
  {
    Obj->WifiCtx.ApState = RCP_WIFI_STATE_AP_RUNNING;
  }

  return ret;
}

RCP_Status_t RCP_WiFi_AP_Start(RCP_Object_t *Obj, RCP_WiFi_ApConfig_t *ApConfig)
{
  RCP_Status_t ret = RCP_STATUS_ERROR;
  int32_t tmp_rssi = 0;
  RCP_WiFi_SecurityType_e tmp_security = RCP_WIFI_SECURITY_UNKNOWN;
  RCP_WiFi_Protocol_e tmp_protocol = RCP_WIFI_PROTOCOL_UNKNOWN;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(ApConfig);

  if (ApConfig->SSID[0] == '\0')
  {
    WIFI_LOG_ERROR("SSID cannot be NULL\n");
    return ret;
  }

  if (strlen((char *)ApConfig->SSID) > RCP_WIFI_MAX_SSID_SIZE)
  {
    WIFI_LOG_ERROR("SSID is too long, maximum length is 32\n");
    return ret;
  }

  if (ApConfig->MaxConnections > RCP_WIFI_MAX_CONNECTED_STATIONS)
  {
    WIFI_LOG_WARN("Max connections number is too high, set to default value 4\n");
    ApConfig->MaxConnections = RCP_WIFI_MAX_CONNECTED_STATIONS;
  }

  /* Get the channel the station is connected to */
  if ((Obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_CONNECTED) || (Obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_GOT_IP))
  {
    if (RCP_WiFi_GetConnectInfo(Obj, &tmp_rssi, &tmp_security, &tmp_protocol) != RCP_STATUS_OK)
    {
      WIFI_LOG_ERROR("Get connection information failed\n");
      return RCP_STATUS_ERROR;
    }
    ApConfig->Channel = Obj->WifiCtx.STASettings.Channel;
    WIFI_LOG_WARN("Soft-AP channel number will align on STATION's one : %" PRIu32 "\n", ApConfig->Channel);
  }
  else if ((ApConfig->Channel < 1) || (ApConfig->Channel > 13))
  {
    WIFI_LOG_WARN("Channel value out of range, set to default value 1\n");
    ApConfig->Channel = 1;
  }

  if ((ApConfig->Security > RCP_WIFI_AP_SECURITY_WPA3_PSK) || (ApConfig->Security == RCP_WIFI_AP_SECURITY_WEP))
  {
    WIFI_LOG_ERROR("Security not supported\n");
    return ret;
  }
  else
  {
    if ((ApConfig->Security != RCP_WIFI_AP_SECURITY_OPEN) &&
        ((strlen((char *)ApConfig->Password) < 8) ||
         (strlen((char *)ApConfig->Password) > RCP_WIFI_MAX_PASSWORD_SIZE)))
    {
      WIFI_LOG_ERROR("Password length incorrect, must be in following range [8;63]\n");
      return ret;
    }

    /* Need to set the password to null if security selected is OPEN */
    if ((ApConfig->Security == RCP_WIFI_AP_SECURITY_OPEN) && (strlen((char *)ApConfig->Password) > 0))
    {
      WIFI_LOG_WARN("Password is not needed for open security, set to NULL\n");
      ApConfig->Password[0] = '\0';
    }
  }

  if (ApConfig->Hidden > 1)
  {
    WIFI_LOG_WARN("Hidden parameter is not supported, set to default value 0\n");
    ApConfig->Hidden = 0;
  }

  if (ApConfig->Protocol == RCP_WIFI_PROTOCOL_UNKNOWN)
  {
    WIFI_LOG_WARN("Protocol set to default value (802.11ax)\n");
    ApConfig->Protocol = RCP_WIFI_PROTOCOL_11AX;
  }
  if (RCP_WiFi_AP_SetMode(Obj, ApConfig->Protocol) != RCP_STATUS_OK)
  {
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWSAP=\"%s\",\"%s\",%" PRIu32 ",%" PRIu16 ",%" PRIu32 ",%" PRIu32 "\r\n",
           ApConfig->SSID, ApConfig->Password, ApConfig->Channel, ApConfig->Security,
           ApConfig->MaxConnections, ApConfig->Hidden);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);

  if (ret == RCP_STATUS_OK)
  {
    WIFI_LOG_DEBUG("Soft-AP configuration done\n");
  }

  return ret;
}

RCP_Status_t RCP_WiFi_AP_Stop(RCP_Object_t *Obj, uint8_t Reconnect)
{
  RCP_Status_t ret;
  uint32_t sta_state = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  /* Check if station is connected */
  if ((Obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_CONNECTED) || (Obj->WifiCtx.StaState == RCP_WIFI_STATE_STA_GOT_IP))
  {
    sta_state = 1;
  }

  if (Reconnect == 0)
  {
    snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWMODE=1,0\r\n");
  }
  else
  {
    snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWMODE=1,%" PRIu32 "\r\n", sta_state);
  }
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);

  if (ret == RCP_STATUS_OK)
  {
    Obj->WifiCtx.ApState = RCP_WIFI_STATE_AP_RESET;
  }

  return ret;
}

RCP_Status_t RCP_WiFi_AP_GetConfig(RCP_Object_t *Obj, RCP_WiFi_ApConfig_t *ApConfig)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(ApConfig);

  strncpy(cmd, "AT+CWSAP?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWSAP:", &argc, argv, RCP_WIFI_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 6)
  {
    return RCP_STATUS_ERROR;
  }

  RCP_RemoveStrQuotes(argv[0]);
  strncpy((char *)ApConfig->SSID, argv[0], RCP_WIFI_MAX_SSID_SIZE);
  ApConfig->SSID[RCP_WIFI_MAX_SSID_SIZE] = '\0'; /* Ensure null termination */
  RCP_RemoveStrQuotes(argv[1]);
  strncpy((char *)ApConfig->Password, argv[1], RCP_WIFI_MAX_PASSWORD_SIZE);
  ApConfig->Password[RCP_WIFI_MAX_PASSWORD_SIZE] = '\0'; /* Ensure null termination */
  ApConfig->Channel = (uint32_t)atoi(argv[2]);
  ApConfig->Security = (RCP_WiFi_ApSecurityType_e)atoi(argv[3]);
  ApConfig->MaxConnections = (uint32_t)atoi(argv[4]);
  ApConfig->Hidden = (uint32_t)atoi(argv[5]);

  if (RCP_WiFi_AP_GetMode(Obj, &ApConfig->Protocol) != RCP_STATUS_OK)
  {
    return RCP_STATUS_ERROR;
  }

  memset(ApConfig->Password, 0, RCP_WIFI_MAX_PASSWORD_SIZE + 1);
  return ret;
}

RCP_Status_t RCP_WiFi_AP_ListConnectedStations(RCP_Object_t *Obj, RCP_WiFi_Connected_Sta_t *Stations)
{
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Stations);
  RCP_Status_t ret;
  struct modem *mdm = (struct modem *) &Obj->Modem;
  struct modem_cmd_handler_data *data = (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;

  struct modem_cmd handlers[] =
  {
    MODEM_CMD("+CWLIF:", on_cmd_ap_liststa, 2U, ","),
  };

  if (data == NULL)
  {
    return RCP_STATUS_ERROR;
  }
  (void)rcp_os_mutex_lock(&data->sem_tx_lock, RCP_OS_WAIT_FOREVER);

  mdm->rx_data = Stations;
  Stations->Count = 0;

  ret = RCP_Status(modem_cmd_send_ext(&mdm->iface,
                                      &mdm->modem_cmd_handler,
                                      handlers,
                                      ARRAY_SIZE(handlers),
                                      (const uint8_t *)"AT+CWLIF\r\n",
                                      &mdm->sem_response,
                                      RCP_NCP_TIMEOUT,
                                      MODEM_NO_TX_LOCK));

  (void)rcp_os_mutex_unlock(&data->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_WiFi_AP_DisconnectStation(RCP_Object_t *Obj, uint8_t *MAC)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(MAC);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWQIF=\"" MACSTR "\"\r\n", MAC2STR(MAC));
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);
}

RCP_Status_t RCP_WiFi_AP_GetMode(RCP_Object_t *Obj, RCP_WiFi_Protocol_e *Protocol)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  uint32_t temp_protocol = 0;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Protocol);

  strncpy(cmd, "AT+CWAPPROTO?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWAPPROTO:", &argc, argv, RCP_WIFI_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  temp_protocol = (uint32_t)atoi(argv[0]);
  switch (temp_protocol)
  {
    case 15:
      *Protocol = RCP_WIFI_PROTOCOL_11AX;
      break;
    case 7:
      *Protocol = RCP_WIFI_PROTOCOL_11N;
      break;
    case 3:
      *Protocol = RCP_WIFI_PROTOCOL_11G;
      break;
    case 1:
      *Protocol = RCP_WIFI_PROTOCOL_11B;
      break;
    default:
      SYS_LOG_ERROR("Invalid Wi-Fi protocol: %d\n", temp_protocol);
      return RCP_STATUS_ERROR;
  }

  return ret;
}

RCP_Status_t RCP_WiFi_AP_SetMode(RCP_Object_t *Obj, RCP_WiFi_Protocol_e Protocol)
{
  uint32_t temp_protocol = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  switch (Protocol)
  {
    case RCP_WIFI_PROTOCOL_11AX:
      temp_protocol = 15;
      break;
    case RCP_WIFI_PROTOCOL_11N:
      temp_protocol = 7;
      break;
    case RCP_WIFI_PROTOCOL_11G:
      temp_protocol = 3;
      break;
    case RCP_WIFI_PROTOCOL_11B:
      temp_protocol = 1;
      break;
    default:
      return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWAPPROTO=%" PRIu32 "\r\n", temp_protocol);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_WIFI_TIMEOUT);
}

RCP_Status_t RCP_WiFi_AP_GetMACAddress(RCP_Object_t *Obj, uint8_t *Mac)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(Mac);

  strncpy(cmd, "AT+CIPAPMAC?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CIPAPMAC:", &argc, argv, RCP_WIFI_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  RCP_RemoveStrQuotes(argv[0]);
  Parser_StrToMAC(argv[0], Mac);

  return ret;
}

RCP_Status_t RCP_WiFi_SetPowerMode(RCP_Object_t *Obj, uint32_t ps_mode,
                              uint32_t hbn_level) {
  RCP_Status_t ret;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

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

RCP_Status_t RCP_WiFi_GetPowerMode(RCP_Object_t *Obj, uint32_t *ps_mode) {
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(ps_mode);

  *ps_mode = Obj->LowPowerCfg.PSMode;
  return RCP_STATUS_OK;
}

RCP_Status_t RCP_WiFi_SetWakeUpPin(RCP_Object_t *Obj, uint32_t wakeup_pin, uint32_t mode)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SLWKIO=%" PRIu32 ",%" PRIu32 "\r\n", wakeup_pin, mode);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_SetClockSource(RCP_Object_t *Obj, uint32_t source) {
  RCP_Status_t ret = RCP_STATUS_ERROR;
  uint32_t current_source;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if ((source == 0U) || (source > 3U)) {
    return RCP_STATUS_ERROR;
  }

  ret = RCP_WiFi_GetClockSource(Obj, &current_source);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }

  if (current_source == source) {
    return RCP_STATUS_OK;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SET_CLOCK=%" PRIu32 "\r\n", source);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, 0U);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("Failed to set clock source");
    return ret;
  }

  ret = RCP_WaitForReady(Obj, RCP_READY_DEFAULT_TIMEOUT_MS);
  if (ret != RCP_STATUS_OK) {
    SYS_LOG_ERROR("sem_if_ready not received\n");
  }
  return ret;
}

RCP_Status_t RCP_WiFi_GetClockSource(RCP_Object_t *Obj, uint32_t *source) {
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(source);

  strncpy(cmd, "AT+GET_CLOCK\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+GET_CLOCK:", &argc, argv,
                               RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    return ret;
  }
  if (argc < 1U) {
    return RCP_STATUS_ERROR;
  }

  *source = (uint32_t)atoi(argv[0]);
  return ret;
}

RCP_Status_t RCP_WiFi_SetDTIM(RCP_Object_t *Obj, uint32_t dtim)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  uint32_t max_dtim = 10;
  RCP_NULL_ASSERT(Obj);

  if (RCP_SdkMinVersion(Obj, 2, 0, 97) != RCP_STATUS_OK)
  {
    if (dtim == 0)
    {
      WIFI_LOG_ERROR("DTIM value should be greater than 0\n");
      return RCP_STATUS_ERROR;
    }
    max_dtim = 25;
  }

  /* Verify the DTIM to configure does not exceed the maximum value */
  if (dtim > max_dtim)
  {
    WIFI_LOG_ERROR("DTIM value exceeds maximum value : %" PRIu32 "\n", max_dtim);
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SLWKDTIM=%" PRIu32 "\r\n", dtim);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_ClearDTIM(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+SLCLDTIM\r\n", RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_GetDTIM_AP(RCP_Object_t *Obj, uint32_t *dtim)
{
  RCP_Status_t ret;
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_NULL_ASSERT(Obj);

  strncpy(cmd, "AT+GET_AP_DTIM?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+DTIM:", &argc, argv, RCP_WIFI_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *dtim = (uint32_t)atoi(argv[0]);
  return RCP_STATUS_OK;
}

RCP_Status_t RCP_WiFi_DeleteWakeUpPin(RCP_Object_t *Obj, uint32_t wakeup_pin)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+DEWKIO=%" PRIu32 "\r\n", wakeup_pin);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_SetWakeUpTimer(RCP_Object_t *Obj, uint32_t mode, uint32_t timeout_ms)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if ((mode > 2U) || (timeout_ms == 0U))
  {
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+SLWKTIMER=%" PRIu32 ",%" PRIu32 "\r\n", mode, timeout_ms);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_StartKeepAlive(RCP_Object_t *Obj, uint32_t period)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if ((period == 0U) || (period > RCP_WIFI_POWER_PERIOD_MAX))
  {
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+START_KEEPALIVE=%" PRIu32 "\r\n", period);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_StopKeepAlive(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+STOP_KEEPALIVE\r\n", RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_StartArp(RCP_Object_t *Obj, uint32_t period)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if ((period == 0U) || (period > RCP_WIFI_POWER_PERIOD_MAX))
  {
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+START_ARP=%" PRIu32 "\r\n", period);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_StopArp(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+STOP_ARP\r\n", RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_TWT_Setup(RCP_Object_t *Obj, RCP_WiFi_TWT_Setup_Params_t *twt_params)
{
  RCP_Status_t ret = RCP_STATUS_ERROR;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(twt_params);

  if (twt_params->setup_type > 2)
  {
    WIFI_LOG_ERROR("TWT setup type should be between [0; 2]\n");
    goto _err;
  }
  if (twt_params->flow_type > 1)
  {
    WIFI_LOG_ERROR("TWT flow type should be between [0; 1]\n");
    goto _err;
  }
  if (twt_params->wake_int_exp > 31)
  {
    WIFI_LOG_ERROR("TWT wake interval exponent should be between [0; 31]\n");
    goto _err;
  }
  if (twt_params->min_twt_wake_dur > 0xFF)
  {
    WIFI_LOG_ERROR("TWT minimum wake duration should be between [0; 255]\n");
    goto _err;
  }
  if (twt_params->wake_int_mantissa > 0xFFFF)
  {
    WIFI_LOG_ERROR("TWT wake interval mantissa should be between [0; 65535]\n");
    goto _err;
  }

  snprintf((char *)cmd, RCP_CMDRSP_STRING_SIZE,
           "AT+TWT_PARAM=%" PRIu16 ",%" PRIu16 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\r\n",
           twt_params->setup_type, twt_params->flow_type, twt_params->wake_int_exp,
           twt_params->min_twt_wake_dur, twt_params->wake_int_mantissa);
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);

_err:
  return ret;
}

RCP_Status_t RCP_WiFi_TWT_Sleep(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+TWT_SLEEP\r\n", RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_TWT_GetStatus(RCP_Object_t *Obj, RCP_WiFi_TWT_Status_t *twt_status)
{
  struct modem *mdm = (struct modem *) &Obj->Modem;
  struct modem_cmd_handler_data *data = (struct modem_cmd_handler_data *)mdm->modem_cmd_handler.cmd_handler_data;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(twt_status);

  struct modem_cmd handlers[] =
  {
    MODEM_CMD_ARGS_MAX("+TWT:", on_cmd_twt_status, 1U, 7U, ":,"),
  };

  if (data == NULL)
  {
    return RCP_STATUS_ERROR;
  }
  (void)rcp_os_mutex_lock(&data->sem_tx_lock, RCP_OS_WAIT_FOREVER);

  mdm->rx_data = twt_status;

  ret = RCP_Status(modem_cmd_send_ext(&mdm->iface,
                                      &mdm->modem_cmd_handler,
                                      handlers,
                                      ARRAY_SIZE(handlers),
                                      (const uint8_t *)"AT+TWT_STATUS?\r\n",
                                      &mdm->sem_response,
                                      RCP_NCP_TIMEOUT,
                                      MODEM_NO_TX_LOCK));

  (void)rcp_os_mutex_unlock(&data->sem_tx_lock);
  return ret;
}

RCP_Status_t RCP_WiFi_TWT_Teardown(RCP_Object_t *Obj, RCP_WiFi_TWT_Teardown_Params_t *twt_params)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(twt_params);

#if 0 /* TWT multi flow is not supported in current version */
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+TWT_TEARDOWN=0,%" PRIu16 ",%" PRIu16 "\r\n",
           twt_params->all_twt, twt_params->id);
#else
  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+TWT_TEARDOWN=0,1,0\r\n");
#endif /* TWT multi flow is not supported in current version */

  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_ClearPowerStats(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);

  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+PWR_CLEAR\r\n", RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_GetPowerStats(RCP_Object_t *Obj, uint64_t *power)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(power);

  strncpy(cmd, "AT+PWR_GET\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+AVERAGE PWR:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *power = strtoull(argv[0], NULL, 10);
  return ret;
}

RCP_Status_t RCP_WiFi_GetLPInterval(RCP_Object_t *Obj, uint32_t *interval)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(interval);

  strncpy(cmd, "AT+LP_INTERVAL_GET\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+LP INTERVAL:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *interval = (uint32_t)atoi(argv[0]);
  return ret;
}

RCP_Status_t RCP_WiFi_SetCoexDuty(RCP_Object_t *Obj, uint32_t active_ms)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWCOEXDUTY=%" PRIu32 "\r\n", active_ms);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_GetCoexDuty(RCP_Object_t *Obj, uint32_t *active_ms)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(active_ms);

  strncpy(cmd, "AT+CWCOEXDUTY?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWCOEXDUTY:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *active_ms = (uint32_t)atoi(argv[0]);
  return ret;
}

RCP_Status_t RCP_WiFi_SetCoexEnable(RCP_Object_t *Obj, uint32_t enable)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if ((enable != 0U) && (enable != 1U))
  {
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWCOEXEN=%" PRIu32 "\r\n", enable);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_GetCoexEnable(RCP_Object_t *Obj, uint32_t *enable)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(enable);

  strncpy(cmd, "AT+CWCOEXEN?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWCOEXEN:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *enable = (uint32_t)atoi(argv[0]);
  return ret;
}

RCP_Status_t RCP_WiFi_TWT_IsSupported(RCP_Object_t *Obj, uint32_t *is_supported)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(is_supported);

  strncpy(cmd, "AT+GET_TWT_SUPPORTED?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+TWT_SUPPORT:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *is_supported = (uint32_t)atoi(argv[0]);
  return ret;
}

RCP_Status_t RCP_WiFi_SetAntennaEnable(RCP_Object_t *Obj, RCP_WiFi_AntennaMode_e mode)
{
  uint32_t pin = 0;
  uint32_t dynamic_enable = 0;
  uint32_t static_enable = 0;
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_NULL_ASSERT(Obj);

  if (Obj->ModuleInfo.ModuleID.ModuleID == RCP_MODULE_ID_B)
  {
    SYS_LOG_ERROR("Antenna configuration not supported by the module\n");
    return RCP_STATUS_ERROR;
  }

  if (mode == RCP_WIFI_ANTENNA_STATIC)
  {
    static_enable = 1;
  }
  else if ((RCP_SdkMinVersion(Obj, 2, 0, 97) == RCP_STATUS_OK) && (mode == RCP_WIFI_ANTENNA_DYNAMIC))
  {
    static_enable = 1;
    dynamic_enable = 1;
  }
  else
  {
    SYS_LOG_ERROR("Invalid antenna mode\n");
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWANTENABLE=%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\r\n",
           dynamic_enable, static_enable, pin);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_GetAntennaEnable(RCP_Object_t *Obj, RCP_WiFi_AntennaMode_e *mode)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  uint32_t dynamic_enable;
  uint32_t static_enable;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);

  strncpy(cmd, "AT+CWANTENABLE?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWANTENABLE:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 2)
  {
    return RCP_STATUS_ERROR;
  }

  dynamic_enable = (uint32_t)atoi(argv[0]);
  static_enable = (uint32_t)atoi(argv[1]);
  if ((dynamic_enable == 0) && (static_enable == 0))
  {
    *mode = RCP_WIFI_ANTENNA_DISABLED;
  }
  else if (dynamic_enable == 1)
  {
    *mode = RCP_WIFI_ANTENNA_DYNAMIC;
  }
  else if ((dynamic_enable == 0) && (static_enable == 1))
  {
    *mode = RCP_WIFI_ANTENNA_STATIC;
  }
  else
  {
    *mode = RCP_WIFI_ANTENNA_UNKNOWN;
  }
  /* Unused: pin = (uint32_t)atoi(argv[2]); */
  return ret;
}

RCP_Status_t RCP_WiFi_GetAntennaUsed(RCP_Object_t *Obj, uint32_t *antenna_id)
{
  char *argv[CONFIG_MODEM_CMD_HANDLER_MAX_PARAM_COUNT];
  char cmd[RCP_CMD_MATCH_BUFF_SIZE];
  uint16_t argc = 0;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);

  strncpy(cmd, "AT+CWANT?\r\n", sizeof(cmd));
  ret = RCP_Common_Query_Parse(Obj, cmd, "+CWANT:", &argc, argv, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK)
  {
    return ret;
  }
  if (argc < 1)
  {
    return RCP_STATUS_ERROR;
  }

  *antenna_id = (uint32_t)atoi(argv[0]);
  return ret;
}

RCP_Status_t RCP_WiFi_CSI_Start(RCP_Object_t *Obj,
                                const RCP_WiFi_CSI_Start_Params_t *params,
                                RCP_WiFi_CSI_Report_cb_t cb)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_WiFi_CSI_Report_cb_t old_cb;
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(params);

  if ((params->buf >= RCP_WIFI_CSI_MAX_BUFFERS) ||
      (params->mode > RCP_WIFI_CSI_MODE_AUTO) ||
      (params->ts_mode > RCP_WIFI_CSI_TS_FTM) ||
      (params->ready_after_isr > 1U))
  {
    return RCP_STATUS_ERROR;
  }

  old_cb = Obj->Callbacks.WiFi_csi_report_cb;
  Obj->Callbacks.WiFi_csi_report_cb = cb;

  /* Emit only the parameters that differ from the device defaults
     (mode=0, ts=2, interval=0, ready=1). As they are positional, a field
     must be emitted when itself or any field to its right is non-default. */
  {
    bool emit_ready = (params->ready_after_isr != 1U);
    bool emit_itvl = (params->interval_ms != 0U) || emit_ready;
    bool emit_ts = (params->ts_mode != RCP_WIFI_CSI_TS_PERIODIC) || emit_itvl;
    bool emit_mode = (params->mode != RCP_WIFI_CSI_MODE_LEGACY_LTF) || emit_ts;
    int len;

    len = snprintf(cmd, RCP_CMDRSP_STRING_SIZE,
                   "AT+CSIDMASTART=\"" MACSTR "\",%" PRIu16,
                   MAC2STR(params->MAC), (uint16_t)params->buf);
    if (emit_mode) {
      len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%" PRIu16,
                      (uint16_t)params->mode);
    }
    if (emit_ts) {
      len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%" PRIu16,
                      (uint16_t)params->ts_mode);
    }
    if (emit_itvl) {
      len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%" PRIu32,
                      params->interval_ms);
    }
    if (emit_ready) {
      len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%" PRIu16,
                      (uint16_t)params->ready_after_isr);
    }
    snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, "\r\n");
  }
  ret = RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
  if (ret != RCP_STATUS_OK) {
    Obj->Callbacks.WiFi_csi_report_cb = old_cb;
  }
  return ret;
}

RCP_Status_t RCP_WiFi_CSI_Stop(RCP_Object_t *Obj, int32_t buf)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  RCP_Status_t ret;
  RCP_NULL_ASSERT(Obj);

  if (buf == RCP_WIFI_CSI_STOP_ALL) {
    ret = RCP_Common_SetExecute(Obj, (uint8_t *)"AT+CSIDMASTOP\r\n", RCP_NCP_TIMEOUT);
    if (ret == RCP_STATUS_OK) {
      Obj->Callbacks.WiFi_csi_report_cb = NULL;
    }
    return ret;
  }

  if ((buf < 0) || (buf >= RCP_WIFI_CSI_MAX_BUFFERS)) {
    return RCP_STATUS_ERROR;
  }

  snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CSIDMASTOP=%" PRIi32 "\r\n", buf);
  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_Monitor_Start(RCP_Object_t *Obj,
                                    const RCP_WiFi_Monitor_Params_t *params)
{
  char cmd[RCP_CMDRSP_STRING_SIZE];
  char min_s[8] = "";
  char max_s[8] = "";
  char out_s[8] = "";
  bool out_valid;
  bool max_valid;
  bool min_valid;
  int len;
  RCP_NULL_ASSERT(Obj);
  RCP_NULL_ASSERT(params);

  /* channel is mandatory on the device side and must be >= 1 */
  if (params->channel == 0U) {
    return RCP_STATUS_ERROR;
  }
  if ((params->output != 0U) && (params->output != 1U) &&
      (params->output != RCP_WIFI_MONITOR_OUTPUT_DEFAULT)) {
    return RCP_STATUS_ERROR;
  }

  /* An optional field must be emitted (even if empty) when a later field is
     present, otherwise positional parameters would shift on the device. */
  out_valid = (params->output != RCP_WIFI_MONITOR_OUTPUT_DEFAULT);
  max_valid = (params->max_pkg_len != 0U) || out_valid;
  min_valid = (params->min_pkg_len != 0U) || max_valid;

  if (min_valid && (params->min_pkg_len != 0U)) {
    snprintf(min_s, sizeof(min_s), "%" PRIu16, params->min_pkg_len);
  }
  if (max_valid && (params->max_pkg_len != 0U)) {
    snprintf(max_s, sizeof(max_s), "%" PRIu16, params->max_pkg_len);
  }
  if (out_valid) {
    snprintf(out_s, sizeof(out_s), "%" PRIu16, (uint16_t)params->output);
  }

  len = snprintf(cmd, RCP_CMDRSP_STRING_SIZE, "AT+CWMONITOR=1,%" PRIu16,
                 (uint16_t)params->channel);
  if (min_valid) {
    len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%s", min_s);
  }
  if (max_valid) {
    len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%s", max_s);
  }
  if (out_valid) {
    len += snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, ",%s", out_s);
  }
  snprintf(cmd + len, RCP_CMDRSP_STRING_SIZE - len, "\r\n");

  return RCP_Common_SetExecute(Obj, (uint8_t *)cmd, RCP_NCP_TIMEOUT);
}

RCP_Status_t RCP_WiFi_Monitor_Stop(RCP_Object_t *Obj)
{
  RCP_NULL_ASSERT(Obj);
  return RCP_Common_SetExecute(Obj, (uint8_t *)"AT+CWMONITOR=0\r\n",
                               RCP_NCP_TIMEOUT);
}

/* Private Functions Definition ----------------------------------------------*/
MODEM_CMD_DEFINE(on_cmd_ap_liststa)
{
  struct modem *mdm = (struct modem *) data->user_data;
  RCP_WiFi_Connected_Sta_t *stations = (RCP_WiFi_Connected_Sta_t *)mdm->rx_data;

  if ((argc >= 2) && (stations->Count < RCP_WIFI_MAX_CONNECTED_STATIONS))
  {
    uint8_t count = stations->Count++;
    Parser_StrToIP((char *)argv[0], stations->STA[count].IP);
    Parser_StrToMAC((char *)argv[1], stations->STA[count].MAC);
  }

  return 0;
}

MODEM_CMD_DEFINE(on_cmd_twt_status)
{
  struct modem *mdm = (struct modem *) data->user_data;
  RCP_WiFi_TWT_Status_t *twt_status = (RCP_WiFi_TWT_Status_t *)mdm->rx_data;

  if ((argc >= 1) && (strcmp((char *)argv[0], "INACTIVE") == 0))
  {
    twt_status->flow_count = 0;
    return 0;
  }
  else if ((argc >= 7) && (strcmp((char *)argv[0], "ACTIVE") == 0))
  {
    uint32_t flow_index = atoi((char *)argv[1]);
    if (flow_index < RCP_WIFI_MAX_TWT_FLOWS)
    {
      twt_status->flow[flow_index].flow_type = atoi((char *)argv[2]);
      twt_status->flow[flow_index].wake_int_exp = atoi((char *)argv[4]);
      twt_status->flow[flow_index].min_twt_wake_dur = atoi((char *)argv[5]);
      twt_status->flow[flow_index].wake_int_mantissa = atoi((char *)argv[6]);
      if (twt_status->flow_count < RCP_WIFI_MAX_TWT_FLOWS)
      {
        twt_status->flow_count++;
      }
    }
  }
  else
  {
    WIFI_LOG_ERROR("Invalid TWT status response\n");
  }
  return 0;
}

static void RCP_WiFi_AT_Event(void *hObj, uint16_t *argc, char **argv)
{
  RCP_Object_t *Obj = (RCP_Object_t *)hObj;
  RCP_WiFi_CbParamData_t cb_param_wifi_data;
  if ((Obj == NULL) || (*argc < 1))
  {
    return;
  }

  if (strcmp(argv[0], "SCAN_DONE") == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(RCP_WIFI_EVT_SCAN_DONE_ID, NULL);
    }
    return;
  }

  if (strcmp(argv[0], "CONNECTED") == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(RCP_WIFI_EVT_CONNECTED_ID, NULL);
    }
    return;
  }

  if (strcmp(argv[0], "GOTIP") == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(RCP_WIFI_EVT_GOT_IP_ID, NULL);
    }
    return;
  }

  if (strcmp(argv[0], "DISCONNECTED") == 0)
  {
    /* Reset the context structure with all the information relative to the previous connection */
    memset(&Obj->NetCtx.Net_sta_info, 0, sizeof(Obj->NetCtx.Net_sta_info));
    memset(&Obj->WifiCtx.SSID, 0, sizeof(Obj->WifiCtx.SSID));
    memset(&Obj->WifiCtx.STASettings, 0, sizeof(Obj->WifiCtx.STASettings));
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(RCP_WIFI_EVT_DISCONNECTED_ID, NULL);
    }
    return;
  }

  if (strcmp(argv[0], "CONNECTING") == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(RCP_WIFI_EVT_CONNECTING_ID, NULL);
    }
    return;
  }

  if (strcmp(argv[0], "ERROR") == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb == NULL)
    {
      return;
    }
    int32_t reason_code = atoi(argv[1]);
    Obj->ulcbs.UL_wifi_sta_cb(RCP_WIFI_EVT_REASON_ID, &reason_code);
    return;
  }

  if ((strcmp(argv[0], "STA_CONNECTED") == 0) && (*argc >= 2))
  {
    RCP_RemoveStrQuotes(argv[1]);

    /* Parse the MAC address */
    Parser_StrToMAC(argv[1], cb_param_wifi_data.MAC);

    /* Check the MAC address validity */
    if (Parser_CheckValidAddress(cb_param_wifi_data.MAC, 6) != 0)
    {
      WIFI_LOG_ERROR("MAC address is not valid\n");
      return;
    }

    if (Obj->ulcbs.UL_wifi_ap_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_ap_cb(RCP_WIFI_EVT_STA_CONNECTED_ID, &cb_param_wifi_data);
    }
    return;
  }

  if ((strcmp(argv[0], "STA_DISCONNECTED") == 0) && (*argc >= 2))
  {
    /* Parse the MAC address */
    RCP_RemoveStrQuotes(argv[1]);
    Parser_StrToMAC(argv[1], cb_param_wifi_data.MAC);

    /* Check the MAC address validity */
    if (Parser_CheckValidAddress(cb_param_wifi_data.MAC, 6) != 0)
    {
      WIFI_LOG_ERROR("MAC address is not valid\n");
      return;
    }

    if (Obj->ulcbs.UL_wifi_ap_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_ap_cb(RCP_WIFI_EVT_STA_DISCONNECTED_ID, &cb_param_wifi_data);
    }
    return;
  }

  if ((strcmp(argv[0], "DIST_STA_IP") == 0) && (*argc >= 3))
  {
    /* Parse the MAC address */
    RCP_RemoveStrQuotes(argv[1]);
    Parser_StrToMAC(argv[1], cb_param_wifi_data.MAC);

    /* Check the MAC address validity */
    if (Parser_CheckValidAddress(cb_param_wifi_data.MAC, 6) != 0)
    {
      WIFI_LOG_ERROR("MAC address is not valid\n");
      return;
    }

    /* Parse the IP address */
    RCP_RemoveStrQuotes(argv[2]);
    Parser_StrToIP(argv[2], cb_param_wifi_data.IP);

    /* Check the IP address validity */
    if (Parser_CheckValidAddress(cb_param_wifi_data.IP, 4) != 0)
    {
      WIFI_LOG_ERROR("IP address is not valid\n");
      return;
    }

    if (Obj->ulcbs.UL_wifi_ap_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_ap_cb(RCP_WIFI_EVT_DIST_STA_IP_ID, &cb_param_wifi_data);
    }
    return;
  }
}

static void RCP_WiFi_scan_result(void *hObj, uint16_t *argc, char **argv)
{
  int32_t protocol;
  RCP_Object_t *Obj = (RCP_Object_t *)hObj;
  RCP_WiFi_Scan_Result_t *scan_result = &Obj->WifiCtx.ScanResults;

  if ((scan_result == NULL) || (scan_result->AP == NULL))
  {
    return;
  }

  /* Parsing of arguments for scan result with 0x */
  if (*argc >= 8)
  {
    if (scan_result->Count >= RCP_WIFI_MAX_DETECTED_AP)
    {
      if (scan_result->More == 0)
      {
        SYS_LOG_WARN("Maximum number of APs reached: %d\n", RCP_WIFI_MAX_DETECTED_AP);
        scan_result->More = 1;
      }
      return;
    }

    /* Remove first '(' */
    scan_result->AP[scan_result->Count].Security = (RCP_WiFi_SecurityType_e)atoi((char *)&argv[0][1]);
    RCP_RemoveStrQuotes((char *)argv[1]);
    strncpy((char *)scan_result->AP[scan_result->Count].SSID, (char *)argv[1], RCP_WIFI_MAX_SSID_SIZE);
    scan_result->AP[scan_result->Count].RSSI = atoi((char *)argv[2]);
    RCP_RemoveStrQuotes((char *)argv[3]);
    Parser_StrToMAC((char *)argv[3], scan_result->AP[scan_result->Count].MAC);
    scan_result->AP[scan_result->Count].Channel = atoi((char *)argv[4]);
    scan_result->AP[scan_result->Count].Pairwise_cipher = (RCP_WiFi_CipherType_e)atoi((char *)argv[5]);
    /* Rework to cast argv[6] and convert 15 in 3, 7 in 2, 2 in 1 and 1 in 0 */
    protocol = atoi((char *)argv[6]);
    switch (protocol)
    {
      case 15:
        scan_result->AP[scan_result->Count].Protocol = RCP_WIFI_PROTOCOL_11AX;
        break;
      case 7:
        scan_result->AP[scan_result->Count].Protocol = RCP_WIFI_PROTOCOL_11N;
        break;
      case 3:
        scan_result->AP[scan_result->Count].Protocol = RCP_WIFI_PROTOCOL_11G;
        break;
      case 1:
        scan_result->AP[scan_result->Count].Protocol = RCP_WIFI_PROTOCOL_11B;
        break;
      default:
        SYS_LOG_ERROR("Invalid Wi-Fi version: %d\n", protocol);
        return;
    }
    /* Remove last ')' */
    if (strlen((char *)argv[7]) < 2U)
    {
      SYS_LOG_ERROR("Invalid WPS field\n");
      return;
    }
    argv[7][1] = '\0';
    scan_result->AP[scan_result->Count].WPS = atoi((char *)argv[7]);
    scan_result->Count++;
  }
  else
  {
    SYS_LOG_ERROR("Invalid number of arguments: %d\n", *argc);
    return;
  }

  return;
}
