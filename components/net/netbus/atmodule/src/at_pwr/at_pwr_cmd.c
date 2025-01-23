/**
  ******************************************************************************
  * @file    at_http_cmd.c
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

#ifdef LP_APP
int lp_set_wakeup_by_io(uint8_t io, uint8_t mode);
int lp_delete_wakeup_by_io(uint8_t io);
void app_pm_enter_hbn(int level);
int app_lp_timer_config(int mode, uint32_t ms);

static int at_pwr_cmd_pwrmode(int argc, const char **argv)
{
    int pwr_mode;
    int pwr_valid;
    int level = 0;
    int level_valid = 0;


    AT_CMD_PARSE_NUMBER(0, &pwr_mode);
    AT_CMD_PARSE_OPT_NUMBER(1, &level, level_valid);

    if (pwr_mode  == 0) {
    } else if (pwr_mode == 1) {
        app_pm_enter_hbn(level);
    } else if (pwr_mode == 2) {
        app_pm_enter_pds15();
    } else {
        return AT_RESULT_CODE_ERROR;
    }

    return AT_RESULT_CODE_OK;
}

static int at_wakeup_timer_cmd(int argc, const char **argv)
{
    int mode;
    uint32_t timeouts_ms;

    AT_CMD_PARSE_NUMBER(0, &mode);
    AT_CMD_PARSE_NUMBER(1, &timeouts_ms);

    if (mode < 0 || mode > 2) {
        return AT_RESULT_CODE_ERROR;
    }

    app_lp_timer_config(mode, timeouts_ms);

    return AT_RESULT_CODE_OK;
}

static int at_dtim_cmd(int argc, const char **argv)
{
    int dtim;

    AT_CMD_PARSE_NUMBER(0, &dtim);

    void set_dtim_config(int dtim);
    set_dtim_config(dtim);

    return AT_RESULT_CODE_OK;
}

static int at_wkio_cmd(int argc, const char **argv)
{
    int wkio;
    int mode;

    AT_CMD_PARSE_NUMBER(0, &wkio);
    AT_CMD_PARSE_NUMBER(1, &mode);

    lp_set_wakeup_by_io(wkio, mode);

    return AT_RESULT_CODE_OK;
}

static int at_dewkio_cmd(int argc, const char **argv)
{
    int wkio;

    AT_CMD_PARSE_NUMBER(0, &wkio);

    lp_delete_wakeup_by_io(wkio);

    return AT_RESULT_CODE_OK;
}

static int at_start_keepalive_cmd(int argc, const char **argv)
{
    int period;

    AT_CMD_PARSE_NUMBER(0, &period);

    printf("at_start_keepalive_cmd period:%ld\r\n", period);
    int app_create_keepalive_timer(uint32_t periods);
    app_create_keepalive_timer(period);

    return AT_RESULT_CODE_OK;
}

static int at_stop_keepalive_cmd(int argc, const char **argv)
{
    int app_delete_keepalive_timer(void);
    app_delete_keepalive_timer();

    return AT_RESULT_CODE_OK;
}

static int at_start_arp_send_cmd(int argc, const char **argv)
{
    int period;

    AT_CMD_PARSE_NUMBER(0, &period);

    int app_pm_create_arp_announce_timer(uint32_t seconds);
    app_pm_create_arp_announce_timer(period);

    return AT_RESULT_CODE_OK;
}

static int at_stop_arp_send_cmd(int argc, const char **argv)
{
    int app_pm_delete_arp_announce_timer(void);
    app_pm_delete_arp_announce_timer();

    return AT_RESULT_CODE_OK;
}

static int at_twt_param_cmd(int argc, const char **argv)
{
    int s, t, e, n, m;

    AT_CMD_PARSE_NUMBER(0, &s);
    AT_CMD_PARSE_NUMBER(1, &t);
    AT_CMD_PARSE_NUMBER(2, &e);
    AT_CMD_PARSE_NUMBER(3, &n);
    AT_CMD_PARSE_NUMBER(4, &m);

    void app_pm_twt_param_set(int s, int t, int e, int n, int m);
    //printf("%d %d %d %d %d\r\n", s, t, e, n, m);
    app_pm_twt_param_set(s, t, e, n, m);

    return AT_RESULT_CODE_OK;
}

static int at_twt_sleep_cmd(int argc, const char **argv)
{
    void app_pm_twt_enter(void);
    app_pm_twt_enter();
    return AT_RESULT_CODE_OK;
}

static const at_cmd_struct at_pwr_cmd[] = {
    {"+PWR", NULL, NULL, at_pwr_cmd_pwrmode, NULL, 1, 3},
    {"+SLWKDTIM", NULL, NULL, at_dtim_cmd, NULL, 1, 1},
    {"+SLWKIO", NULL, NULL, at_wkio_cmd, NULL, 2, 2},
    {"+DEWKIO", NULL, NULL, at_dewkio_cmd, NULL, 1, 1},
    {"+SLWKTIMER", NULL, NULL, at_wakeup_timer_cmd, NULL, 2, 2},
    {"+START_KEEPALIVE", NULL, NULL, at_start_keepalive_cmd, NULL, 1, 1},
    {"+STOP_KEEPALIVE", NULL, NULL, NULL, at_stop_keepalive_cmd, 0, 0},
    {"+START_ARP", NULL, NULL, at_start_arp_send_cmd, NULL, 1, 1},
    {"+STOP_ARP", NULL, NULL, NULL, at_stop_arp_send_cmd, 0, 0},
    {"+TWT_PARAM", NULL, NULL, at_twt_param_cmd, NULL, 5, 5},
    {"+TWT_SLEEP", NULL, NULL, NULL, at_twt_sleep_cmd, 0, 0},
};

bool at_pwr_cmd_regist(void)
{
    if (at_cmd_register(at_pwr_cmd, sizeof(at_pwr_cmd) / sizeof(at_pwr_cmd[0])) == 0)
        return true;
    else
        return false;
}
#endif
