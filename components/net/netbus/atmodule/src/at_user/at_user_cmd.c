/**
  ******************************************************************************
  * @file    at_user_cmd.c
  * @version V1.0
  * @date
  * @brief   This file is part of AT command framework
  ******************************************************************************
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "at_main.h"
#include "at_core.h"
#ifdef CONFIG_NETHUB_SDIO_WIFI_AT_PWR
#include "wifi_mgmr_ext.h"
#endif

static int at_query_cmd_userram(int argc, const char **argv)
{
    if (argv == NULL) {
        AT_CMD_PRINTF("[AT_USER] Error: argv is NULL\r\n");
        return AT_RESULT_WITH_SUB_CODE(AT_SUB_PARA_VALUE_INVALID);
    }
    // Add further query logic here if needed
    return AT_RESULT_CODE_OK;
}

static int at_setup_cmd_userram(int argc, const char **argv)
{
    if (argv == NULL) {
        AT_CMD_PRINTF("[AT_USER] Error: argv is NULL\r\n");
        return AT_RESULT_WITH_SUB_CODE(AT_SUB_PARA_VALUE_INVALID);
    }
    // Add further setup logic here if needed
    return AT_RESULT_CODE_OK;
}

#ifdef CONFIG_NETHUB_SDIO_WIFI_AT_PWR
static int at_setup_cmd_pwr(int argc, const char **argv)
{
    int pwr_mode;
    int ret;

    AT_CMD_PARSE_NUMBER(0, &pwr_mode);

    if (pwr_mode == 0 && argc == 1) {
        ret = wifi_mgmr_sta_ps_exit();
    } else if (pwr_mode == 2 && argc == 1) {
        ret = wifi_mgmr_sta_ps_enter();
    } else {
        AT_CMD_PRINTF("[AT_USER] Error: invalid pwr_mode value %d\r\n", pwr_mode);
        return AT_RESULT_CODE_ERROR;
    }

    return (ret == 0) ? AT_RESULT_CODE_OK : AT_RESULT_CODE_ERROR;
}
#endif

static const at_cmd_struct at_user_cmd[] = {
#ifdef CONFIG_NETHUB_SDIO_WIFI_AT_PWR
    {"+PWR", NULL, NULL, at_setup_cmd_pwr, NULL, 1, 1},
#endif
    //{"+USERRAM", NULL, at_query_cmd_userram, at_setup_cmd_userram, NULL, 0, 0},
    {NULL, NULL, NULL, NULL, NULL, 0, 0},
};

bool at_user_cmd_regist(void)
{
    if (at_cmd_register(at_user_cmd, sizeof(at_user_cmd) / sizeof(at_user_cmd[0])) == 0)
        return true;
    else
        return false;
}
