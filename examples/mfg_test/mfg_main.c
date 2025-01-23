#include "mfg_main.h"

#include <FreeRTOS.h>
#include "queue.h"
#include <event_groups.h>
#include "rfparam_adapter.h"

#if defined(MFG_QCC74x_undef)
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_mfg_media.h"
#endif
#if defined(MFG_QCC743)
#include "qcc743_glb.h"
#include "qcc743_mfg_media.h"
//#include "qcc743_adc.h"
#endif
#if defined(CONFIG_BLE_MFG) || defined(CONFIG_BT_MFG)
#include "btble_lib_api.h"
#endif

#include "qcc74x_wdg.h"
#include "sdk_version.h"
#include "mfg_sdio.h"

#include "mfg_m154.h"
#include "mfg_atcmd.h"
#include "mfg_otp.h"

#include "qcc743_common.h"
#include "qcc743_pm.h"
#include "qcc74x_adc.h"
#include "qcc74x_clock.h"
#include "qcc743_glb.h"
#include "qcc743_hbn.h"

static uint8_t mfg_m154_en = 0;
struct qcc74x_device_s *adc;

static uint8_t phy_init_flag = 0;
static mfg_tx_para_t g_mfg_tx_para = {
    .tx_pwr_dbm = 17,
    .tx_duty = 50,
    .mfgmode = 0,
    .tx_mode = 0,
    .tx_mcs = 0,
    .cont_mode = 0,
    .sg_chantype = 0,
    .ax_coding_type = 0,
    .ax_gi = 0,
    .frequency = 0,
    .tx_len = 0,
    .sg_channel = 2412,
    .sg_channel_index = 1,
    .ble_is_sending = 0,
    .ble_sending_power = 0,
    .zb_sending_power = 10,
    .tx_on = 0,
    .bw = 0,
};
uint8_t mfg_cmd[MFG_CMD_MAX_LEN] = {0};
uint8_t phy_cli_cmd[20][32];
uint8_t *phy_cli_list[20]={NULL};
#ifdef MFG_GU //defined(MFG_LPGU)
// #if (PM_PDS_LDO_LEVEL_DEFAULT != 0)
// const uint32_t hbn_ucode[] = {
//     0x2000F737, 0x06375B1C, 0xE7934000, 0xDB1C0017, 0x200097B7, 0xF6934BD4, 0xCBD4C006, 0xE6934BD4,
//     0xCBD40276, 0x8ED14BD4, 0x4BD4CBD4, 0xC0000637, 0x8EF1167D, 0x5B1CCBD4, 0x00F106B7, 0x8FF516FD,
//     0xAA0A06B7, 0xDB1C8FD5, 0x200057B7, 0x063743B4, 0x8ED10040, 0x43B4C3B4, 0x00800637, 0xC3B48ED1,
//     0x10072223, 0x10072023, 0x630227B7, 0x07378782
// };
// #else
const uint32_t hbn_ucode[] = {
    0x2000F737, 0x06375B1C, 0xE7934000, 0xDB1C0017, 0x200097B7, 0xF6934BD4, 0xCBD4C006, 0xE6934BD4,
    0xCBD40276, 0x8ED14BD4, 0x4BD4CBD4, 0xC0000637, 0x8EF1167D, 0x57B7CBD4, 0x43B42000, 0x00400637,
    0xC3B48ED1, 0x063743B4, 0x8ED10080, 0x2223C3B4, 0x20231007, 0x27B71007, 0x87826302
};
// #endif
#endif

#if defined(CONFIG_BLE_MFG)
static uint8_t ble_ch = 0;
#define LE_RX_TEST_CMD_V3_OPCODE  0x204F
#define LE_RX_TEST_CMD_OPCODE     0x2033
#define LE_RX_TEST_CMD_V1_OPCODE  0x201D
#define LE_TX_TEST_CMD_OPCODE     0x207B
#define LE_TX_TEST_CMD_V3_OPCODE  0x2050
#define LE_TX_TEST_CMD_V2_OPCODE  0x2034
#define LE_TX_TEST_CMD_V1_OPCODE  0x201E
#define LE_TEST_END_CMD_OPCODE    0x201F
#endif

#if defined(CONFIG_BT_MFG)
#define BT_RX_TEST_CMD_OPCODE             0xFC70
#define BT_TX_TEST_CMD_OPCODE             0xFC71
#define BT_TEST_END_CMD_OPCODE            0xFC72
#define BT_WR_SCAN_EN_CMD_OPCODE          0x0C1A
#define HCI_EN_DUT_MODE_CMD_OPCODE        0x1803
#define HCI_ACCEPT_CON_REQ_CMD_OPCODE     0x0409
#endif

#if defined(CONFIG_BLE_MFG) || defined(CONFIG_BT_MFG)
#define HCI_RESET                  0x0C03
#define HCI_RD_BD_ADDR_CMD_OPCODE  0x1009
#endif


#define MFG_TEMP_COMPENSATION_PERIOD    5000

QueueHandle_t Message_Queue;
uint32_t g_flash_test_delay_us;
uint32_t g_flash_test_debug_on;
uint32_t g_flash_test_debug_out_num;
volatile uint8_t tcal_enable=0;
uint8_t tcal_val_fix=0;
uint8_t tcal_debug=0;
int16_t tcal_val_fix_value=0;
static uint32_t tsen_offset=0;
static uint32_t const channel_freq[] =
{
    2412,
    2417,
    2422,
    2427,
    2432,
    2437,
    2442,
    2447,
    2452,
    2457,
    2462,
    2467,
    2472,
    2484
};

static uint16_t const ble_channel_freq[] =
{
    2402,
    2404,
    2406,
    2408,
    2410,
    2412,
    2414,
    2416,
    2418,
    2420,
    2422,
    2424,
    2426,
    2428,
    2430,
    2432,
    2434,
    2436,
    2438,
    2440,
    2442,
    2444,
    2446,
    2448,
    2450,
    2452,
    2454,
    2456,
    2458,
    2460,
    2462,
    2464,
    2466,
    2468,
    2470,
    2472,
    2474,
    2476,
    2478,
    2480
};

static uint8_t ble_tx_hopping_task_en = 0;
static uint8_t ef_ctrl_wr_lock_mac = 0;

#ifndef DISABLE_MFG_AUTOBOOT
#if defined(MFG_GU_QCC74x_undef) || defined(MFG_GU_QCC743)
#define __BOOT_FLAG_SECTION __attribute__((section("BOOT_FLAG_SECTION"), used))
static const __BOOT_FLAG_SECTION uint8_t boot_flag[] = { "0mfg" };
#endif
#endif

const uint64_t rf_param[2048/8] __attribute__ ((section(".rfparam"),aligned(64))) = {0x4152415046524c42};//"..RFPARAk1bXkD6O"

#if defined(MFG_CCI_QCC743)
#define __CCI_RAM __attribute__ ((section("CCI_RAM")))
cli_ate_ret_t cci_ate_ret __CCI_RAM;
#endif

void cmd_phy_init_register(char *buf, int len, int argc, char **argv);

static int32_t mfg_cmd_shake_hand(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_tx_toggle(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_unicast(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_rx_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_frequency_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_mode_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_test_mode_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11b_longpre_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11b_shortpre_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11g_longpre_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11g_shortpre_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11n_shortgi_rate_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11n_longgi_rate_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_11ax_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_channel_switch(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_power_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_len_ctrl(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_get_rf_param(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_set_cap_code(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_set_tx_duty(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_pmem(char *buf, int len );
static int32_t mfg_cmd_mmem(char *buf, int len);
static int32_t mfg_cmd_get_info(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_le_test(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_io(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_reaload_cal_data(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_write_efuse(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_load_efuse(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_read_efuse(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_save_efuse(uint8_t *data, uint16_t len);
#if defined(MFG_GU_QCC743) || defined(MFG_GU_QCC74x_undef)
static int32_t mfg_cmd_hbn(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_sleep(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_wakeup(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_para_set(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_flash_test(uint8_t *data, uint16_t len);
static int32_t mfg_cmd_flash_test_set_param(uint8_t *data, uint16_t len);
#endif

#if defined(CONFIG_BLE_MFG)
static int32_t mfg_cmd_le_test(uint8_t *data, uint16_t len);
#endif

#if defined(CONFIG_BT_MFG)
static int32_t mfg_cmd_bt_test(uint8_t *data, uint16_t len);
#endif

#if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
static int32_t mfg_cmd_btble_hci_test(uint8_t *data, uint16_t len);
#endif

static int32_t mfg_cmd_cfo_correct(uint8_t *data, uint16_t len);

#if defined(MFG_PSRAM_TEST)
static int32_t mfg_cmd_psram_test(uint8_t *data, uint16_t len);
#endif

static const mfg_cmd_t mfg_cmds_handler[]={
    {'H',mfg_cmd_shake_hand},
    {'T',mfg_cmd_tx_toggle},
    {'t',mfg_cmd_tx_toggle},
    {'U',mfg_cmd_unicast},
    {'u',mfg_cmd_unicast},
    {'r',mfg_cmd_rx_ctrl},
    {'f',mfg_cmd_frequency_ctrl},
    {'F',mfg_cmd_frequency_ctrl},
    {'N',mfg_cmd_11n_shortgi_rate_ctrl},
    {'n',mfg_cmd_11n_longgi_rate_ctrl},
    {'m',mfg_cmd_mode_ctrl},
    {'M',mfg_cmd_test_mode_ctrl},
    {'G',mfg_cmd_11g_longpre_ctrl},
    {'g',mfg_cmd_11g_shortpre_ctrl},
    {'B',mfg_cmd_11b_longpre_ctrl},
    {'b',mfg_cmd_11b_shortpre_ctrl},
    {'Q',mfg_cmd_11ax_ctrl},
    {'c',mfg_cmd_channel_switch},
    {'P',mfg_cmd_power_ctrl},
    {'p',mfg_cmd_power_ctrl},
    {'l',mfg_cmd_len_ctrl},
    {'Y',mfg_cmd_get_info},
    {'y',mfg_cmd_get_info},
    {'x',mfg_cmd_set_cap_code},
    {'X',mfg_cmd_set_cap_code},
    {'d',mfg_cmd_set_tx_duty},
    {'Z',mfg_cmd_get_rf_param},
    {'z',mfg_cmd_get_rf_param},
    {'o',mfg_cmd_mmem},
    {'k',mfg_cmd_pmem},
    {'I',mfg_cmd_io},
    {'V',mfg_cmd_reaload_cal_data},
    {'W',mfg_cmd_write_efuse},
    {'L',mfg_cmd_load_efuse},
    {'R',mfg_cmd_read_efuse},
    {'S',mfg_cmd_save_efuse},
#if defined(MFG_PSRAM_TEST)
    {'J',mfg_cmd_psram_test},
#endif


    {'q',m154_cmd_set_seq_num},

#if defined(MFG_GU)
    {'h',mfg_cmd_hbn},
    {'s',mfg_cmd_sleep},
    {'a',mfg_cmd_wakeup},
    {'A',mfg_cmd_wakeup},
    {'i',mfg_cmd_para_set},
    {'K',mfg_cmd_flash_test},
    {'O',mfg_cmd_flash_test_set_param},
    {'C',mfg_cmd_cfo_correct},
#endif

#if defined(CONFIG_BLE_MFG) && (!defined(MFG_CCI))
    {'E',mfg_cmd_le_test},
#endif
#if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
    { BT_LE_HCI_CMD, mfg_cmd_btble_hci_test },
#endif
};

static char hex2char(uint8_t hex)
{
    if(hex >= 0 && hex <= 9){
        return hex + 0x30;
    }else if(hex >= 0x0a && hex <= 0x0f){
        return hex + 87;
    }
    return 0;
}

static int32_t char2hex(char c, uint8_t *x)
{
    if (c >= '0' && c <= '9') {
        *x = c - '0';
    } else if (c >= 'a' && c <= 'f') {
        *x = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        *x = c - 'A' + 10;
    } else {
        return -1;
    }

    return 0;
}

static uint32_t str2hex(char *str)
{
    uint8_t x1,x2,x3,x4,x5,x6,x7,x8;
    char2hex(str[2],&x1);
    char2hex(str[3],&x2);
    char2hex(str[4],&x3);
    char2hex(str[5],&x4);
    char2hex(str[6],&x5);
    char2hex(str[7],&x6);
    char2hex(str[8],&x7);
    char2hex(str[9],&x8);

    return (x1 << 28) | (x2 << 24) | (x3 << 20) | (x4 << 16) | (x5 << 12) | (x6 << 8) | (x7 << 4) | (x8 << 0);
}


static int32_t mfg_cmd_pmem(char *buf, int len)
{
    int   i;
    char *addr   = NULL;
    int   nunits = 16;
    char  start_addr[11];
    char  length[5];

    /* the cmd format must be "k 0x23000000 100" */
    memset(start_addr,0,sizeof(start_addr));
    memset(length,0,sizeof(length));
    memcpy(start_addr,&buf[1],10);
    memcpy(length,&buf[12],strlen(buf)-12);

    nunits = strtol(length, NULL, 0);
    nunits = nunits > 0x400 ? 0x400 : nunits;


    //addr = (char *)strtol(start_addr, NULL, 0);
    addr = (char *)str2hex(start_addr);

    for (i = 0; i < nunits; i++) {
        if (i % 4 == 0) {
            mfg_print("0x%08x:", (unsigned int)addr);
        }
        mfg_print(" %08x", *(unsigned int *)addr);
        addr += 4;
        if (i % 4 == 3) {
            mfg_print("\r\n");
        }
    }
    mfg_print("\r\n");

    return 0;
}


static int32_t mfg_cmd_mmem(char *buf, int len)
{
    void        *addr  = NULL;
    unsigned int value = 0;
    unsigned int old_value;
    unsigned int new_value;

    char  str_addr[11];
    char  str_value[11];

    /* the cmd format must be "o 0x23000000 0x12345678" */
    memset(str_addr,0,sizeof(str_addr));
    memset(str_value,0,sizeof(str_value));
    memcpy(str_addr,&buf[1],10);
    memcpy(str_value,&buf[12],strlen(buf)-12);

    //value = strtol(str_value, NULL, 0);
    value = str2hex(str_value);

    //mfg_print("value %08x\r\n",value);

    //addr = (char *)strtol(str_addr, NULL, 0);
    addr = str2hex(str_addr);



    if (addr == NULL) {
        mfg_print("m <addr> <value> <width>\r\n"
                       "addr  : address to modify\r\n"
                       "value : new value (default is 0)\r\n"
                       "width : width of unit, 1/2/4 (default is 4)\r\n");
        return 0;
    }


    old_value                      = *(unsigned int volatile *)addr;
    *(unsigned int volatile *)addr = (unsigned int)value;
    new_value                      = *(unsigned int volatile *)addr;
    mfg_print("value on 0x%x change from 0x%x to 0x%x.\r\n", (unsigned int)addr, old_value, new_value);

    return 0;
}

#if defined(MFG_PSRAM_TEST)
static int32_t mfg_cmd_psram_test(uint8_t *data, uint16_t len)
{
    extern int mfg_psram_test(int test_num);

    if (data != NULL) {
        int num = 0;
        int i = 0;
        while(data[i] >= '0' && data[i] <= '9') {
            num = num * 10 + (data[i] - '0');
            i++;
        }
        if (num > 0 && num < 16) {
            mfg_psram_test(num);
        } else {
            mfg_print("NUM must less 15!\r\n");
        }
    } else {
        mfg_print("NUM is NULL!\r\n");
    }

    return 0;
}
#endif

static void mfg_get_gpio_list(uint8_t *str,uint8_t gpio_list[23],uint8_t *gpio_num)
{
    uint8_t i,j=0,flag=0,t=0;
    for(i=0;str[i]!='\0';i++){
        if(str[i]>='0'&&str[i]<='9'&&flag==0){
            flag=1;
            t=(int)str[i]-'0';
        }
        else if(str[i]>='0'&&str[i]<='9'&&flag!=0){
          t=t*10+(int)str[i]-'0';
        }
        else if(flag!=0){
            gpio_list[j]=t;
            j++;
            flag=0;
        }
   }
   if(str[i-1]>='0'&&str[i-1]<='9'){
        gpio_list[j]=t;
        j++;
    }
    *gpio_num = j;
}

static int mfg_do_io_test(uint8_t *gpio_list,uint8_t *gpio_list2,int gpio_num)
{
    GLB_GPIO_Cfg_Type gpio_cfg;
    uint8_t i;
#if 0
    gpio_cfg.drive = 0;
    gpio_cfg.gpioFun = GPIO_FUN_GPIO;
    gpio_cfg.gpioMode = GPIO_MODE_INPUT;
    gpio_cfg.outputMode = 0;
    gpio_cfg.pullType = GPIO_PULL_DOWN;
    gpio_cfg.smtCtrl = 1;

    /* set group 2 input */
    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_INPUT;
        gpio_cfg.gpioPin = gpio_list2[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        mfg_print("%d set input\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Init(&gpio_cfg);
    }

    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpioPin = gpio_list[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        GLB_GPIO_Init(&gpio_cfg);
        mfg_print("%d set high\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Write(gpio_cfg.gpioPin, 1);
    }
    arch_delay_us(100);

    for(i = 0; i < gpio_num; i++){
        if(1!=GLB_GPIO_Read(gpio_list2[i])){
            mfg_print("gpio%d should get high\r\n", gpio_list2[i]);
            return -1;
        }
    }

    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpioPin = gpio_list[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        GLB_GPIO_Init(&gpio_cfg);
        mfg_print("%d set low\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Write(gpio_list[i], 0);
    }
    arch_delay_us(100);

    for(i = 0; i < gpio_num; i++){
        if(0!=GLB_GPIO_Read(gpio_list2[i])){
            mfg_print("gpio%d should get low\r\n", gpio_list2[i]);
            return -1;
        }
    }
    arch_delay_us(100);
#endif

    uint8_t j;
    gpio_cfg.drive = 0;
    gpio_cfg.gpioFun = GPIO_FUN_GPIO;
    gpio_cfg.outputMode = 0;
    gpio_cfg.pullType = GPIO_PULL_DOWN;
    gpio_cfg.smtCtrl = 1;

    /* set group 1 output */
    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpioPin = gpio_list[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        GLB_GPIO_Init(&gpio_cfg);
        mfg_print("%d set low\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Write(gpio_list[i], 0);
    }
    arch_delay_us(100);

    /* set group 2 input */
    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_INPUT;
        gpio_cfg.gpioPin = gpio_list2[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        mfg_print("%d set input\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Init(&gpio_cfg);
    }

    for(i = 0; i < gpio_num; i ++) {
        // set io high
        gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpioPin = gpio_list[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        GLB_GPIO_Init(&gpio_cfg);
        GLB_GPIO_Write(gpio_cfg.gpioPin, 1);
        mfg_print("%d set high\r\n",gpio_cfg.gpioPin);
        arch_delay_us(100);

        // check other io
        for(j = 0; j < gpio_num; j ++) {
            mfg_print("%d set input\r\n", gpio_list2[j]);
            if(i != j){
                if(1 == GLB_GPIO_Read(gpio_list2[j])) {
                    mfg_print("Possible short circuit between gpio%d and gpio%d\r\n", gpio_list[i], gpio_list[j]);
                    GLB_GPIO_Write(gpio_cfg.gpioPin, 0);
                    return -1;
                }
            } else if (i == j) {
                if (0 == GLB_GPIO_Read(gpio_list2[j])) {
                    mfg_print("set gpio%d high failed\r\n", gpio_cfg.gpioPin);
                    GLB_GPIO_Write(gpio_cfg.gpioPin, 0);
                    return -1;
                }
            }
        }

        // set io low
        GLB_GPIO_Write(gpio_cfg.gpioPin, 0);
    }

    /* set group 1 output */
    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpioPin = gpio_list[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        GLB_GPIO_Init(&gpio_cfg);
        mfg_print("%d set high\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Write(gpio_list[i], 1);
    }
    arch_delay_us(100);

    /* set group 2 input */
    for(i = 0; i < gpio_num; i++){
        gpio_cfg.gpioMode = GPIO_MODE_INPUT;
        gpio_cfg.gpioPin = gpio_list2[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        mfg_print("%d set input\r\n",gpio_cfg.gpioPin);
        GLB_GPIO_Init(&gpio_cfg);
    }

    for(i = 0; i < gpio_num; i ++) {
        // set io low
        gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
        gpio_cfg.gpioPin = gpio_list[i];
        if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
            gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
        }
        GLB_GPIO_Init(&gpio_cfg);
        GLB_GPIO_Write(gpio_cfg.gpioPin, 0);
        mfg_print("%d set low\r\n",gpio_cfg.gpioPin);
        arch_delay_us(100);

        // check other io
        for(j = 0; j < gpio_num; j ++) {
            mfg_print("%d set input\r\n", gpio_list2[j]);
            if(i != j){
                if(0 == GLB_GPIO_Read(gpio_list2[j])) {
                    mfg_print("Possible short circuit between gpio%d and gpio%d\r\n", gpio_list[i], gpio_list[j]);
                    GLB_GPIO_Write(gpio_cfg.gpioPin, 1);
                    return -1;
                }
            } else if (i == j) {
                if (1 == GLB_GPIO_Read(gpio_list2[j])) {
                    mfg_print("set gpio%d low failed\r\n", gpio_cfg.gpioPin);
                    GLB_GPIO_Write(gpio_cfg.gpioPin, 1);
                    return -1;
                }
            }
        }

        // set io low
        GLB_GPIO_Write(gpio_cfg.gpioPin, 1);
    }

    return 0;
}

static int32_t mfg_cmd_io(uint8_t *data, uint16_t len)
{
    uint8_t gpio_list[GLB_GPIO_PIN_MAX];
    uint8_t gpio_list2[GLB_GPIO_PIN_MAX];
    uint8_t gpio_num=0;
    uint8_t gpio_num2=0;

    if (data[0] == 'O') {
        if (data[1] == 'S') {
            mfg_get_gpio_list(&data[2],gpio_list,&gpio_num);
            GLB_GPIO_Cfg_Type gpio_cfg;
            if(gpio_num){
                for(uint8_t i = 0; i < gpio_num; i++){
                    gpio_cfg.drive = 0;
                    gpio_cfg.gpioFun = GPIO_FUN_GPIO;
                    gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
                    gpio_cfg.gpioPin = gpio_list[i];
                    if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
                        gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
                    }
                    gpio_cfg.outputMode = 0;
                    gpio_cfg.pullType = GPIO_PULL_NONE;
                    gpio_cfg.smtCtrl = 1;
                    GLB_GPIO_Init(&gpio_cfg);
                    GLB_GPIO_Write(gpio_cfg.gpioPin, 1);
                    mfg_print("gpio%d set high\r\n", gpio_cfg.gpioPin);
                }
            }
        } else if (data[1] == 'C') {
            mfg_get_gpio_list(&data[2],gpio_list,&gpio_num);
            GLB_GPIO_Cfg_Type gpio_cfg;
            if(gpio_num){
                for(uint8_t i = 0; i < gpio_num; i++){
                    gpio_cfg.drive = 0;
                    gpio_cfg.gpioFun = GPIO_FUN_GPIO;
                    gpio_cfg.gpioMode = GPIO_MODE_OUTPUT;
                    gpio_cfg.gpioPin = gpio_list[i];
                    if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
                        gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
                    }
                    gpio_cfg.outputMode = 0;
                    gpio_cfg.pullType = GPIO_PULL_NONE;
                    gpio_cfg.smtCtrl = 1;
                    GLB_GPIO_Init(&gpio_cfg);
                    GLB_GPIO_Write(gpio_cfg.gpioPin, 0);
                    mfg_print("gpio%d set low\r\n", gpio_cfg.gpioPin);
                }
            }
        } else if (data[1] == 'R') {
            mfg_get_gpio_list(&data[2],gpio_list,&gpio_num);
            GLB_GPIO_Cfg_Type gpio_cfg;
            if(gpio_num){
                for(uint8_t i = 0; i < gpio_num; i++){
                    gpio_cfg.drive = 0;
                    gpio_cfg.gpioFun = GPIO_FUN_GPIO;
                    gpio_cfg.gpioMode = GPIO_MODE_INPUT;
                    gpio_cfg.gpioPin = gpio_list[i];
                    if (gpio_cfg.gpioPin >= GLB_GPIO_PIN_MAX) {
                        gpio_cfg.gpioPin = GLB_GPIO_PIN_MAX - 1;
                    }
                    gpio_cfg.outputMode = 0;
                    gpio_cfg.pullType = GPIO_PULL_NONE;
                    gpio_cfg.smtCtrl = 1;
                    GLB_GPIO_Init(&gpio_cfg);
                    mfg_print("gpio%d read value is %d\r\n", (int)gpio_cfg.gpioPin, (int)GLB_GPIO_Read(gpio_cfg.gpioPin));
                }
            }
        }
        if (data[1] == 'T') {
            char *psplit=strchr((char *)&data[2],'+');
            if(psplit==NULL){
                mfg_print("No group found\r\n");
                return -1;
            }else{
                *psplit=0;
                psplit++;
                mfg_print("mfg_get_gpio_list\r\n");
                mfg_get_gpio_list(&data[2],gpio_list,&gpio_num);
                mfg_get_gpio_list((uint8_t *)psplit,gpio_list2,&gpio_num2);
                if(gpio_num!=gpio_num2){
                    mfg_print("Group not match\r\n");
                    return -1;
                }
                if(gpio_num==0){
                    mfg_print("Group must great than 0\r\n");
                    return -1;
                }
                mfg_print("\r\nGroup1:");
                for(uint8_t i = 0; i < gpio_num; i++){
                    mfg_print("%d ",gpio_list[i]);
                }
                mfg_print("\r\nGroup2:");
                for(uint8_t i = 0; i < gpio_num; i++){
                    mfg_print("%d ",gpio_list2[i]);
                }
                mfg_print("\r\nmfg_do_io_test\r\n");
                if(0!=mfg_do_io_test(gpio_list,gpio_list2,gpio_num)){
                    mfg_print("IO Test Fail\r\n");
                    return -1;
                }
                if(0!=mfg_do_io_test(gpio_list2,gpio_list,gpio_num)){
                    mfg_print("IO Test Fail\r\n");
                    return -1;
                }
                mfg_print("IO Test Pass\r\n");
            }
        }
    }
    return 0;
}

static void mfg_dump_data(uint8_t *buf, uint32_t size)
{
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0)
            mfg_print("\r\n");
        mfg_print("%02x ", buf[i]);
    }
    mfg_print("\r\n");
}

static void flash_test_task_entry(void *pvParameters)
{
    uint8_t data[4096];
    while(1){
        qcc74x_flash_read(0,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x0000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 1,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x1000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 2,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x2000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 3,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x3000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 4,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x4000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 5,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x5000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 6,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x6000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 7,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x7000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 8,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x8000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 9,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0x9000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 10,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0xA000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 11,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0xB000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 12,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0xC000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 13,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0xD000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 14,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0xE000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        qcc74x_flash_read(4096 * 15,data,sizeof(data));
        if(g_flash_test_debug_on){
            mfg_print("addr 0xF000:\r\n");
            if(g_flash_test_debug_out_num > 4096){
                g_flash_test_debug_out_num = 4096;
            }
            mfg_dump_data(data, g_flash_test_debug_out_num);
        }
        if(g_flash_test_delay_us)
            arch_delay_us(g_flash_test_delay_us);
        //mfg_print("flash test task running\r\n");
        //vTaskDelay(1000);
    }
}

static int32_t mfg_cmd_flash_test(uint8_t *data, uint16_t len)
{
    static mfg_thread_state_t thread_create_flag = MFG_THREAD_NOT_CREATE;
    static TaskHandle_t handle;

    if(data[0] == '0'){
        if(MFG_THREAD_RUNNING == thread_create_flag){
            vTaskSuspend(handle);
            thread_create_flag = MFG_THREAD_SUSPEND;
            mfg_print("suspend flash test thread ok\r\n");
        }else if(MFG_THREAD_SUSPEND == thread_create_flag){
            mfg_print("mfg flash tset thread already suspend!\r\n");
        }else if(MFG_THREAD_NOT_CREATE == thread_create_flag){
            mfg_print("mfg flash test thread not create yet!\r\n");
        }
    }else if(data[0] == '1'){
        g_flash_test_delay_us = atoi((char *)&data[1]);
        mfg_print("flash test delay %d us\r\n",(int)g_flash_test_delay_us);
        if(MFG_THREAD_NOT_CREATE == thread_create_flag){
            vTaskEnterCritical();
            xTaskCreate(flash_test_task_entry, (char*)"flash_tset", 512, NULL, 4 ,&handle);
            thread_create_flag = MFG_THREAD_RUNNING;
            mfg_print("mfg flash test thread create suss\r\n");
            vTaskExitCritical();
        }else if(MFG_THREAD_SUSPEND == thread_create_flag){
            vTaskResume(handle);
            thread_create_flag = MFG_THREAD_RUNNING;
            mfg_print("mfg flash test thread running\r\n");
        }else if(MFG_THREAD_RUNNING == thread_create_flag){
            mfg_print("mfg flash test already running!\r\n");
        }
    }

    return 0;
}

static int32_t mfg_cmd_flash_test_set_param(uint8_t *data, uint16_t len)
{
    if(data[0] == '1'){
        g_flash_test_debug_on = 1;
    }else if(data[0] == '0'){
        g_flash_test_debug_on = 0;
    }

    g_flash_test_debug_out_num = atoi((char *)&data[1]);

    mfg_print("flash test debug on %d, out num %d\r\n",g_flash_test_debug_on,g_flash_test_debug_out_num);

    return 0;
}

static int32_t mfg_cmd_get_rf_param(uint8_t *data, uint16_t len)
{
    return 0;
}
uint16_t mfg_get_channel(void)
{

    return g_mfg_tx_para.sg_channel;
}

uint8_t mfg_get_chantype(void)
{
    return 0;
}

void mfg_update_tx_power_value_only(int8_t dbm)
{

}

int8_t mfg_get_tx_power(void)
{
    return g_mfg_tx_para.tx_pwr_dbm;
}

void mfg_set_channel(uint8_t chantype, uint16_t channel)
{

}

void mfg_update_tx_power(int8_t power_dbm)
{

}


void mfg_auto_update_tx_power(void)
{

}

uint16_t mfg_get_power_value(uint8_t power)
{
    return 0;
}


uint16_t get_power_value(uint8_t power)
{
    return 0;
}

#ifdef CONFIG_XTAL_SELECT_EN

void modify_bit(uint32_t *reg_addr, uint8_t bit_position, uint8_t bit_value) {
    if (bit_value) {
        *reg_addr |= (1U << bit_position);
    } else {
        *reg_addr &= ~(1U << bit_position);
    }
}

static void xtal32k_input(void)
{
    modify_bit(0x2000F204, 19, 0x1);
    modify_bit(0x2000F204, 21, 0x1);
    modify_bit(0x2000F204, 2, 0x1);
    modify_bit(0x2000F204, 10, 0);
    modify_bit(0x2000F204, 9, 0);
    modify_bit(0x2000F204, 17, 0x0);
}


static TaskHandle_t xtal32k_select_task_hd = NULL;
static StackType_t qcc74x_mfg_xtal32k_select_stack[512];
static uint32_t xtal_reg_val = 0;
// volatile uint8_t xtal32k_ready_flag = 0;

/**********************************************************
    xtal32k check task func
 **********************************************************/
static int xtal32k_check_entry_task(int crystal_flag)
{
    uint32_t xtal32_regulator_flag = 0;

    uint64_t timeout_start;

    uint32_t retry_cnt = 0;

    uint64_t rtc_cnt, rtc_record_us, rtc_now_us;
    uint32_t rtc_cnt_low, rtc_cnt_high;
    uint64_t mtimer_record_us, mtimer_now_us;

    uint32_t rtc_us, mtimer_us;
    int32_t diff_us;

    uint32_t success_flag = 0;

    int ret = 0;

    /* TODO */
    // qcc74x_lp_set_32k_clock_ready(0);

    vTaskDelay(10);
    printf("xtal32k_check_entry task enable, freq_mtimer must be 1MHz!\r\n");

    GLB_GPIO_Cfg_Type gpioCfg = {
        .gpioPin = GLB_GPIO_PIN_0,
        .gpioFun = GPIO_FUN_ANALOG,
        .gpioMode = GPIO_MODE_ANALOG,
        .pullType = GPIO_PULL_NONE,
        .drive = 1,
        .smtCtrl = 1
    };

    if (crystal_flag) {
        gpioCfg.gpioPin = 16;
        GLB_GPIO_Init(&gpioCfg);
        printf("use passive crystal.\r\n");
    } else {
        printf("use active crystal.\r\n");
    }

    gpioCfg.gpioPin = 17;
    GLB_GPIO_Init(&gpioCfg);
    HBN_Clear_RTC_Counter();

    if (crystal_flag) {
        // *(uint32_t*)(0x2000F204) = xtal_reg_val;
        /* power on */
        HBN_Set_Xtal_32K_Inverter_Amplify_Strength(3);
        HBN_Power_On_Xtal_32K();
    } else {
        xtal32k_input();
    }

    timeout_start = qcc74x_mtimer_get_time_us();

    printf("xtal32k_check: delay 100 ms\r\n");
    vTaskDelay(500);


    printf("xtal32k_check: start check\r\n");

    HBN_32K_Sel(1);
    HBN_Enable_RTC_Counter();
    vTaskDelay(2);

    while(1){
        retry_cnt += 1;

        /* disable irq */
        __disable_irq();

        mtimer_record_us = qcc74x_mtimer_get_time_us();
        HBN_Get_RTC_Timer_Val((uint32_t *)&rtc_cnt, (uint32_t *)&rtc_cnt + 1);
        // HBN_Get_RTC_Timer_Val(&rtc_cnt_low, &rtc_cnt_high);
        // rtc_cnt = ((uint64_t)rtc_cnt_high << 32) | rtc_cnt_low;
        // printf("xtal32k_check: record rtc_cnt_high:%ld, rtc_cnt_low:%ld\r\n", rtc_cnt_high,rtc_cnt_low);
        // printf("xtal32k_check: record rtc_cnt:%lld \r\n", rtc_cnt);
        /* enable irq */
        __enable_irq();

        rtc_record_us = QCC74x_PDS_CNT_TO_US(rtc_cnt);

        /* delay */
        vTaskDelay(10);

         /* disable irq */
        __disable_irq();

        mtimer_now_us = qcc74x_mtimer_get_time_us();
        HBN_Get_RTC_Timer_Val((uint32_t *)&rtc_cnt, (uint32_t *)&rtc_cnt + 1);
        // HBN_Get_RTC_Timer_Val(&rtc_cnt_low, &rtc_cnt_high);
        // rtc_cnt = ((uint64_t)rtc_cnt_high << 32) | rtc_cnt_low;
        // printf("xtal32k_check: now rtc_cnt_high:%ld, rtc_cnt_low:%ld\r\n", rtc_cnt_high,rtc_cnt_low);
        // printf("xtal32k_check: now rtc_cnt:%lld \r\n", rtc_cnt);
        /* enable irq */
        __enable_irq();

        rtc_now_us = QCC74x_PDS_CNT_TO_US(rtc_cnt);

        /* calculate */
        rtc_us = (uint32_t)(rtc_now_us - rtc_record_us);
        mtimer_us = (uint32_t)(mtimer_now_us - mtimer_record_us);
        diff_us = rtc_us - mtimer_us;
        
        // printf("xtal32k_check: rtc_now_us:%lld, rtc_record_us:%lld\r\n", rtc_now_us, rtc_record_us);
        // printf("xtal32k_check: mtimer_now_us:%lld, mtimer_record_us:%lld\r\n", mtimer_now_us, mtimer_record_us);
        printf("xtal32k_check: mtimer_us:%ld, rtc_us:%ld\r\n", mtimer_us, rtc_us);

        if(diff_us < -100 || diff_us > 100){
            /* continue */
            printf("xtal32k_check: retry_cnt:%d, diff_us:%d, continue...\r\n", retry_cnt, diff_us);
            vTaskDelay(10);
        }else{
            /* finish */
            printf("xtal32k_check: retry_cnt:%d, diff_us:%d, finish!\r\n", retry_cnt, diff_us);
            success_flag = 1;
            break;
        }

        /* 1sec, set xtal regulator */
        if((xtal32_regulator_flag == 0) && (qcc74x_mtimer_get_time_us() - timeout_start > 1000*1000)){
            printf("xtal32K_check: reset xtal32k regulator\r\n");
            xtal32_regulator_flag = 1;

            HBN_32K_Sel(0);
            HBN_Power_Off_Xtal_32K();

            vTaskDelay(10);

            HBN_Set_Xtal_32K_Regulator(3);
            HBN_Power_On_Xtal_32K();
            HBN_32K_Sel(1);
        }

        if(qcc74x_mtimer_get_time_us() - timeout_start > 3 * 1000 * 1000){
            success_flag = 0;
            break;
        }
    }

    if(success_flag){
        printf("xtal32k_check: success!, total time:%dms\r\n", (int)(qcc74x_mtimer_get_time_us() - timeout_start) / 1000);

        /* GPIO17 no pull */
        *((volatile uint32_t *)0x2000F014) &= ~(1 << 16);

        ret = 1;
        printf("select xtal32k\r\n");

    }else{
        printf("xtal32k_check: failure!, total time:%dms\r\n", (int)(qcc74x_mtimer_get_time_us() - timeout_start) / 1000);
        printf("xtal32k_check: select rc32k, and xtal32k poweroff \r\n");
        HBN_32K_Sel(0);
        HBN_Power_Off_Xtal_32K();
    }

    /* TODO */
    // printf("xtal32k_check: set lp_32k ready!\r\n");
    // qcc74x_lp_set_32k_clock_ready(1); 

    return ret;
}


#define BASE_ADDRESS 0x2000f000
#define OFFSET 0x204

void modify_register_bits_incremental(int increment)
{
    volatile uint32_t *reg_address = (volatile uint32_t *)(BASE_ADDRESS + OFFSET);

    uint32_t mask = (0x3F << 11);
    uint32_t current_value = *reg_address;
    uint32_t target_bits = (current_value & mask) >> 11;

    /*
    printf("Before change:\r\n");
    printf("  Register value (full): 0x%08X\r\n", current_value);
    printf("  bit11 to bit16 value: 0x%X\r\n", target_bits);
    */

    target_bits = (int32_t)(target_bits) + increment;
    if (target_bits > 0x3F) {
        target_bits = 0x3F;
    } else if (target_bits < 0) {
        target_bits = 0;
    }

    current_value = (current_value & ~mask) | (target_bits << 11);
    *reg_address = current_value;
    current_value = *reg_address;
    target_bits = (current_value & mask) >> 11;

    /*
    printf("After change:\r\n");
    printf("  Register value (full): 0x%08X\r\n", current_value);
    printf("  bit11 to bit16 value: 0x%X\r\n", target_bits);
    */
}

static void xtal32k_calibration(void)
{
    uint32_t retry_cnt = 5;
    uint64_t timeout_start;

    uint64_t rtc_cnt, rtc_record_us, rtc_now_us;
    uint64_t mtimer_record_us, mtimer_now_us;

    uint32_t rtc_us, mtimer_us;
    int32_t diff_us;

    /* TODO */
    // qcc74x_lp_set_32k_clock_ready(0);

    while (retry_cnt) {
        retry_cnt--;

        __disable_irq();
        mtimer_record_us = qcc74x_mtimer_get_time_us();
        HBN_Get_RTC_Timer_Val((uint32_t *)&rtc_cnt, (uint32_t *)&rtc_cnt + 1);
        __enable_irq();

        rtc_record_us = QCC74x_PDS_CNT_TO_US(rtc_cnt);

        vTaskDelay(1000);

        __disable_irq();
        mtimer_now_us = qcc74x_mtimer_get_time_us();
        HBN_Get_RTC_Timer_Val((uint32_t *)&rtc_cnt, (uint32_t *)&rtc_cnt + 1);
        __enable_irq();

        rtc_now_us = QCC74x_PDS_CNT_TO_US(rtc_cnt);

        rtc_us = (uint32_t)(rtc_now_us - rtc_record_us);
        mtimer_us = (uint32_t)(mtimer_now_us - mtimer_record_us);
        diff_us = rtc_us - mtimer_us;

        if(diff_us < 0){
            modify_register_bits_incremental(-8);
        }else if (diff_us > 25){
            modify_register_bits_incremental(8);
        } else {
            printf("xtal32k_check: mtimer_us:%ld, rtc_us:%ld minus:%ld\r\n", mtimer_us, rtc_us, diff_us);
            break;
        }

        printf("xtal32k_check: mtimer_us:%ld, rtc_us:%ld minus:%ld\r\n", mtimer_us, rtc_us, diff_us);
    }
    /* TODO */
    // qcc74x_lp_set_32k_clock_ready(1);
}

static void xtal32k_select_task(void *pvParameters)
{

    xtal_reg_val = *(uint32_t*)(0x2000F204);
    if (xtal32k_check_entry_task(1)) {
        // calibration xtal32k
        // printf("xtal32k_calibration\r\n");
        // xtal32k_calibration();

    } else {
        xtal32k_check_entry_task(0);
    }

    printf("xtal32k_select_task: vTaskDelete\r\n");
    xtal32k_select_task_hd = NULL;
    vTaskDelete(NULL);
}

#endif

int32_t mfg_cmd_input(uint8_t *data, uint16_t len, int from_isr)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(len<1){
        return 0;
    }
    data[len]=0;

    //mfg_print("Comamnd input\r\n");
    //mfg_print("%s\r\n",data);
    //mfg_print("End\r\n");
    #if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
    if (data[0] != BT_LE_HCI_CMD){
    #endif
    while(len>=1&&(data[len-1]==0x0d||data[len-1]==0x0a)){
        data[len-1]=0;
        len-=1;
    }
    #if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
    }
    #endif

    if (from_isr) {
        xQueueSendToBackFromISR( Message_Queue, data, &xHigherPriorityTaskWoken );
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        xQueueSendToBack( Message_Queue, data, 0 );
    }

    return 0;
}

static unsigned char auchCRCHi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
} ;
/* Table of CRC values for low-order byte */
static char auchCRCLo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
} ;
unsigned short mfg_CRC16(unsigned char *puchMsg, unsigned short usDataLen)
{
    unsigned char uchCRCHi = 0xFF ;
    /* high CRC byte initialized */
    unsigned char uchCRCLo = 0xFF ;
    /* low CRC byte initialized */
    unsigned uIndex ;
    /* will index into CRC lookup table */
    while (usDataLen--)
    /* pass through message buffer */
    {
        uIndex = uchCRCHi ^ *puchMsg++ ;
        /* calculate the CRC */
        uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
        uchCRCLo = auchCRCLo[uIndex] ;
    }
    return (uchCRCHi << 8 | uchCRCLo) ;
}

void mfg_board_erase_boot_flag(void)
{
    uint32_t ret;
    uint32_t flag_addr;
    uint32_t cnt = 10;
    int ret1 = 0, ret2 = 0;
    uint32_t result;

    flag_addr = qcc74x_sf_ctrl_get_flash_image_offset(0, 0) + MFG_BOOT_REQ_ADDR;

    do {
        qcc74x_flash_erase(flag_addr, 4096);
        qcc74x_flash_read(flag_addr, &result, 4);

        cnt--;
        mfg_print("Erase Flag %08x,%d,%d,%08x", (unsigned int)flag_addr, ret1, ret2, (unsigned int)result);
        if (result != 0xFFFFFFFF) {
            mfg_print(" Fail\r\n");
            ret = ERROR;
        } else {
            mfg_print(" Success\r\n");
            ret = SUCCESS;
        }
    } while (ret != SUCCESS && cnt > 0);
}

//extern int32_t mfg_at_cmd_enter(uint8_t *data);
int32_t mfg_cmd_pre_deal(uint8_t *data)
{
    uint8_t ef_buf[32]={0};
    unsigned short crc;
    if(arch_memcmp((char *)data,"Reset",sizeof("Reset")-1)==0){
        GLB_SW_POR_Reset();
        return 0;
    }else if(arch_memcmp((char *)data,"ATSC",sizeof("ATSC")-1)==0){
        mfg_board_erase_boot_flag();
        return 0;
    }else if(arch_memcmp((char *)data,"AT^MAC?",sizeof("AT^MAC?")-1)==0){
        mfg_efuse_read(0x150,(uint32_t *)ef_buf,8,1);
        crc = mfg_CRC16((unsigned char *)ef_buf,30);
        if(arch_memcmp(&ef_buf[30],&crc,2) == 0){
            mfg_print("MAC:%02x%02x%02x%02x%02x%02x\r\n",ef_buf[0],ef_buf[1],ef_buf[2],ef_buf[3],ef_buf[4],ef_buf[5]);
            return 0;
        }
        mfg_efuse_read(0x128,(uint32_t *)ef_buf,8,1);
        crc = mfg_CRC16((unsigned char *)ef_buf,30);
        if(arch_memcmp(&ef_buf[30],&crc,2) == 0){
            mfg_print("MAC:%02x%02x%02x%02x%02x%02x\r\n",ef_buf[0],ef_buf[1],ef_buf[2],ef_buf[3],ef_buf[4],ef_buf[5]);
            return 0;
        }
        mfg_efuse_read(0x100,(uint32_t *)ef_buf,8,1);
        crc = mfg_CRC16((unsigned char *)ef_buf,30);
        if(arch_memcmp(&ef_buf[30],&crc,2) == 0){
            mfg_print("MAC:%02x%02x%02x%02x%02x%02x\r\n",ef_buf[0],ef_buf[1],ef_buf[2],ef_buf[3],ef_buf[4],ef_buf[5]);
            return 0;
        }
        mfg_print("ERROR\r\n");
        return 0;
    }else if(arch_memcmp((char *)data,"AT",sizeof("AT")-1)==0){
#ifndef MFG_RAM
        //mfg_at_cmd_enter(data);
#endif
        return 0;
    }else if(arch_memcmp((char *)data,"m154_enter",sizeof("m154_enter")-1)==0){
        m154_init();
        mfg_m154_en = 1;
        mfg_print("Enter 802.15.4 test\r\n");
        return 0;
    }else if(arch_memcmp((char *)data,"m154_exit",sizeof("m154_exit")-1)==0){
        mfg_m154_en = 0;
        m154_exit();
        mfg_print("Exit 802.15.4 test\r\n");
        return 0;
    }

    else if(arch_memcmp((char *)data,"xtal32k_select",sizeof("xtal32k_select")-1)==0){
        #ifdef CONFIG_XTAL_SELECT_EN
        /* auto select xtal32k */
        vTaskEnterCritical();
        mfg_print("Create xtal32k_select_task ...\r\n");
        xTaskCreateStatic(xtal32k_select_task, (char*)"xtal32k_select_task", 512, NULL, 20, qcc74x_mfg_xtal32k_select_stack, &xtal32k_select_task_hd);
        vTaskExitCritical();
        return 0;
        #endif
    }


    return -1;
}

static int32_t mfg_cmd_shake_hand(uint8_t *data, uint16_t len)
{
    mfg_print("mfg\r\n");

    return 0;
}



static void mfg_get_duty_value(uint8_t duty,uint16_t *frequency,uint16_t *length)
{

}

static uint16_t mfg_cal_duty_value(uint8_t duty,uint16_t length,uint16_t frequency)
{
    return 0;
}

static void mfg_update_duty()
{

}



static int32_t mfg_calc_ifs(mfg_tx_para_t tx_para)
{
    const float mode_11b_rate[]={1,2,5.5,11};
    const float mode_11g_rate[]={6,9,12,18,24,36,48,54};
    const float mode_11n_rate[]={6.5,13,19.5,26,39,52,58.5,65};
    const float mode_11ax_rate[]={8.1,16.3,24.4,32.5,48.8,65,73.1,81.3,97.5,108.3,121.9,135.4,4.0,8.1,16.3,24.4,8.1,16.3,24.4};

    if(tx_para.tx_duty == 100){
        return 4;
    }else{
        if(tx_para.tx_mode == MFG_TX_MODE_B){
            return (int32_t)(8.0 * tx_para.tx_len / mode_11b_rate[tx_para.tx_mcs] + 192)*(100-tx_para.tx_duty)/tx_para.tx_duty;
        }else if(tx_para.tx_mode == MFG_TX_MODE_G){
            return (int32_t)((8.0 * tx_para.tx_len + 22) / mode_11g_rate[tx_para.tx_mcs] + 20)*(100-tx_para.tx_duty)/tx_para.tx_duty;
        }else if(tx_para.tx_mode == MFG_TX_MODE_N){
            return (int32_t)((8.0 * tx_para.tx_len + 22)  / mode_11n_rate[tx_para.tx_mcs] + 36)*(100-tx_para.tx_duty)/tx_para.tx_duty;
        }else if(tx_para.tx_mode == MFG_TX_MODE_AX){
            return (int32_t)((8.0 * tx_para.tx_len + 22)  / mode_11ax_rate[tx_para.tx_mcs] + 40)*(100-tx_para.tx_duty)/tx_para.tx_duty;
        }else{
            return 100;
        }
    }


}

static int32_t mfg_cmd_tx_toggle(uint8_t *data, uint16_t len)
{
    if(mfg_m154_en){
        wl_rf_set_bz_target_power_table(g_mfg_tx_para.zb_sending_power);
        wl_rf_set_154_tx_power(g_mfg_tx_para.zb_sending_power);
        return m154_cmd_tx_toggle(data, len);
    }

    /* tx11b_start mcs=[1|2|3|4] len=1000 filter=1 pwrdbm=15 ifs=100 continuous=0 */
    if(data[0] == '1'){
        g_mfg_tx_para.tx_on = 1;
        if(g_mfg_tx_para.tx_mode == MFG_TX_MODE_B){

            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",20);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",20);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
            cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);

            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","tx11b_start");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"mcs=%d",g_mfg_tx_para.tx_mcs+1);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"len=%d",g_mfg_tx_para.tx_len);
            snprintf((char*)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]),"pwrdbm=%d",g_mfg_tx_para.tx_pwr_dbm);
            snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"ifs=%d",mfg_calc_ifs(g_mfg_tx_para));
            snprintf((char*)phy_cli_cmd[6],sizeof(phy_cli_cmd[6]),"preamble=%d",1);
            snprintf((char*)phy_cli_cmd[7],sizeof(phy_cli_cmd[7]),"bw=%d",20);

            cmd_tx11b_start_register(0,0,8,(char **)phy_cli_list);
        }else if(g_mfg_tx_para.tx_mode == MFG_TX_MODE_G){

            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",20);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",20);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
            cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);


        /* tx11g_start mcs=[0~7] len=4096 filter=1 pwrdbm=15 ifs=100 continuous=0 */
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","tx11g_start");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"mcs=%d",g_mfg_tx_para.tx_mcs);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"len=%d",g_mfg_tx_para.tx_len);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"pwrdbm=%d",g_mfg_tx_para.tx_pwr_dbm);
            snprintf((char*)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]),"ifs=%d",mfg_calc_ifs(g_mfg_tx_para));
            snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"bw=%d",20);
            cmd_tx11g_start_register(0,0,6,(char **)phy_cli_list);

        }else if(g_mfg_tx_para.tx_mode == MFG_TX_MODE_N){
            if(g_mfg_tx_para.bw == 40){
                memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
                snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
                snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",40);
                snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",40);
                snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
                cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
            }else if(g_mfg_tx_para.bw == 20){
                memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
                snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
                snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",20);
                snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",20);
                snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
                cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
            }
            /* tx11n_start mcs=[0~7] len=4096 filter=1 pwrdbm=15 ifs=100 continuous=0 ldpc=[0|1] */
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","tx11n_start");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"mcs=%d",g_mfg_tx_para.tx_mcs);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"len=%d",g_mfg_tx_para.tx_len);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"pwrdbm=%d",g_mfg_tx_para.tx_pwr_dbm);
            snprintf((char*)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]),"ldpc=%d",g_mfg_tx_para.ax_coding_type);
            snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"ifs=%d",mfg_calc_ifs(g_mfg_tx_para));
            snprintf((char*)phy_cli_cmd[6],sizeof(phy_cli_cmd[6]),"bw=%d",g_mfg_tx_para.bw);

            cmd_tx11n_start_register(0,0,7,(char **)phy_cli_list);

        }else if(g_mfg_tx_para.tx_mode == MFG_TX_MODE_AX){

            if(g_mfg_tx_para.bw == 40){
                memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
                snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
                snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",40);
                snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",40);
                snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
                cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
            }else if(g_mfg_tx_para.bw == 20){
                memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
                snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
                snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",20);
                snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",20);
                snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
                cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
            }

            /* MCS0: tx11ax_su_start su_mcs=0 su_len=4096 pwrdbm=10 su_fec=1 he_dcm=0 ifs=100 bw=20 */
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

            if(g_mfg_tx_para.tx_mcs <= 11){
                snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"su_mcs=%d",g_mfg_tx_para.tx_mcs);
            }else if((g_mfg_tx_para.tx_mcs >= 12) && (g_mfg_tx_para.tx_mcs <= 15)){
                if(g_mfg_tx_para.tx_mcs == 12){
                    snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"su_mcs=%d",0);
                }else if(g_mfg_tx_para.tx_mcs == 13){
                    snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"su_mcs=%d",1);
                }else if(g_mfg_tx_para.tx_mcs == 14){
                    snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"su_mcs=%d",3);
                }else if(g_mfg_tx_para.tx_mcs == 15){
                    snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"su_mcs=%d",4);
                }

            }else if((g_mfg_tx_para.tx_mcs >= 16) && (g_mfg_tx_para.tx_mcs <= 18)){
                snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"su_mcs=%d",g_mfg_tx_para.tx_mcs - 16);
            }

            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"su_len=%d",g_mfg_tx_para.tx_len);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"pwrdbm=%d",g_mfg_tx_para.tx_pwr_dbm);
            snprintf((char*)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]),"su_fec=%d",g_mfg_tx_para.ax_coding_type);

            if((g_mfg_tx_para.tx_mcs >= 12) && (g_mfg_tx_para.tx_mcs <= 15)){
                snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"he_dcm=%d",1);
            }else{
                snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"he_dcm=%d",0);
            }


            snprintf((char*)phy_cli_cmd[6],sizeof(phy_cli_cmd[6]),"ifs=%d",mfg_calc_ifs(g_mfg_tx_para));
            snprintf((char*)phy_cli_cmd[7],sizeof(phy_cli_cmd[7]),"bw=%d",g_mfg_tx_para.bw);

            snprintf((char*)phy_cli_cmd[8],sizeof(phy_cli_cmd[8]),"ldpc=%d",g_mfg_tx_para.ax_coding_type);

            if(g_mfg_tx_para.ax_gi == 0){
                snprintf((char*)phy_cli_cmd[9],sizeof(phy_cli_cmd[9]),"he_ltf_type=%d",1);
                snprintf((char*)phy_cli_cmd[10],sizeof(phy_cli_cmd[10]),"he_gi_type=%d",0);
            }else if(g_mfg_tx_para.ax_gi == 1){
                snprintf((char*)phy_cli_cmd[9],sizeof(phy_cli_cmd[9]),"he_ltf_type=%d",1);
                snprintf((char*)phy_cli_cmd[10],sizeof(phy_cli_cmd[10]),"he_gi_type=%d",1);
            }else if(g_mfg_tx_para.ax_gi == 2){
                snprintf((char*)phy_cli_cmd[9],sizeof(phy_cli_cmd[9]),"he_ltf_type=%d",2);
                snprintf((char*)phy_cli_cmd[10],sizeof(phy_cli_cmd[10]),"he_gi_type=%d",2);
            }

            if(g_mfg_tx_para.tx_mcs <= 15){
                snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","tx11ax_su_start");
                cmd_tx11ax_su_start_register(0,0,11,(char **)phy_cli_list);
            }else if((g_mfg_tx_para.tx_mcs >= 16) && (g_mfg_tx_para.tx_mcs <= 18)){
                snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","tx11ax_er_start");
                cmd_tx11ax_er_start_register(0,0,11,(char **)phy_cli_list);
            }
        }
    }else if(data[0] == '0'){
        cmd_tx_stop_register(0,0,0,0);
        g_mfg_tx_para.tx_on = 0;
        mfg_print("tx stop\r\n");
    }else if(data[0] == 'b'){
        int bw = 0;
        bw = data[5] - 0x30;
        g_mfg_tx_para.tx_on = 1;
        if(bw == 1){
            g_mfg_tx_para.bw = 40;
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",40);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",40);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
            cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
        }else if(bw == 0){
            g_mfg_tx_para.bw = 20;
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",20);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",20);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
            cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
        }
        /* MCS0: tx11ax_su_start su_mcs=0 su_len=4096 pwrdbm=10 su_fec=1 he_dcm=0 ifs=100 bw=20 */
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
        int tb_ru = 0, mcs = 0, l_length = 3250, tb_len = 400;
        int l_length_ru26[] = {3250,1642,2698,2038,1366,1030,922,850,850,850};
        int l_length_ru52[] =  {4030,2038,1366,1030,694,526,478,442,442,442};
        int l_length_ru106[] = {1918,970,658,502,346,262,238,226,226,226};
        int l_length_ru242[]= {850,442,298,238,166,130,118,118,118,118};

        mcs = data[4] - 0x30;
        if(mcs <2)
            tb_len = 400;
        else
            tb_len = 1000;
        if(data[1] == '0'){
            tb_ru = (0 + (data[2]-0x30)*10 + data[3]-0x30)*2;
            l_length = l_length_ru26[mcs];
        }
        else if(data[1] == '1'){
            tb_ru = (37 + data[3]-0x30)*2;
            l_length = l_length_ru52[mcs];
        }
        else if(data[1] == '2'){
            tb_ru = (53 + data[3]-0x30)*2;
            l_length = l_length_ru26[mcs];
        }
        else if(data[1] == '3'){
            tb_ru = (61 + data[3]-0x30)*2;
            l_length = l_length_ru106[mcs];
        }
        else if(data[1]=='4'){
            tb_ru = 65*2;
            l_length = l_length_ru242[mcs];
        }
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"pwrdbm=%d",g_mfg_tx_para.tx_pwr_dbm);
        snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"l_length=%d",l_length);
        snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"he_gi_type=%d",1);
        snprintf((char*)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]),"he_ltf_type=%d",1);
        snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"he_doppler=%d",0);
        snprintf((char*)phy_cli_cmd[6],sizeof(phy_cli_cmd[6]),"he_midamble=%d",0);
        snprintf((char*)phy_cli_cmd[7],sizeof(phy_cli_cmd[7]),"tb_mcs=%d",mcs);
        snprintf((char*)phy_cli_cmd[8],sizeof(phy_cli_cmd[8]),"tb_fec=%d",1);
        snprintf((char*)phy_cli_cmd[9],sizeof(phy_cli_cmd[9]),"tb_len=%d",tb_len);
        snprintf((char*)phy_cli_cmd[10],sizeof(phy_cli_cmd[10]),"tb_pe=%d",0);
        snprintf((char*)phy_cli_cmd[11],sizeof(phy_cli_cmd[11]),"tb_ru=%d",tb_ru);
        snprintf((char*)phy_cli_cmd[12],sizeof(phy_cli_cmd[12]),"tb_ltf_num=%d",0);
        snprintf((char*)phy_cli_cmd[13],sizeof(phy_cli_cmd[13]),"tb_ltf_mode=%d",1);
        snprintf((char*)phy_cli_cmd[14],sizeof(phy_cli_cmd[14]),"tb_ldpc_extra_sym=%d",0);
        snprintf((char*)phy_cli_cmd[15],sizeof(phy_cli_cmd[15]),"tb_trigger_method=%d",0);
        snprintf((char*)phy_cli_cmd[16],sizeof(phy_cli_cmd[16]),"bw=%d",g_mfg_tx_para.bw);
        cmd_tx11ax_tb_start_register(0,0,17,(char **)phy_cli_list);
    }
    return 0;
}


#ifdef MFG_GU //defined(MFG_LPGU)

#define GET_OFFSET(_type, _member) ((unsigned long)(&((_type *)0)->_member))
static volatile iot2lp_para_t *iot2lp_para = (iot2lp_para_t *)IOT2LP_PARA_ADDR;
qcc74x_lp_fw_cfg_t g_lp_fw_cfg = {
    .tim_wakeup_en = 1, /* 1: enable tim wakeup */
    .channel = 11,
    .bssid = {},
    .mac = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
    .aid = 2,
    .tpre = 120,
    .rtc_timeout_ms = 0, /* 0: disable rtc wakeup */
    .dtim_num = 10,
    .mtimer_timeout_ms = 10,
};
int stress_test_start=0;

int qcc74x_lp_fw_enter_check_allow(qcc74x_lp_fw_cfg_t *qcc74x_lp_fw_cfg)
{
    int ret = 0;
    uint32_t valLow, valHigh;
    uint32_t pds_sleep_ms;
    uint64_t current_time;
    uint32_t rtc_timeout_ms = qcc74x_lp_fw_cfg->rtc_timeout_ms;

    /* clean check_data_valid */
    iot2lp_para->check_data_valid = 0;
    iot2lp_para->rtc_timeout_ms = rtc_timeout_ms;
    iot2lp_para->pds_sleep_ms = 100;

    if(qcc74x_lp_fw_cfg->tim_wakeup_en == 0){
        /* No need to receive wifi */
        iot2lp_para->pds_sleep_ms = 0;
        return 1;
    }

    /* calculate pds sleep time */
    if (iot2lp_para->next_beacon_stamp_rtc_valid) {
        uint32_t sleep_time_us;
        /* pds cnt to us */
        HBN_Get_RTC_Timer_Val(&valLow, &valHigh);
        current_time = ((uint64_t)valHigh << 32) + valLow;
        current_time = QCC74x_PDS_CNT_TO_US(current_time);

        if ((current_time + 2) < iot2lp_para->next_beacon_stamp_rtc) {
            /* current time not expired the target time */
            sleep_time_us = iot2lp_para->next_beacon_stamp_rtc - current_time;
        } else {
            if (current_time < iot2lp_para->next_beacon_stamp_rtc) {
                /* we almost near the beacon wake up time, just sleep for netx one */
                sleep_time_us = iot2lp_para->dtim_num * 102400 + (iot2lp_para->next_beacon_stamp_rtc - current_time);
            } else {
                /* dtime beacon has already expired */
                sleep_time_us = iot2lp_para->dtim_num * 102400 - (current_time - iot2lp_para->next_beacon_stamp_rtc) % (unsigned int)(iot2lp_para->dtim_num * 102400);
            }
        }
        /* us to ms */
        pds_sleep_ms = sleep_time_us / 1000;

        if (iot2lp_para->next_beacon_stamp_rtc_valid == BEACON_STAMP_LPFW) {
            /* last beacon form lpfw */
            if (!(*((volatile uint32_t *)0x2000f030) & (1 << 3))) {
                /* rc32k */
                pds_sleep_ms = (pds_sleep_ms > 4) ? (pds_sleep_ms - 4) : 0;
            } else {
                /* xtal */
                pds_sleep_ms = (pds_sleep_ms > 2) ? (pds_sleep_ms - 2) : 0;
            }

        } else if (iot2lp_para->next_beacon_stamp_rtc_valid == BEACON_STAMP_APP) {
            /* last beacon form app */
            if (!(*((volatile uint32_t *)0x2000f030) & (1 << 3))) {
                /* rc32k */
                pds_sleep_ms = (pds_sleep_ms > 6) ? (pds_sleep_ms - 6) : 0;
            } else {
                /* xtal */
                pds_sleep_ms = (pds_sleep_ms > 4) ? (pds_sleep_ms - 4) : 0;
            }
        }

        if(pds_sleep_ms < 2){
            pds_sleep_ms = 2;
        }

        iot2lp_para->pds_sleep_ms = pds_sleep_ms;

        if(pds_sleep_ms < PROTECT_AF_MS){
            /* Time is too short, No sleep allowed  */
            return 0;
        }

    } else {
        /* It shouldn't be here */
        pds_sleep_ms = 100;
    }

    /* Calculate a safe rtc_timeout */
    if ((rtc_timeout_ms >= pds_sleep_ms && rtc_timeout_ms - pds_sleep_ms <= PROTECT_BF_MS) ||
        (pds_sleep_ms >= rtc_timeout_ms && pds_sleep_ms - rtc_timeout_ms <= PROTECT_AF_MS)){

        /* Dangerous time zone, Advance the rtc time */
        if(pds_sleep_ms > PROTECT_AF_MS){
            rtc_timeout_ms = pds_sleep_ms - PROTECT_AF_MS;
        }else{
            rtc_timeout_ms = 0;
            return 0;
        }
    }

    if(rtc_timeout_ms < 2){
        /* Time is too short, No sleep allowed */
        iot2lp_para->rtc_timeout_ms = 2;
        return 0;
    }

    /* Check passed. Record sleep time */
    iot2lp_para->check_data_valid = 1;
    iot2lp_para->pds_sleep_ms = pds_sleep_ms;
    iot2lp_para->rtc_timeout_ms = rtc_timeout_ms;

    return 1;
}
static void rtc_wakeup_init(uint32_t sleep_ms)
{
    uint32_t tmpVal;
    uint32_t comp_l, comp_h;
    uint64_t rtc_cnt;

    /* Set RTC compare mode, and enable rtc */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    tmpVal = tmpVal & 0xfffffff1;
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal | 0x00000003);

    /* Tigger RTC val latch  */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RTC_TIME_H);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_RTC_TIME_LATCH);
    QCC74x_WR_REG(HBN_BASE, HBN_RTC_TIME_H, tmpVal);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_RTC_TIME_LATCH);
    QCC74x_WR_REG(HBN_BASE, HBN_RTC_TIME_H, tmpVal);

    /* Read RTC cnt */
    rtc_cnt = (QCC74x_RD_REG(HBN_BASE, HBN_RTC_TIME_H) & 0xff) << 32;
    rtc_cnt |= QCC74x_RD_REG(HBN_BASE, HBN_RTC_TIME_L);

    /* calculate RTC Comp cnt */
    rtc_cnt += (float)sleep_ms * 32.768f;
    iot2lp_para->rtc_wakeup_cnt = rtc_cnt;

    /* Set RTC Comp cnt */
    QCC74x_WR_REG(HBN_BASE, HBN_TIME_H, (uint32_t)(rtc_cnt >> 32) & 0xff);
    QCC74x_WR_REG(HBN_BASE, HBN_TIME_L, (uint32_t)rtc_cnt);

    /* Set interrupt delay option */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_RTC_DLY_OPTION, HBN_RTC_INT_DELAY_0T);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, (uint32_t)tmpVal);
}

void lp_fw_save_cpu_para(uint32_t save_addr)
{
    __asm__ __volatile__(
        "sw      x0, 0(a0)\n\t"
        "sw      x1, 4(a0)\n\t"
        "sw      x2, 8(a0)\n\t"
        "sw      x3, 12(a0)\n\t"
        "sw      x4, 16(a0)\n\t"
        "sw      x5, 20(a0)\n\t"
        "sw      x6, 24(a0)\n\t"
        "sw      x7, 28(a0)\n\t"
        "sw      x8, 32(a0)\n\t"
        "sw      x9, 36(a0)\n\t"
        "sw      x10, 40(a0)\n\t"
        "sw      x11, 44(a0)\n\t"
        "sw      x12, 48(a0)\n\t"
        "sw      x13, 52(a0)\n\t"
        "sw      x14, 56(a0)\n\t"
        "sw      x15, 60(a0)\n\t"
        "sw      x16, 64(a0)\n\t"
        "sw      x17, 68(a0)\n\t"
        "sw      x18, 72(a0)\n\t"
        "sw      x19, 76(a0)\n\t"
        "sw      x20, 80(a0)\n\t"
        "sw      x21, 84(a0)\n\t"
        "sw      x22, 88(a0)\n\t"
        "sw      x23, 92(a0)\n\t"
        "sw      x24, 96(a0)\n\t"
        "sw      x25, 100(a0)\n\t"
        "sw      x26, 104(a0)\n\t"
        "sw      x27, 108(a0)\n\t"
        "sw      x28, 112(a0)\n\t"
        "sw      x29, 116(a0)\n\t"
        "sw      x30, 120(a0)\n\t"
        "sw      x31, 124(a0)\n\t");
}
void lp_fw_restore_cpu_para(uint32_t save_addr)
{
    __asm__ __volatile__(
        "lw      x0, 0(a0)\n\t"
        "lw      x1, 4(a0)\n\t"
        "lw      x2, 8(a0)\n\t"
        "lw      x3, 12(a0)\n\t"
        "lw      x4, 16(a0)\n\t"
        "lw      x5, 20(a0)\n\t"
        "lw      x6, 24(a0)\n\t"
        "lw      x7, 28(a0)\n\t"
        "lw      x8, 32(a0)\n\t"
        "lw      x9, 36(a0)\n\t"
        "lw      x10, 40(a0)\n\t"
        "lw      x11, 44(a0)\n\t"
        "lw      x12, 48(a0)\n\t"
        "lw      x13, 52(a0)\n\t"
        "lw      x14, 56(a0)\n\t"
        "lw      x15, 60(a0)\n\t"
        "lw      x16, 64(a0)\n\t"
        "lw      x17, 68(a0)\n\t"
        "lw      x18, 72(a0)\n\t"
        "lw      x19, 76(a0)\n\t"
        "lw      x20, 80(a0)\n\t"
        "lw      x21, 84(a0)\n\t"
        "lw      x22, 88(a0)\n\t"
        "lw      x23, 92(a0)\n\t"
        "lw      x24, 96(a0)\n\t"
        "lw      x25, 100(a0)\n\t"
        "lw      x26, 104(a0)\n\t"
        "lw      x27, 108(a0)\n\t"
        "lw      x28, 112(a0)\n\t"
        "lw      x29, 116(a0)\n\t"
        "lw      x30, 120(a0)\n\t"
        "lw      x31, 124(a0)\n\t"
        "la      a0, freertos_risc_v_trap_handler\n\t"
        "ori     a0, a0, 3\n\t"
        "csrw    mtvec, a0\n\t");
}

int qcc74x_lp_fw_enter(qcc74x_lp_fw_cfg_t *qcc74x_lp_fw_cfg)
{
    int ret = 0;
    uint32_t dst_addr = LP_FW_START_ADDR;
    uint32_t lpfw_size = *((uint32_t *)__lpfw_start - 7);
    uint32_t pds_sleep_ms;
    uint32_t rtc_timeout_ms;

    if (qcc74x_lp_fw_cfg == NULL) {
        return -1;
    }

    if ((qcc74x_lp_fw_cfg->tim_wakeup_en == 0) && (qcc74x_lp_fw_cfg->rtc_timeout_ms == 0)) {
        /* no wake-up source */
        return -2;
    }

    /* reload lpfw */
    memcpy((void *)LP_FW_PRE_JUMP_ADDR, hbn_ucode, sizeof(hbn_ucode));
    memcpy((void *)(dst_addr), __lpfw_start, lpfw_size);

    qcc74x_lp_fw_cfg->wakeup_reason = 4;

    if(iot2lp_para->check_data_valid == 0){
        qcc74x_lp_fw_enter_check_allow(qcc74x_lp_fw_cfg);
    }

    iot2lp_para->check_data_valid = 0;
    pds_sleep_ms = iot2lp_para->pds_sleep_ms;
    rtc_timeout_ms = iot2lp_para->rtc_timeout_ms;

#if 1
    printf("next_beacon_stamp_rtc: %lld\r\n", iot2lp_para->next_beacon_stamp_rtc);
    printf("stamp_rtc_valid: %ld\r\n", iot2lp_para->next_beacon_stamp_rtc_valid);
    printf("pds_sleep_ms: %ld\r\n", pds_sleep_ms);
    printf("rtc_timeout_ms: %ld\r\n", rtc_timeout_ms);
    pds_sleep_ms -= 1;
    rtc_timeout_ms -= 1;
#endif
#if 0
    if (gp_lp_env->sys_pre_enter_callback) {
        ret = gp_lp_env->sys_pre_enter_callback(gp_lp_env->sys_enter_arg);
    }
    if (gp_lp_env->user_pre_enter_callback) {
        ret = gp_lp_env->user_pre_enter_callback(gp_lp_env->user_enter_arg);
    }
    assert(!ret);
#endif
    /* Save rc32k_fr_ext in HBN_RAM */
    iot2lp_para->rc32k_fr_ext = (*((volatile uint32_t *)0x2000F200)) >> 22;

    /* Set lp_fw as the wake-up entry  */
    pm_set_wakeup_callback((void (*)(void))LP_FW_PRE_JUMP_ADDR);
    *((volatile uint32_t *)0x2000e504) &= ~(1 << 6);
    pm_pds_mask_all_wakeup_src();
    AON_Output_Float_LDO15_RF();

    /* lpfw cfg: system para */
    iot2lp_para->lpfw_loss_cnt = 0;
    iot2lp_para->lpfw_recv_cnt = 0;
    iot2lp_para->pattern = 0xAA5555AA;
    iot2lp_para->wakeup_flag = 0;
    iot2lp_para->flash_offset = 0x11000;
    iot2lp_para->app_entry = (uint32_t)lp_fw_restore_cpu_para;
    iot2lp_para->args[0] = GET_OFFSET(iot2lp_para_t, cpu_regs) + IOT2LP_PARA_ADDR;
    /* lpfw cfg: rtc wakeup para */
    iot2lp_para->rtc_wakeup_ms = rtc_timeout_ms;

    /* rc32k auto calibration */
    iot2lp_para->rc32k_auto_cal = 1;
    iot2lp_para->last_rtc_cnt=0;
    iot2lp_para->last_time_stamp=0;
    iot2lp_para->recal_cnt=0;
    iot2lp_para->mtimer_timeout_ms = qcc74x_lp_fw_cfg->mtimer_timeout_ms;
    if (qcc74x_lp_fw_cfg->dtim_num != 0) {
        iot2lp_para->dtim_num = qcc74x_lp_fw_cfg->dtim_num;
    } else {
        iot2lp_para->dtim_num = 10;
    }

    if (qcc74x_lp_fw_cfg->tim_wakeup_en) {
        iot2lp_para->tim_wake_enable = 1;
        /* lpfw cfg: wifi para */
        memcpy(iot2lp_para->bssid, qcc74x_lp_fw_cfg->bssid, 6);
        memcpy(iot2lp_para->local_mac, qcc74x_lp_fw_cfg->mac, 6);
        iot2lp_para->ap_channel = qcc74x_lp_fw_cfg->channel;
        iot2lp_para->aid = qcc74x_lp_fw_cfg->aid;

        /* lpfw cfg: beacon attracking para */
        iot2lp_para->tpre = qcc74x_lp_fw_cfg->tpre;
        /* take rc32k error rate 1.5% as default */
        iot2lp_para->rc32k_err_rate = 15;
        iot2lp_para->lpfw_wakeup_cnt = 0;

        if (qcc74x_lp_fw_cfg->timeout_cnt_th) {
            iot2lp_para->timeout_cnt_th = qcc74x_lp_fw_cfg->timeout_cnt_th;
        } else {
            iot2lp_para->timeout_cnt_th = 3; /* The default value is 3 */
        }

        if (qcc74x_lp_fw_cfg->ap_loss_cnt_th) {
            iot2lp_para->ap_loss_cnt_th = qcc74x_lp_fw_cfg->ap_loss_cnt_th;
        } else {
            iot2lp_para->ap_loss_cnt_th = iot2lp_para->timeout_cnt_th + 3;
        }

    } else {
        iot2lp_para->tim_wake_enable = 0;
    }

    __disable_irq();
    //TS_RECORD(TS_ALLOW_SLEEP_QCC74x_LP);
    iot2lp_para->ap_loss_cnt_th = 255;

#ifdef TICKLESS_DEBUG
    time_static_record_dump();
#endif
    L1C_DCache_Clean_All();
    lp_fw_save_cpu_para(GET_OFFSET(iot2lp_para_t, cpu_regs) + IOT2LP_PARA_ADDR);
    arch_delay_ms(2);
    if (iot2lp_para->wakeup_flag == 0) {
        if ((qcc74x_lp_fw_cfg->tim_wakeup_en) && (rtc_timeout_ms)) {
            /* rtc cfg */
            rtc_wakeup_init(rtc_timeout_ms);
            /* clear rtc IRQ */
            QCC74x_WR_REG(HBN_BASE, HBN_IRQ_CLR, 0x00000001 << 16);
            /* enable rtc wakeup source */
            QCC74x_WR_REG(PDS_BASE, PDS_INT, QCC74x_RD_REG(PDS_BASE, PDS_INT) | (0x00000001 << 11));
            arch_delay_ms(2);
            /* pds15 enter */
            pm_pds_mode_enter(PM_PDS_LEVEL_15, QCC74x_MS_TO_PDS_CNT(pds_sleep_ms));

        } else if (qcc74x_lp_fw_cfg->tim_wakeup_en) {
            /* disable rtc comp */
            uint32_t tmpVal;
            tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
            tmpVal = tmpVal & 0xfffffff1;
            QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);
            arch_delay_ms(2);
            /* pds15 enter */
            pm_pds_mode_enter(PM_PDS_LEVEL_15, QCC74x_MS_TO_PDS_CNT(pds_sleep_ms));

        } else if (rtc_timeout_ms) {
            arch_delay_ms(2);
            /* pds15 enter */
            pm_pds_mode_enter(PM_PDS_LEVEL_15, QCC74x_MS_TO_PDS_CNT(rtc_timeout_ms));
        }

        iot2lp_para->wakeup_flag = 1;
        lp_fw_restore_cpu_para(GET_OFFSET(iot2lp_para_t, cpu_regs) + IOT2LP_PARA_ADDR);
    }
    /* try to use app mtimer to cal rc32,so make beacon_stamp_mtimer_valid=0 and init rc32k auto cal
       if xtal32 used, following code is also ok */
    pm_rc32k_auto_cal_init();
    arch_delay_ms(2);

    /* The rc32k code is restored  */
    uint32_t reg = *((volatile uint32_t *)0x2000F200) & ~(0x3FF << 22);
    reg = reg | iot2lp_para->rc32k_fr_ext << 22;
    *((volatile uint32_t *)0x2000F200) = reg;

    qcc74x_lp_fw_cfg->wakeup_reason = iot2lp_para->wakeup_reason;
    qcc74x_lp_fw_cfg->lpfw_recv_cnt = iot2lp_para->lpfw_recv_cnt;
    qcc74x_lp_fw_cfg->lpfw_loss_cnt = iot2lp_para->lpfw_loss_cnt;


    arch_delay_ms(2);
    //CPU_Interrupt_Enable(7);
    //CPU_Interrupt_Enable(44);
    __enable_irq();

    return iot2lp_para->wakeup_reason;
}
#endif
static int32_t mfg_cmd_unicast(uint8_t *data, uint16_t len)
{
    return 0;
}

static int32_t mfg_cmd_rx_ctrl(uint8_t *data, uint16_t len)
{
    if(mfg_m154_en){
        return m154_cmd_rx_ctrl(data, len);
    }

#ifdef MFG_GU//defined(MFG_LPGU)
    if((data[0] == ':') && (data[1] == 'l') && (data[2] == 'e')){

        GLB_Set_EM_Sel(GLB_WRAM160KB_EM0KB);
        iot2lp_para->next_beacon_stamp_rtc_valid = 0;
        iot2lp_para->continuous_loss_cnt = 0;
        iot2lp_para->check_data_valid = 0;

        if (iot2lp_para->pattern != 0xAA5555AA || iot2lp_para->dtim_num == 0) {
            iot2lp_para->dtim_num = 10;
        }

        g_lp_fw_cfg.channel = 11;

        g_lp_fw_cfg.bssid[0]=0x67;
        g_lp_fw_cfg.bssid[1]=0x39;
        g_lp_fw_cfg.bssid[2]=0x33;
        g_lp_fw_cfg.bssid[3]=0x04;
        g_lp_fw_cfg.bssid[4]=0xb9;
        g_lp_fw_cfg.bssid[5]=0x4d;

        g_lp_fw_cfg.tpre = 0;
        g_lp_fw_cfg.rtc_timeout_ms = 6000000;
        stress_test_start = 0;
        g_lp_fw_cfg.dtim_num = 10;
        g_lp_fw_cfg.mtimer_timeout_ms = 0;

        printf("Channel:%d,BSSID=%02x:%02x:%02x:%02x:%02x:%02x,tpr=%d, rtc_time=%d, dtim_num=%d, timeout=%d\r\n",
        g_lp_fw_cfg.channel, g_lp_fw_cfg.bssid[0], g_lp_fw_cfg.bssid[1], g_lp_fw_cfg.bssid[2], g_lp_fw_cfg.bssid[3], g_lp_fw_cfg.bssid[4], g_lp_fw_cfg.bssid[5],
        g_lp_fw_cfg.tpre, g_lp_fw_cfg.rtc_timeout_ms,g_lp_fw_cfg.dtim_num,g_lp_fw_cfg.mtimer_timeout_ms);

        qcc74x_lp_fw_enter(&g_lp_fw_cfg);
    }
#endif

    if(phy_init_flag == 0){
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_init");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"%s",MFG_XTAL_VALUE_STR);
        cmd_phy_init_register(0,0,2,(char **)phy_cli_list);
        phy_init_flag = 1;
    }

    if((data[0] == ':')&&(data[1] == 's')){
        mfg_print("len %d\r\n",len);
        if(data[2] ==  '2'){
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",20);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",20);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
            cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
        }else if(data[2] == '4'){
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"fbw=%d",40);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"cbw=%d",40);
            snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"primary=%d",-1);
            cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);
        }
        cmd_rx_start_at(0,0,0,0);
    }else if ((data[0] == ':') && (data[1] == 'g')){
        cmd_rx_poll_stat_at(0,0,0,0);
#if defined(MFG_CCI_QCC743)
        cmd_rx_ate_get_ret(&cci_ate_ret);

        //cli_ate_ret_t
        mfg_print("get ate ret,rxok %d,rxend %d, ppm_dsss %f, ppm_ofdm %f,rssi %d\r\n",\
        (int)cci_ate_ret.rxok,(int)cci_ate_ret.rxend,cci_ate_ret.ppm_dsss,cci_ate_ret.ppm_ofdm,cci_ate_ret.rssi);
#endif
    }else if ((data[0] == ':') && (data[1] == 'p')){
        cmd_rx_stop_at(0,0,0,0);
    }else if((data[0] == ':') && (data[1] == 'i')){
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","cmd_rx_start_ate");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"rssi=%d",-atoi((char *)&data[2]));
        mfg_print("%s\r\n",(char *)phy_cli_cmd[1]);
        extern void cmd_rx_start_ate(char *buf, int len, int argc, char **argv);
        cmd_rx_start_ate(0,0,2,(char **)phy_cli_list);
    }else{
        mfg_print("cmd param error\r\n");
    }

    return 0;
}

static int32_t mfg_cmd_frequency_ctrl(uint8_t *data, uint16_t len)
{
    if(mfg_m154_en){
        return m154_cmd_frequency_ctrl(data, len);
    }

    mfg_print("tx frenquecy:%s\r\n", data);

    g_mfg_tx_para.frequency = atoi((char *)data);
    return 0;
}

static void mfg_construct_11n_cont_tx(uint8_t mcs)
{

}

static int32_t mfg_cmd_11n_shortgi_rate_ctrl(uint8_t *data, uint16_t len)
{
    return 0;
}

static int32_t mfg_cmd_11n_longgi_rate_ctrl(uint8_t *data, uint16_t len)
{

    return 0;
}

static int32_t mfg_cmd_mode_ctrl(uint8_t *data, uint16_t len)
{
    mfg_print("11n tx\r\n");
    g_mfg_tx_para.tx_mode = MFG_TX_MODE_N;
    g_mfg_tx_para.tx_mcs = data[3] - 0x30;
    //g_mfg_tx_para.ax_coding_type = data[4] - 0x30;

    if(data[2] == '2'){
        g_mfg_tx_para.bw = 20;
    }else if(data[2] == '4'){
        g_mfg_tx_para.bw = 40;
    }
    g_mfg_tx_para.ax_coding_type = data[4] - 0x30;
    mfg_print("tx mode %d\r\n",g_mfg_tx_para.tx_mode);
    mfg_print("tx mcs %d\r\n",g_mfg_tx_para.tx_mcs);
    mfg_print("tx bw %d\r\n",g_mfg_tx_para.bw);
    mfg_print("tx coding type %d\r\n",g_mfg_tx_para.ax_coding_type);
    return 0;
}

static int32_t mfg_cmd_test_mode_ctrl(uint8_t *data, uint16_t len)
{
    uint8_t mfg_mode;
    mfg_mode = data[0] - 0x30;

    if(mfg_mode == 0){
        if(g_mfg_tx_para.mfgmode == 1){
            /* stop cw mode */
            cmd_cw_stop_register(0, 0, 0, 0);
        }
    }
    else if(mfg_mode == 1){
        if(g_mfg_tx_para.mfgmode == 0){
            /* start cw mode */
            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","cw_start");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"freq_mhz=%d",g_mfg_tx_para.sg_channel);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"pwr_dbm=%d",g_mfg_tx_para.tx_pwr_dbm);

            //mfg_print("%s\r\n",(char*)phy_cli_cmd[0]);
            //mfg_print("%s\r\n",(char*)phy_cli_cmd[1]);
            //mfg_print("%s\r\n",(char*)phy_cli_cmd[2]);
            cmd_cw_start_register(0,0,3,(char **)phy_cli_list);
        }
    }

    g_mfg_tx_para.mfgmode = mfg_mode;
    mfg_print("mfgmode %d\r\n",mfg_mode);
    return 0;
}

static void mfg_construct_11g_cont_tx(uint8_t mcs)
{

}

static int32_t mfg_cmd_11g_longpre_ctrl(uint8_t *data, uint16_t len)
{
    mfg_print("11g long preamble tx");
    g_mfg_tx_para.tx_mode = MFG_TX_MODE_G;
    g_mfg_tx_para.tx_mcs = atoi((char *)&data[0]);
    mfg_print("tx mode %d\r\n",g_mfg_tx_para.tx_mode);
    mfg_print("tx mcs %d\r\n",g_mfg_tx_para.tx_mcs);
    return 0;
}

static int32_t mfg_cmd_11g_shortpre_ctrl(uint8_t *data, uint16_t len)
{
    return 0;
}

static void mfg_construct_11b_cont_tx(uint8_t mcs)
{

}

/*0: short preamble; 1: Long preamble*/
static int32_t mfg_cmd_11b_rate_ctrl(uint8_t *data, uint16_t len,uint32_t pre_type)
{
    return 0;
}

static int32_t mfg_cmd_11b_longpre_ctrl(uint8_t *data, uint16_t len)
{
    g_mfg_tx_para.tx_mode = MFG_TX_MODE_B;
    g_mfg_tx_para.tx_mcs = atoi((char *)&data[0]);
    mfg_print("tx mode %d\r\n",g_mfg_tx_para.tx_mode);
    mfg_print("tx mcs %d\r\n",g_mfg_tx_para.tx_mcs);
    return 0;
}

static int32_t mfg_cmd_11b_shortpre_ctrl(uint8_t *data, uint16_t len)
{
    g_mfg_tx_para.tx_mode = MFG_TX_MODE_B;
    g_mfg_tx_para.tx_mcs = atoi((char *)&data[0]);
    mfg_print("tx mode %d\r\n",g_mfg_tx_para.tx_mode);
    mfg_print("tx mcs %d\r\n",g_mfg_tx_para.tx_mcs);
    return 0;
}

static int32_t mfg_cmd_channel_switch(uint8_t *data, uint16_t len)
{
    if(mfg_m154_en){
        return m154_cmd_channel_switch(data, len);
    }

    int channel_index;

    if(phy_init_flag == 0){
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_init");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"%s",MFG_XTAL_VALUE_STR);
        cmd_phy_init_register(0,0,2,(char **)phy_cli_list);
        phy_init_flag = 1;
    }
//    memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

//    snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_bandwidth");
//    snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"%s","fbw=20");
//    snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"%s","cbw=20");
//    snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"%s","primary=1");

//    cmd_bandwidth_register(0,0,4,(char **)phy_cli_list);


    memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
    channel_index = atoi((char *)data);

    snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","phy_channel");
    snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"%d",channel_freq[channel_index-1]);

    cmd_channel_register(0,0,2,phy_cli_list);
    g_mfg_tx_para.sg_channel = channel_freq[channel_index-1];
    g_mfg_tx_para.sg_channel_index = channel_index;

    if(g_mfg_tx_para.mfgmode == 1){
        /* start cw mode */
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","cw_start");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"freq_mhz=%d",g_mfg_tx_para.sg_channel);
        snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"pwr_dbm=%d",g_mfg_tx_para.tx_pwr_dbm);

        //mfg_print("%s\r\n",(char*)phy_cli_cmd[0]);
        //mfg_print("%s\r\n",(char*)phy_cli_cmd[1]);
        //mfg_print("%s\r\n",(char*)phy_cli_cmd[2]);
        cmd_cw_start_register(0,0,3,(char **)phy_cli_list);
    }

    return 0;
}

static int32_t mfg_cmd_power_ctrl(uint8_t *data, uint16_t len)
{

    if (mfg_m154_en) {
        g_mfg_tx_para.zb_sending_power = atoi((char *)&data[0]);
        if (g_mfg_tx_para.zb_sending_power > 20) {
            g_mfg_tx_para.zb_sending_power = 20;
        }
        mfg_print("15.4 tx power %d\r\n",g_mfg_tx_para.zb_sending_power);
        return 0;
    }

    g_mfg_tx_para.tx_pwr_dbm = atoi((char *)&data[0]);
    mfg_print("tx power %d\r\n",g_mfg_tx_para.tx_pwr_dbm);

    if(g_mfg_tx_para.mfgmode == 1){
        /* start cw mode */
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","cw_start");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"freq_mhz=%d",g_mfg_tx_para.sg_channel);
        snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"pwr_dbm=%d",g_mfg_tx_para.tx_pwr_dbm);

        //mfg_print("%s\r\n",(char*)phy_cli_cmd[0]);
        //mfg_print("%s\r\n",(char*)phy_cli_cmd[1]);
        //mfg_print("%s\r\n",(char*)phy_cli_cmd[2]);
        cmd_cw_start_register(0,0,3,(char **)phy_cli_list);
    }
    return 0;
}

static int32_t mfg_cmd_len_ctrl(uint8_t *data, uint16_t len)
{
    g_mfg_tx_para.tx_len = atoi((char *)&data[0]);
    mfg_print("tx len %d\r\n",g_mfg_tx_para.tx_len);
    return 0;
}

static int32_t mfg_cmd_11ax_ctrl(uint8_t *data, uint16_t len)
{
    mfg_print("11ax tx\r\n");
    g_mfg_tx_para.tx_mode = MFG_TX_MODE_AX;
    g_mfg_tx_para.ax_coding_type = data[0] - 0x30;
    if(g_mfg_tx_para.ax_coding_type == 0){
        mfg_print("coding type BCC\r\n");
    }else if(g_mfg_tx_para.ax_coding_type == 1){
        mfg_print("coding type LDPC\r\n");
    }else{
        mfg_print("cmd param err\r\n");
    }
    g_mfg_tx_para.ax_gi = data[1] - 0x30;
    if(g_mfg_tx_para.ax_gi == 0){
        mfg_print("2x HELTF+0.8us GI\r\n");
    }else if(g_mfg_tx_para.ax_gi == 1){
        mfg_print("2x HELTF+1.6us GI\r\n");
    }else if(g_mfg_tx_para.ax_gi == 2){
        mfg_print("4x HELTF+3.2us GI\r\n");
    }else{
        mfg_print("cmd param err\r\n");
    }
    g_mfg_tx_para.tx_mcs = (data[2] - 0x30) * 10 + (data[3] - 0x30);
    mfg_print("mcs %d\r\n",g_mfg_tx_para.tx_mcs);

    if(data[4] == '0'){
        g_mfg_tx_para.bw = 20;
    }else if(data[4] == '1'){
        g_mfg_tx_para.bw = 40;
    }else if(data[4] == '2'){
        g_mfg_tx_para.bw = 80;
    }else if(data[4] == '3'){
        g_mfg_tx_para.bw = 160;
    }

    mfg_print("tx bw %d\r\n",g_mfg_tx_para.bw);

    return 0;
}



#if defined(CONFIG_BLE_MFG)

/* BLE Tx with Hopping for KCC */
static void ble_tx_hopping_task_entry(void *pvParameters)
{
    uint8_t data[13] = {0};
    uint8_t tx_channel = 0;
    struct hci_le_tx_test_v1_cmd tx_v1_param;
    tx_v1_param.tx_channel = tx_channel;
    tx_v1_param.test_data_len = 37;
    tx_v1_param.pkt_payl = 0;

    while(1){
        if (ble_tx_hopping_task_en){
            tx_v1_param.tx_channel = tx_channel;
            hci_le_tx_test_v1_cmd_handler(&tx_v1_param, LE_TX_TEST_CMD_V1_OPCODE, true);
            // ETE002500010f
            // memset((char *)&data[0],0,sizeof(data));
            // memcpy((char *)&data[0],(char *)"ETE",3);
            // snprintf((char*)&data[3],"%02x",tx_channel);
            // memcpy((char *)&data[5],(char *)"2500010F",8);
            // xQueueSendToBack( Message_Queue, data, 13 );
            tx_channel++;
            if(tx_channel >= 40){
                tx_channel = 0;
            }
            vTaskDelay(10);
            // memset((char *)&data[0],0,sizeof(data));
            // memcpy((char *)&data[0],(char *)"EE",2);
            // xQueueSendToBack( Message_Queue, data, 13 );
            hci_le_test_end_cmd_handler(NULL, LE_TEST_END_CMD_OPCODE, true);
            vTaskDelay(10);
        } else {
            vTaskDelay(1000);
        }
    }
}

static int32_t mfg_cmd_le_test(uint8_t *data, uint16_t len)
{
    uint8_t status;

    int8_t power;
    uint8_t ble_channel;
    uint8_t x,y;

    /* if do ble test, wifi controller need to re-init  */
    phy_init_flag = 0;

    if(data[0] == 'R'){
        /*DEV CUBE:ERE0302 rx_channel=03 rx_rate=02*/

        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","le_rx_test");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"%c%c",data[2],data[3]);
        snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"%c%c",data[4],data[5]);
        snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"s","00");

        cmd_le_rx_test(0, 0, 4, (char **)phy_cli_list);

    }else if(data[0] == 'T'){
        *(volatile uint32_t *)0x20001220 |= (0x1<<26)|(0x1<<27);
        g_mfg_tx_para.ble_is_sending = 1;
        /*le_tx_test 0F 1F 00 01 00
        tx_channel = 0x0F, test_data_len = 0x1F, packet_payload = 0x00, tx_phy=0x01, power_level=0x00
        */


        /*
        DEV CUBE:ETE032500010f tx_channel=03 test_data_len=25  payload_type=00 tx_rate=01 power=0f
        */

        if(g_mfg_tx_para.mfgmode == 1){
            /* start cw mode */

            char2hex(data[10], &x);
            char2hex(data[11],&y);
            power = x * 16 + y;
            char2hex(data[2], &x);
            char2hex(data[3],&y);
            ble_channel = x * 16 + y;

            mfg_print("ble cw channel %d\r\n",(int)ble_channel_freq[ble_channel]);

            mfg_print("ble cw power %d\r\n",power);

            memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

            snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","cw_start");
            snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"freq_mhz=%d",ble_channel_freq[ble_channel]);
            snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"pwr_dbm=%d",power);


            cmd_cw_start_register(0,0,3,(char **)phy_cli_list);

            return 0;
        }

        if(data[10] == '-' && data[11] == '1'){
            struct wl_cfg_t * rfparam_cfg = rfparam_cfg_get();
            power = rfparam_cfg->param.pwrtarget.pwr_ble;
        }else{
            char2hex(data[10], &x);
            char2hex(data[11],&y);
            power = x * 16 + y;
        }

        g_mfg_tx_para.ble_sending_power = power;
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","bz_set_target_power");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"power=%d",power);
        cmd_bz_set_target_power_register(0, 0, 2, (char **)phy_cli_list);

        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","le_tx_test");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"%c%c",data[2],data[3]);
        snprintf((char*)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]),"%c%c",data[4],data[5]);
        snprintf((char*)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]),"%c%c",data[6],data[7]);
        snprintf((char*)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]),"%c%c",data[8],data[9]);
        if(data[10] == '-' && data[11] == '1'){
            snprintf((char *)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]), "%c%c", hex2char(power>>4), hex2char(power&0x0f));
        }else{
            snprintf((char*)phy_cli_cmd[5],sizeof(phy_cli_cmd[5]),"%c%c",data[10],data[11]);
        }

        mfg_print("ble pwr %d\r\n",(int)power);
        cmd_le_tx_test(0, 0, 6, (char **)phy_cli_list);
    }else if(data[0] == 'E'){
        phy_init_flag = 0;
        g_mfg_tx_para.ble_is_sending = 0;

        if(g_mfg_tx_para.mfgmode == 1){
            cmd_cw_stop_register(0, 0, 0, 0);
            return 0;
        }

        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));

        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","le_test_stop");
        cmd_le_test_stop(0, 0, 1, (char **)phy_cli_list);
    }else if(data[0] == 'S'){

        static mfg_thread_state_t thread_create_flag = MFG_THREAD_NOT_CREATE;
        static TaskHandle_t handle;

        if(data[1] == '0'){
            if(MFG_THREAD_RUNNING == thread_create_flag){
                // vTaskSuspend(handle);
                ble_tx_hopping_task_en = 0;
                thread_create_flag = MFG_THREAD_SUSPEND;
                mfg_print("suspend ble tx hopping test thread ok\r\n");
            }else if(MFG_THREAD_SUSPEND == thread_create_flag){
                mfg_print("mfg ble tx hopping test thread already suspended\r\n");
            }else if(MFG_THREAD_NOT_CREATE == thread_create_flag){
                mfg_print("mfg ble tx hopping test thread not created yet\r\n");
            }
        }else if(data[1] == '1'){
            if(MFG_THREAD_NOT_CREATE == thread_create_flag){
                vTaskEnterCritical();
                ble_tx_hopping_task_en = 1;
                xTaskCreate(ble_tx_hopping_task_entry, (char*)"ble_test", 128, NULL, 14 ,&handle);
                thread_create_flag = MFG_THREAD_RUNNING;
                mfg_print("mfg ble tx hopping test thread create ok\r\n");
                vTaskExitCritical();
            }else if(MFG_THREAD_SUSPEND == thread_create_flag){
                // vTaskResume(handle);
                ble_tx_hopping_task_en = 1;
                thread_create_flag = MFG_THREAD_RUNNING;
                mfg_print("mfg ble tx hopping test thread running\r\n");
            }else if(MFG_THREAD_RUNNING == thread_create_flag){
                mfg_print("mfg ble tx hopping test thread already running\r\n");
            }
        }
    }
#if defined(CONFIG_BT_MFG)
    else if(data[0] == 'B')
    {
        mfg_cmd_bt_test(&data[1],len);
    }
#endif
    else
    {
        mfg_print("Unknown command\r\n");
    }

    return 0;
}
#endif

#if defined(CONFIG_BT_MFG)
static int32_t mfg_cmd_bt_test(uint8_t *data, uint16_t len)
{
    uint8_t status;
    int32_t power;
    int16_t power_start;
    uint8_t bt_channel;
    uint16_t bt_freq;

    /* if do ble test, wifi controller need to re-init  */
    //phy_init_flag = 0;

    if (data[0] == 'R') {
        /*DEV CUBE:EBRB0302 rx_channel=03 pkt_type=02*/

        memset((char *)phy_cli_cmd, 0, sizeof(phy_cli_cmd));

        snprintf((char *)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]), "%s", "bt_rx_test");
        snprintf((char *)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]), "%c%c", data[2], data[3]);
        snprintf((char *)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]), "%c%c", data[4], data[5]);

        cmd_bt_rx_test(0, 0, 3, (char **)phy_cli_list);

    } else if (data[0] == 'T') {

        /*
        DEV CUBE:EBTB0300010a tx_channel=03 payload_type=00 pkt_type=01 power=0a
        */

        if (g_mfg_tx_para.mfgmode == 1) {
            uint8_t x, y,z,d;
            char2hex(data[12], &x);
            char2hex(data[13], &y);
            power_start = x * 16 + y;
            char2hex(data[2], &x);
            char2hex(data[3], &y);
            bt_channel = x * 16 + y;
            bt_freq = 2402 + (uint16_t)bt_channel;

            mfg_print("bt rfc_config_channel %d\r\n", (int)bt_freq);

            mfg_print("bt cmd_txsin_start_do power %d\r\n", power_start);

            //rfc_config_channel(bt_freq);
            power = power_start - 26;
            //cmd_txsin_start_do(0, 1, power, 0);
            //rf_pri_cw_start(power_start, bt_freq);

            return 0;
        }


        memset((char *)phy_cli_cmd, 0, sizeof(phy_cli_cmd));

        snprintf((char *)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]), "%s", "le_tx_test");
        snprintf((char *)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]), "%c%c", data[2], data[3]);
        snprintf((char *)phy_cli_cmd[2],sizeof(phy_cli_cmd[2]), "%c%c", data[4], data[5]);
        snprintf((char *)phy_cli_cmd[3],sizeof(phy_cli_cmd[3]), "%c%c", data[6], data[7]);
        snprintf((char *)phy_cli_cmd[4],sizeof(phy_cli_cmd[4]), "%c%c", data[8], data[9]);
        int8_t bt_tx_pwr = strtol((char*)&data[8],NULL,16);
        mfg_print("bt_tx_pwr %d\r\n",(int)bt_tx_pwr);
        if(bt_tx_pwr != -1){
            wl_rf_set_bz_target_power_table(bt_tx_pwr);
        }
        cmd_bt_tx_test(0, 0, 5, (char **)phy_cli_list);
    } else if (data[0] == 'E') {

        memset((char *)phy_cli_cmd, 0, sizeof(phy_cli_cmd));

        snprintf((char *)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]), "%s", "bt_test_stop");
        cmd_bt_test_stop(0, 0, 1, (char **)phy_cli_list);

    } else {
        mfg_print("Unknown command\r\n");
    }

    return 0;
}
#endif


static int32_t mfg_cmd_reaload_cal_data(uint8_t *data, uint16_t len)
{
//    uint16_t channel;
    struct wl_cfg_t * rfparam_cfg = rfparam_cfg_get();

    if(data[0] == '0'){
        rfparam_cfg->en_param_load = 0;
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz));
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan));
        memset(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan));
        //rfparam_printf("clear wlan and bz power offset\r\n");
    }else if(data[0] == '1'){
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz));
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan));
        memset(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan));
        rfparam_get_wlan_pwroffset_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,1);
        rfparam_get_wlan_pwroffset_lp_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,1);
        rfparam_get_bz_pwroffset_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,1);
    }else if(data[0] == '2'){

        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz));
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan));
        memset(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan));
        rfparam_get_wlan_pwroffset_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,0);
        rfparam_get_bz_pwroffset_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0);
    }else if(data[0] == '3'){
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz));
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan));
        memset(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan));
        rfparam_get_wlan_pwroffset_lp_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,0);
        rfparam_get_bz_pwroffset_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0);
    }else if(data[0] == '4'){
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_bz));
        memset(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_pwrcomp_wlan));
        memset(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan,0,sizeof(rfparam_cfg->param.pwrcal.channel_lp_pwrcomp_wlan));
        rfparam_get_bz_pwroffset_with_option(rfparam_tlv_base_addr_get(), rfparam_cfg->param.pwrcal.channel_pwrcomp_bz,0);
    }

    if(g_mfg_tx_para.ble_is_sending){
        memset((char *)phy_cli_cmd,0,sizeof(phy_cli_cmd));
        snprintf((char*)phy_cli_cmd[0],sizeof(phy_cli_cmd[0]),"%s","bz_set_target_power");
        snprintf((char*)phy_cli_cmd[1],sizeof(phy_cli_cmd[1]),"power=%d",g_mfg_tx_para.ble_sending_power);
        cmd_bz_set_target_power_register(0, 0, 2, (char **)phy_cli_list);
    }else{
        wl_rf_set_bz_channel_pwr_comp();
    }
    wl_rf_set_channel_pwr_comp(g_mfg_tx_para.sg_channel_index);

    return 0;
}



static int32_t mfg_cmd_write_efuse(uint8_t *data, uint16_t len)
{
    int32_t ret;
    if (data[0] == 'E') {
        if (data[1] == 'X') {
            if(!mfg_media_is_xtal_capcode_slot_empty(1)){
                mfg_print("cap code full\r\n");
            }
            ret = mfg_media_write_xtal_capcode_pre_with_lock((uint8_t)atoi((char *)&data[2]), 0);
            if (ret) {
                mfg_print("capcode write error,ret %d\r\n", ret);
            } else {
                mfg_print("capcode write suss\r\n");
            }

        } else if (data[1] == 'P') {

            int k = 0;
            int offset[14];
            int8_t pwrOffsetTable[14];
            memset(offset, 0, sizeof(offset));
            if(data[2] == 'H'){
                mfg_print("hp Pwr offset cmd:");
            }else if(data[2] == 'L'){
                mfg_print("lp Pwr offset cmd:");
            }

            sscanf((char *)&data[3], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                   &offset[0], &offset[1], &offset[2], &offset[3], &offset[4], &offset[5], &offset[6],
                   &offset[7], &offset[8], &offset[9], &offset[10], &offset[11], &offset[12], &offset[13]);
            for (k = 0; k < 14; k++) {
                /* Use 3 bits as signed value */
                if (offset[k] > MFG_PWR_OFFSET_MAX) {
                    offset[k] = MFG_PWR_OFFSET_MAX;
                }
                if (offset[k] < MFG_PWR_OFFSET_MIN) {
                    offset[k] = MFG_PWR_OFFSET_MIN;
                }
                pwrOffsetTable[k] = offset[k];
                mfg_print("%d ", pwrOffsetTable[k]);
            }
            mfg_print("\r\n");

            if(data[2] == 'H'){
                if(!mfg_media_is_hp_poweroffset_slot_empty(1)){
                    mfg_print("hp poweroffset full\r\n");
                }
                ret = mfg_media_write_hp_poweroffset_pre_with_lock(pwrOffsetTable, 0);
                if(ret){
                    mfg_print("hp poweroffset write error, ret %d\r\n",ret);
                }else{
                    mfg_print("hp poweroffset write suss\r\n");
                }
            }else if(data[2] == 'L'){
                if(!mfg_media_is_lp_poweroffset_slot_empty(1)){
                    mfg_print("lp poweroffset full\r\n");
                }
                ret = mfg_media_write_lp_poweroffset_pre_with_lock(pwrOffsetTable, 0);
                if(ret){
                    mfg_print("lp poweroffset write error, ret %d\r\n",ret);
                }else{
                    mfg_print("lp poweroffset write suss\r\n");
                }
            }

        } else if (data[1] == 'E') {
            int k = 0;
            int offset[20];
            int8_t BlepwrOffsetTable[20];
            memset(offset, 0, sizeof(offset));
            mfg_print("BLE Pwr offset cmd:");
            sscanf((char *)&data[2], "%d,%d,%d,%d,%d",
                   &offset[0], &offset[1], &offset[2], &offset[3], &offset[4]);
            for (k = 0; k < 5; k++) {
                /* Use 3 bits as signed value */
                if (offset[k] > MFG_PWR_OFFSET_MAX) {
                    offset[k] = MFG_PWR_OFFSET_MAX;
                }
                if (offset[k] < MFG_PWR_OFFSET_MIN) {
                    offset[k] = MFG_PWR_OFFSET_MIN;
                }
                BlepwrOffsetTable[k] = offset[k];
                mfg_print("%d ", BlepwrOffsetTable[k]);
            }
            mfg_print("\r\n");

            if(!mfg_media_is_bz_poweroffset_slot_empty(1)){
                mfg_print("bz poweroffset full\r\n");
            }

            ret = mfg_media_write_bz_poweroffset_pre_with_lock(BlepwrOffsetTable, 0);
            if (ret) {
                mfg_print("bz power offset write error,ret %d\r\n", ret);
            } else {
                mfg_print("bz power offset write suss\r\n");
            }
        } else if (data[1] == 'A') {
            //
        } else if (data[1] == 'M') {
            /* Read efuse mac address value */
            uint8_t mac[6] = { 0 };
            uint32_t mac_int[6] = { 0 };
            uint32_t i;
            uint8_t program = 0;
            uint8_t reload = 1;
            uint8_t slots_num;
            uint8_t mac_0_is_empty_without_wr_lock = 0;
            uint8_t mac_0_is_empty_with_wr_lock = 0;
            ef_ctrl_wr_lock_mac = 0;
            uint32_t tmpVal;
            int ret;
            slots_num = mfg_efuse_get_rf_cal_slots();
            if (slots_num >= 1 && qcc74x_efuse_is_mac_address_slot_empty(0, reload)) { // MAC Address 0 is All 0
                tmpVal = QCC74x_RD_WORD(0x2005607C);
                tmpVal = (tmpVal & (1 << 16));
                if (tmpVal) {
                    mac_0_is_empty_with_wr_lock = 1;
                    mfg_print("mac 0 is empty with wr lock \r\n");
                } else {
                    mac_0_is_empty_without_wr_lock = 1;
                    mfg_print("mac 0 is empty without wr lock \r\n");
                }
                if (mac_0_is_empty_without_wr_lock) {
                    memset(mac_int, 0, sizeof(mac_int));
                    mfg_print("MAC address cmd:");
                    sscanf((char *)&data[2], "%02x:%02x:%02x:%02x:%02x:%02x",
                    (unsigned int *)&mac_int[0], (unsigned int *)&mac_int[1], (unsigned int *)&mac_int[2],
                    (unsigned int *)&mac_int[3], (unsigned int *)&mac_int[4], (unsigned int *)&mac_int[5]);
                    for (i = 0; i < 6; i++) {
                        mac[i] = mac_int[i];
                        mfg_print("%02x ", mac[i]);
                    }
                    mfg_print("\r\n");
                    ret = qcc74x_efuse_write_mac_address_opt(0, mac, program);
                    ef_ctrl_wr_lock_mac = 1;
                } else if (mac_0_is_empty_with_wr_lock) {
                    memset(mac_int, 0, sizeof(mac_int));
                    mfg_print("MAC address cmd:");
                    sscanf((char *)&data[2], "%02x:%02x:%02x:%02x:%02x:%02x",
                    (unsigned int *)&mac_int[0], (unsigned int *)&mac_int[1], (unsigned int *)&mac_int[2],
                    (unsigned int *)&mac_int[3], (unsigned int *)&mac_int[4], (unsigned int *)&mac_int[5]);
                    for (i = 0; i < 6; i++) {
                        mac[i] = mac_int[i];
                        mfg_print("%02x ", mac[i]);
                    }
                    mfg_print("\r\n");
                    if (slots_num >= 2 && qcc74x_efuse_is_mac_address_slot_empty(1, reload)) {
                        ret = qcc74x_efuse_write_mac_address_opt(1, mac, program);
                        mfg_print("MAC addr slot 1\r\n");
                    } else if (slots_num >= 3 && qcc74x_efuse_is_mac_address_slot_empty(2, reload)) {
                        ret = qcc74x_efuse_write_mac_address_opt(2, mac, program);
                        mfg_print("MAC addr slot 2\r\n");
                    } else {
                    mfg_print("MAC addr full\r\n");
                    }
                }
            }
            // normal operation
            else if (!mfg_media_is_macaddr_slot_empty(1)) {
                mfg_print("MAC addr full\r\n");
            } else {
                /* Write efuse mac address value */
                uint8_t mac[6] = { 0 };
                uint32_t mac_int[6] = { 0 };
                uint32_t i;

                memset(mac_int, 0, sizeof(mac_int));
                mfg_print("MAC address cmd:");
                sscanf((char *)&data[2], "%02x:%02x:%02x:%02x:%02x:%02x",
                       (unsigned int *)&mac_int[0], (unsigned int *)&mac_int[1], (unsigned int *)&mac_int[2],
                       (unsigned int *)&mac_int[3], (unsigned int *)&mac_int[4], (unsigned int *)&mac_int[5]);
                for (i = 0; i < 6; i++) {
                    mac[i] = mac_int[i];
                    mfg_print("%02x ", mac[i]);
                }
                mfg_print("\r\n");
                mfg_media_write_macaddr_pre_with_lock(mac, 0);
            }
        }else if(data[1] == 'B'){
            uint32_t addr=0;
            char *p;
            uint8_t x;
            uint8_t y;
            uint8_t k = 0;
            uint8_t ef_buf[256];
            p=strchr((char*)&data[1],'=');

            if(p!=NULL){
                if(((strlen((char *)data) - 13) % 2 ) != 0){
                    mfg_print("param len error\r\n");
                    return 0;
                }
                sscanf((char *)&data[2],"0x%08x=",(unsigned int *)&addr);
                mfg_print("write to efuse addr %08x\r\n",(unsigned int)addr);
                mfg_print("len is %d\r\n",(strlen((char *)data) - 13)/2);

                memset(ef_buf,0,256);
                k = 0;
                for(int i = 13; i < strlen((char *)data) - 1; i+=2){
                    char2hex(data[i], &x);
                    char2hex(data[i+1], &y);
                    ef_buf[k++] = (x << 4) | y;
                }
                mfg_print("write to efuse data:\r\n");
                for(int i = 0; i < k; i++){
                    mfg_print("%02x ",ef_buf[i]);
                    if((i > 0) && (i % 16 == 15)){
                        mfg_print("\r\n");
                    }
                }
                mfg_print("\r\n");
                mfg_efuse_program(addr, (uint32_t *)ef_buf, (k % 4) == 0 ? (k / 4):(k / 4) + 1, 0);

            }else{
                mfg_print("formate should be 0x00000100=xxxxxxxxxxxx like\r\n");
            }
        }
    }

    return 0;
}

static int32_t mfg_cmd_read_efuse_do(uint8_t *data, uint16_t len, uint8_t efuse_reload)
{
    int32_t ret = 0;
    if (data[0] == 'E') {
        if (data[1] == 'X') {
            uint8_t capCode = 0;
            ret = mfg_media_read_xtal_capcode_with_lock(&capCode, efuse_reload);
            if (ret) {
                mfg_print("capcode golden read error,ret %d\r\n", ret);
            } else {
                mfg_print("capcode golden read suss,capcode %d\r\n", (int)capCode);
            }

        } else if (data[1] == 'P') {
            uint32_t k;
            int8_t pwrOffset[14];

            if(data[2] == 'H'){
                ret = mfg_media_read_hp_poweroffset_with_lock(pwrOffset, efuse_reload);
            }else if(data[2] == 'L'){
                ret = mfg_media_read_lp_poweroffset_with_lock(pwrOffset, efuse_reload);
            }
            if (!ret) {
                if(data[2] == 'H'){
                    mfg_print("hp Power offset:");
                }else if(data[2] == 'L'){
                    mfg_print("lp Power offset:");
                }

                for (k = 0; k < 14; k++) {
                    mfg_print("%d", pwrOffset[k]);
                    if (k < 13) {
                        mfg_print(",");
                    }
                }
                mfg_print("\r\n");
            } else {
                mfg_print("Read media err\r\n");
            }
        } else if (data[1] == 'A') {
        uint8_t buf1[4] = {0x12,0x34,0x56,0x78};
        mfg_print("%08x\r\n",*(uint32_t *)buf1);

        } else if (data[1] == 'E') {
            uint32_t k;
            int8_t BzpwrOffset[5];
            ret = mfg_media_read_bz_poweroffset_with_lock(BzpwrOffset, efuse_reload);
            if (!ret) {
                mfg_print("bz Power offset:");
                for (k = 0; k < 5; k++) {
                    mfg_print("%d", BzpwrOffset[k]);
                    if (k < 5) {
                        mfg_print(",");
                    }
                }
                mfg_print("\r\n");
            } else {
                mfg_print("Read media err\r\n");
            }
        } else if (data[1] == 'M') {
            uint8_t mac[6] = { 0 };
            uint32_t i = 0;
            /* Read efuse mac address value */
            if (mfg_media_read_macaddr_with_lock(mac, efuse_reload) == SUCCESS) {
                mfg_print("MAC:");
                for (i = 0; i < 6; i++) {
                    if (i != 5) {
                        mfg_print("%02x:", mac[i]);
                    } else {
                        mfg_print("%02x\r\n", mac[i]);
                    }
                }
            } else {
                mfg_print("Rd media err\r\n");
                mfg_print("Rtn default\r\n");
                mfg_print("MAC:");
                mac[0] = 0x00;
                mac[1] = 0x11;
                mac[2] = 0x22;
                mac[3] = 0x33;
                mac[4] = 0x44;
                mac[5] = 0x55;
                for (i = 0; i < 6; i++) {
                    if (i != 5) {
                        mfg_print("%02x:", mac[i]);
                    } else {
                        mfg_print("%02x\r\n", mac[i]);
                    }
                }
            }
        }else if(data[1] == 'B'){
            uint32_t addr=0;
            uint32_t len=0;
            uint8_t ef_buf[256];
            sscanf((char *)&data[2],"0x%08x",(unsigned int *)&addr);
            if(data[12] == 'L'){
                sscanf((char *)&data[13],"0x%02x",&len);
                mfg_efuse_read(addr,(uint32_t *)ef_buf,len/4==0?len/4:len/4+1,efuse_reload);
                mfg_print("read efuse addr 0x%08x\r\n",(unsigned int)addr);
                mfg_print("len is %d\r\n",len);
                mfg_print("read efuse data:\r\n");
                for(int i = 0; i < len; i++){
                    mfg_print("%02x ",ef_buf[i]);
                    if((i > 0) && (i % 16 == 15)){
                        mfg_print("\r\n");
                    }
                }
                mfg_print("\r\n");

            }else{
                mfg_print("formate should be 0x00000100L0x20 like\r\n");
            }

        }
    } else if (data[0] == 'F') {
    } else if (data[0] == 'M') {
        uint32_t addr = 0;
        sscanf((char *)&data[1], "0x%08x", (unsigned int *)&addr);
        if (addr != 0) {
            mfg_print("0x%08x=0x%08x\r\n", (unsigned int)addr, *((unsigned int *)addr));
        } else {
            mfg_print("Address is 0\r\n");
        }
    } else if (data[0] == 'B') {
        if (data[1] == 'M') {
            uint8_t b_mac[6];
            EF_Ctrl_Read_MAC_Address( b_mac );
            mfg_print("MAC:");
            for (uint8_t i = 0; i < 6; i++) {
                if (i != 5) {
                    mfg_print("%02x:", b_mac[i]);
                } else {
                    mfg_print("%02x\r\n", b_mac[i]);
                }
            }
        } else {
            mfg_print("Read mac err\r\n");
        }
    }

    return 0;
}

static int32_t mfg_cmd_load_efuse(uint8_t *data, uint16_t len)
{
    mfg_cmd_read_efuse_do(data, len, 0);

    return 0;
}

static int32_t mfg_cmd_read_efuse(uint8_t *data, uint16_t len)
{
    mfg_cmd_read_efuse_do(data, len, 1);

    return 0;
}


static int32_t mfg_cmd_save_efuse(uint8_t *data, uint16_t len)
{
    if (data[0] == 'E') {
        if (data[1] == 'X') {
            mfg_media_write_xtal_capcode_with_lock();
            mfg_print("Save cap code OK\r\n");
        } else if (data[1] == 'P') {
            if(data[2] == 'H'){
                mfg_media_write_hp_poweroffset_with_lock();
                mfg_print("Save hp power offset OK\r\n");
            }else if(data[2] == 'L'){
                mfg_media_write_lp_poweroffset_with_lock();
                mfg_print("Save lp power offset OK\r\n");
            }
        } else if (data[1] == 'A') {
            //if(0==mfg_efuse_program()){
            //mfg_print("Save efuse OK\r\n");
            //}else{
            //mfg_print("Save efuse fail\r\n");
            //}
        } else if (data[1] == 'M') {
            mfg_media_write_macaddr_with_lock();
            mfg_print("Save MAC address OK\r\n");
            if (ef_ctrl_wr_lock_mac) {
                mfg_print("Set MAC address locked\r\n");
                EF_Ctrl_Writelock_MAC_Address(1);
            }
        } else if (data[1] == 'E') {
            mfg_media_write_bz_poweroffset_need_lock();
            mfg_print("Save bz power offset ok\r\n");
        } else if(data[1] == 'B'){
            mfg_efuse_program(0, NULL, 0, 1);
        }
    } else if (data[0] == 'F') {
    } else if (data[0] == 'M') {
    }
    return 0;
}


#if defined(MFG_GU_QCC743) || defined(MFG_GU_QCC74x_undef)

static int32_t mfg_cmd_hbn(uint8_t *data, uint16_t len)
{
    return 0;
}

static int32_t mfg_cmd_sleep(uint8_t *data, uint16_t len)
{
    uint32_t pds_slp_msec;
    static int pds_mod=15;

    if (data[0] == 'l' || data[0] == 'L') {
        pds_mod=atoi((const char*)&data[1]);
        mfg_print("pds mode=%d\r\n",pds_mod);
    }else if (data[0] == 'A' || data[0] == 'a') {
        //TODO
        //ADC_Disable();
        L1C_DCache_Clean_Invalid_All();
        pm_pds_mask_all_wakeup_src();
        pm_pds_mode_enter(pds_mod,0xffffffff);
    } else if (data[0] == ':') {
        uint32_t dtim =  data[1] - '0';
        uint32_t exit_cnt = atoi((const char*)&data[2]);
        mfg_print("%d,%d\r\n", (unsigned int)dtim, (unsigned int)exit_cnt);
        mfg_print("command not support yet\r\n");
    } else {
        pds_slp_msec= atoi((const char*)&data[0]);
        //mfg_board_enter_pds(pds_slp_msec << 5);
        //TODO
        //ADC_Disable();
        L1C_DCache_Clean_Invalid_All();
        pm_pds_mask_all_wakeup_src();
        pm_pds_mode_enter(pds_mod,pds_slp_msec << 5);
    }
    return 0;
}

static int32_t mfg_cmd_wakeup(uint8_t *data, uint16_t len)
{
    return 0;
}

static int32_t mfg_cmd_para_set(uint8_t *data, uint16_t len)
{
    unsigned int tcal_val_fix_tmp=0;
    unsigned int tcal_debug_tmp=0;
    int tcal_val_fix_value_tmp=0;
    unsigned int mac1,mac2,mac3,mac4,mac5,mac6;
    uint8_t mac[6];
    uint8_t i=0;

    if(data[0]=='t'){
        sscanf((char*)&data[1],"%d,%d,%d",&tcal_debug_tmp,&tcal_val_fix_tmp,&tcal_val_fix_value_tmp);
        tcal_debug=tcal_debug_tmp;
        tcal_val_fix=tcal_val_fix_tmp;
        tcal_val_fix_value=tcal_val_fix_value_tmp;

        mfg_print("TCal debug=%d,fixed temp=%d,fixed temp val=%d\r\n",tcal_debug,tcal_val_fix,tcal_val_fix_value);
    }
    if(data[0]=='m'){
#if 0
        sscanf((char*)&data[1],"%02x:%02x:%02x:%02x:%02x:%02x",&mac1,&mac2,&mac3,&mac4,&mac5,&mac6);
        mac[0]=mac1;mac[1]=mac2;mac[2]=mac3;mac[3]=mac4;mac[4]=mac5;mac[5]=mac6;
        cci_paras_env.unicast_enable=1;
        memcpy(&cci_paras_env.peer_mac,mac,6);
        for(i=0;i<6;i++){
            if(i==5){
                mfg_print("%02x\r\n",mac[i]);
            }else{
                mfg_print("%02x:",mac[i]);
            }
        }
#endif
    }
    return 0;
}

static int32_t mfg_cmd_cfo_correct(uint8_t *data, uint16_t len)
{
    int64_t freq_offset_Hz = 0;
    double dphi = 0;
    uint32_t reg_value = 0;
    if(data[0] == 'F'){
        QCC74x_WR_WORD(0x24c00844, 0x10000000);
    }else{
        freq_offset_Hz = atoi((const char*)&data[2]);
        mfg_print("input freq_offset_Hz %d\r\n",(int)freq_offset_Hz);
        //g_print("input freq_offset_Hz %d\r\n",(int)(freq_offset_Hz >> 32));
        if(data[0] == '0'){
            mfg_print("calc 20MHz\r\n");
            dphi = (double)(freq_offset_Hz << 24) / (float)20000000;
        }else if(data[0] == '1'){
            mfg_print("calc 40MHz\r\n");
            dphi = (double)(freq_offset_Hz << 24) / (float)40000000;
        }
        if(freq_offset_Hz < 0){
            reg_value = 0x01ffffff & (int)dphi;
        }else{
            reg_value = 0x00ffffff & (int)dphi;
        }
        reg_value |= (0x1 << 30);
        reg_value &= ~(0x1 << 29);
        reg_value &= ~(0x1 << 28);
        QCC74x_WR_WORD(0x24c00844, reg_value);

    }
    mfg_print("read 0x24c00844=0x%08x\r\n",QCC74x_RD_WORD(0x24c00844));
    return 0;
}

#endif


int16_t mfg_board_tsen_get_val(void)
{

    //return (int16_t)TSEN_Get_Temp(tsen_offset);
    return (int16_t)qcc74x_adc_tsen_get_temp(adc);
}

int32_t mfg_board_get_average_temp()
{
#define SAMPLE_COUNT        10

    int32_t total=0;
    int16_t tmp[SAMPLE_COUNT];
    int16_t max=-1000;
    int16_t min=1000;
    uint32_t i=0;
    int16_t average=0;

    for(i=0;i<SAMPLE_COUNT;i++){
        tmp[i]=mfg_board_tsen_get_val();
        vTaskDelay(200);
    }
    for(i=0;i<SAMPLE_COUNT;i++){
        if(max<tmp[i]){
            max=tmp[i];
        }
        if(min>tmp[i]){
            min=tmp[i];
        }
    }
    for(i=0;i<SAMPLE_COUNT;i++){
        total+=tmp[i];
    }
    if(max==-1000||min==1000){
        average=total/SAMPLE_COUNT;
        //mfg_print("Temp sample maybe uncorrect\r\n");
        for(i=0;i<SAMPLE_COUNT;i++){
            mfg_print("%d,",(int)tmp[i]);
            mfg_print("\r\n");
        }
    }else{
        total-=(max+min);
        average=total/(SAMPLE_COUNT-2);
    }
    return average;
}

static int32_t mfg_cmd_get_info(uint8_t *data, uint16_t len)
{
    extern qcc74xverinf_t app_ver;

    if (data[0] == ':') {
        switch (data[1]) {
            case 'i': {
                mfg_print("#*#*");
                mfg_print("duty:%d\r\n", (unsigned int)g_mfg_tx_para.tx_duty);
            } break;
            case 'v': {
                mfg_print("#*#*");
                /* mfg_print("%s\r\n", PROJECT_SDK_VERSION); */
                mfg_print("version:%d.%d.%d\r\n",(int)app_ver.x,(int)app_ver.y,(int)app_ver.z);
                /* mfg_print("        %d %d\r\n",(int)app_ver.rsvd0,(int)app_ver.rsvd1); */
            } break;
            case 'd': {
                mfg_print("#*#*");
                mfg_print("date:%s time:%s\r\n", __DATE__, __TIME__);
            } break;
            case 'p': {
                mfg_print("#*#*");
                mfg_print("power:%d\r\n", (unsigned int)mfg_get_tx_power());
            } break;
            case 'c': {
                mfg_print("#*#*");
                mfg_print("channel:%d\r\n", (unsigned int)mfg_get_channel());
            } break;
            case 't': {
                mfg_print("#*#*");
                mfg_print("tx:%d\r\n", (unsigned int)g_mfg_tx_para.tx_on);
            } break;
            case 'f': {
                mfg_print("#*#*");
                mfg_print("freq:%d\r\n", (unsigned int)g_mfg_tx_para.frequency);
            } break;
            case 'x': {
                mfg_print("#*#*");
                mfg_print("capcode:%d\r\n", (unsigned int)AON_Get_Xtal_CapCode());
            } break;
            case 'M': {
                mfg_print("#*#*");
                mfg_print("mfgmode:%d\r\n", (unsigned int)g_mfg_tx_para.mfgmode);
            } break;
            case 'K': {
                mfg_print("Temperature: %d degree Celsius\r\n", mfg_board_get_average_temp());
            } break;
            default:
                break;
        }
    }
    return 0;
}


static int32_t mfg_cmd_set_cap_code(uint8_t *data, uint16_t len)
{
    struct wl_cfg_t * rfparam_cfg = rfparam_cfg_get();
    if(rfparam_cfg->param.tcap.en_tcap){
        mfg_print("en temp-capcode mode, x cmd will not work\r\n");
        return 0;
    }
    int capcode;
    capcode = atoi((char *)&data[0]);
    if((capcode > 0) && (capcode <= 255)){
        AON_Set_Xtal_CapCode((uint8_t)capcode,(uint8_t)capcode);
        mfg_print("capcode %d\r\n",capcode);
    }else if(-1 == capcode){
        rfparam_get_cap_code_with_option(rfparam_tlv_base_addr_get(), &rfparam_cfg->param.xtalcapcode_in, &rfparam_cfg->param.xtalcapcode_out,1);
        AON_Set_Xtal_CapCode((uint8_t)rfparam_cfg->param.xtalcapcode_in,(uint8_t)rfparam_cfg->param.xtalcapcode_out);
    }else if(-2 == capcode){
        rfparam_get_cap_code_with_option(rfparam_tlv_base_addr_get(), &rfparam_cfg->param.xtalcapcode_in, &rfparam_cfg->param.xtalcapcode_out,0);
        AON_Set_Xtal_CapCode((uint8_t)rfparam_cfg->param.xtalcapcode_in,(uint8_t)rfparam_cfg->param.xtalcapcode_out);
    }

    return 0;
}

static int32_t mfg_cmd_set_tx_duty(uint8_t *data, uint16_t len)
{
    mfg_print("duty:%s%%\r\n", data);
    if((data[0] == '1') && (data[1] == '0') && (data[2] == '0')){
        g_mfg_tx_para.tx_duty = 100;
    }else if((data[0] == '1') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 10;
    }else if((data[0] == '2') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 20;
    }else if((data[0] == '3') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 30;
    }else if((data[0] == '4') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 40;
    }else if((data[0] == '5') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 50;
    }else if((data[0] == '6') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 60;
    }else if((data[0] == '7') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 70;
    }else if((data[0] == '8') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 80;
    }else if((data[0] == '9') && (data[1] == '0')){
        g_mfg_tx_para.tx_duty = 90;
    }else{
        g_mfg_tx_para.tx_duty = 0;
    }

    return 0;
}


#if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
extern int btblecontroller_putchar(int c);

static int32_t mfg_cmd_btble_hci_test(uint8_t *data, uint16_t len)
{
    uint8_t unknow_hci_cmd[7] = { 0x04, 0x0e, 0x04, 0x05, 0x00, 0x00, 0x01 };
    uint16_t opcode = data[1] << 8 | data[0];
    #if defined(CONFIG_BLE_MFG_HCI_CMD)
    struct hci_le_rx_test_v3_cmd rx_v3_param;
    struct hci_le_rx_test_v2_cmd rx_param;
    struct hci_le_rx_test_v1_cmd rx_v1_param;
    struct hci_le_tx_test_v4_cmd tx_param;
    struct hci_le_tx_test_v3_cmd tx_v3_param;
    struct hci_le_tx_test_v2_cmd tx_v2_param;
    struct hci_le_tx_test_v1_cmd tx_v1_param;
    #endif
    #if defined(CONFIG_BT_MFG_HCI_CMD)
    struct hci_vs_rx_test_cmd bt_rx_param;
    struct hci_vs_tx_test_cmd bt_tx_param;
    #endif

    //Notice:in hci command format, data[2] is len of payload.
    if (opcode == HCI_RESET){
        reset_cmd_handler();
    }
    #if defined(CONFIG_BLE_MFG_HCI_CMD)
    else if (opcode == LE_RX_TEST_CMD_V3_OPCODE) {
        rx_v3_param.rx_channel = data[3];
        rx_v3_param.phy = data[4];
        rx_v3_param.mod_idx = data[5];
        rx_v3_param.exp_cte_len = data[6];
        rx_v3_param.exp_cte_type = data[7];
        rx_v3_param.slot_dur = data[8];
        rx_v3_param.switching_pattern_len = data[9];
        memcpy(&rx_v3_param.antenna_id, &data[10], rx_v3_param.switching_pattern_len);
        hci_le_rx_test_v3_cmd_handler(&rx_v3_param, opcode, true);
    } else if (opcode == LE_RX_TEST_CMD_OPCODE) {
        rx_param.rx_channel = data[3];
        rx_param.phy = data[4];
        rx_param.mod_idx = data[5];
        hci_le_rx_test_v2_cmd_handler(&rx_param, opcode, true);
    } else if (opcode == LE_RX_TEST_CMD_V1_OPCODE){
        rx_v1_param.rx_channel = data[3];
        hci_le_rx_test_v1_cmd_handler(&rx_v1_param, opcode, true);
    } else if (opcode == LE_TX_TEST_CMD_OPCODE) {
        tx_param.tx_channel = data[3];
        tx_param.test_data_len = data[4];
        tx_param.pkt_payl = data[5];
        tx_param.phy = data[6];
        tx_param.cte_len = data[7];
        tx_param.cte_type = data[8];
        tx_param.switching_pattern_len = data[9];
        memcpy(&tx_param.antenna_id, &data[10], tx_param.switching_pattern_len);
        tx_param.tx_pwr_lvl = data[10 + tx_param.switching_pattern_len];
        hci_le_tx_test_v4_cmd_handler(&tx_param, opcode, true);
    } else if (opcode == LE_TX_TEST_CMD_V3_OPCODE){
        struct wl_cfg_t * rfparam_cfg = rfparam_cfg_get();
        int8_t power = rfparam_cfg->param.pwrtarget.pwr_ble;
        ble_controller_set_tx_pwr(power);
        tx_v3_param.tx_channel = data[3];
        tx_v3_param.test_data_len = data[4];
        tx_v3_param.pkt_payl = data[5];
        tx_v3_param.phy = data[6];
        tx_v3_param.cte_len = data[7];
        tx_v3_param.cte_type = data[8];
        tx_v3_param.switching_pattern_len = data[9];
        memcpy(&tx_v3_param.antenna_id, &data[10], tx_v3_param.switching_pattern_len);
        hci_le_tx_test_v3_cmd_handler(&tx_v3_param, opcode, true);
    } else if (opcode == LE_TX_TEST_CMD_V2_OPCODE){
        struct wl_cfg_t * rfparam_cfg = rfparam_cfg_get();
        int8_t power = rfparam_cfg->param.pwrtarget.pwr_ble;
        ble_controller_set_tx_pwr(power);
        tx_v2_param.tx_channel = data[3];
        tx_v2_param.test_data_len = data[4];
        tx_v2_param.pkt_payl = data[5];
        tx_v2_param.phy = data[6];
        hci_le_tx_test_v2_cmd_handler(&tx_v2_param, opcode, true);
    } else if (opcode == LE_TX_TEST_CMD_V1_OPCODE){
        struct wl_cfg_t * rfparam_cfg = rfparam_cfg_get();
        int8_t power = rfparam_cfg->param.pwrtarget.pwr_ble;
        ble_controller_set_tx_pwr(power);
        tx_v1_param.tx_channel = data[3];
        tx_v1_param.test_data_len = data[4];
        tx_v1_param.pkt_payl = data[5];
        hci_le_tx_test_v1_cmd_handler(&tx_v1_param, opcode, true);
    } else if (opcode == LE_TEST_END_CMD_OPCODE) {
        hci_le_test_end_cmd_handler(NULL, LE_TEST_END_CMD_OPCODE, true);
    }
    #endif
    #if defined(CONFIG_BT_MFG_HCI_CMD)
    else if (opcode == BT_RX_TEST_CMD_OPCODE) {
        bt_rx_param.rx_channel = data[3];
        bt_rx_param.pkt_type = data[4];
        hci_vs_rx_test_cmd_handler(&bt_rx_param, opcode, true);
    } else if (opcode == BT_TX_TEST_CMD_OPCODE) {
        bt_tx_param.tx_channel = data[3];
        bt_tx_param.test_data_len = data[5] << 8 | data[4];
        bt_tx_param.pkt_payl = data[6];
        bt_tx_param.pkt_type = data[7];
        bt_tx_param.tx_pwr_lvl = data[8];
        hci_vs_tx_test_cmd_handler(&bt_tx_param, opcode, true);

    } else if (opcode == BT_TEST_END_CMD_OPCODE){
        hci_vs_test_end_cmd_handler(NULL, BT_TEST_END_CMD_OPCODE, true);
    }
    #endif
    else {
        unknow_hci_cmd[4] = data[0];
        unknow_hci_cmd[5] = data[1];
        for (int i = 0; i < 7; i++)
            btblecontroller_putchar(unknow_hci_cmd[i]);
    }

    return 0;
}
#endif //#if defined(CONFIG_BLE_MFG_HCI_CMD)


/*partiton need this*/
//void main(void)
//{

//}

#if 0
void ADC_Clock_Init(uint8_t div)
{
    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_GPIP);
    GLB_Set_ADC_CLK(ENABLE, GLB_ADC_CLK_XCLK, div);
    mfg_print("\nGLB_Set_ADC_CLK_Div(%d) == clock/(%d+1)\r\n", div, div);
    mfg_print("ADC clock is %d\r\n", Clock_Peripheral_Clock_Get(QCC74x_PERIPHERAL_CLOCK_GPADC));
}
void mfg_board_tsen_init()
{
    ADC_CFG_Type adcCfg = {
    .v18Sel = ADC_V18_SEL_1P82V,                    /*!< ADC 1.8V select */
    .v11Sel = ADC_V11_SEL_1P1V,                     /*!< ADC 1.1V select */
    .clkDiv = ADC_CLK_DIV_4,                       /*!< Clock divider */
    .gain1 = ADC_PGA_GAIN_1,                        /*!< PGA gain 1 */
    .gain2 = ADC_PGA_GAIN_1,                        /*!< PGA gain 2 */
    .chopMode = ADC_CHOP_MOD_AZ_PGA_ON,             /*!< ADC chop mode select */
    .biasSel = ADC_BIAS_SEL_MAIN_BANDGAP,           /*!< ADC current form main bandgap or aon bandgap */
    .vcm = ADC_PGA_VCM_1V,                          /*!< ADC VCM value */
    .vref = ADC_VREF_2P0V,                          /*!< ADC voltage reference */
    .inputMode = ADC_INPUT_SINGLE_END,              /*!< ADC input signal type */
    .resWidth = ADC_DATA_WIDTH_16_WITH_256_AVERAGE, /*!< ADC resolution and oversample rate */
    .offsetCalibEn = 0,                             /*!< Offset calibration enable */
    .offsetCalibVal = 0,                            /*!< Offset calibration value */
    };

    ADC_FIFO_Cfg_Type adcFifoCfg = {
        .fifoThreshold = ADC_FIFO_THRESHOLD_1,
        .dmaEn = DISABLE,
    };


    ADC_Clock_Init(4);
    ADC_Disable();
    ADC_Enable();

    ADC_Reset();

    ADC_Init(&adcCfg);
    ADC_Channel_Config(ADC_CHAN_TSEN_P, ADC_CHAN_GND, 0);
    ADC_Tsen_Init(ADC_TSEN_MOD_INTERNAL_DIODE);

    ADC_FIFO_Cfg(&adcFifoCfg);

    if(ADC_Trim_TSEN((uint16_t *)&tsen_offset) == ERROR){
        tsen_offset = 2010;
        //mfg_print("No TSEN Trim found\r\n");
    }else{
        //mfg_print("TSEN Trim found in efuse\r\n");
    }

}
#endif


void mfg_temp_trim(void *pvParameters)
{
    int16_t temperature=25;
    volatile uint8_t tcal_recovery=0;
    volatile uint8_t tcal_init=0;

    vTaskDelay(200);
    //mfg_board_tsen_init();

    //open clk
    PERIPHERAL_CLOCK_ADC_DAC_ENABLE();
    GLB_Set_ADC_CLK(ENABLE, GLB_ADC_CLK_XCLK, 1);

    adc = qcc74x_device_get_by_name("adc");
    /* adc clock = XCLK / 2 / 32 */
    struct qcc74x_adc_config_s adc_cfg;
    adc_cfg.clk_div = ADC_CLK_DIV_32;
    adc_cfg.scan_conv_mode = false;
    adc_cfg.continuous_conv_mode = false;
    adc_cfg.differential_mode = false;
    adc_cfg.resolution = ADC_RESOLUTION_16B;
    adc_cfg.vref = ADC_VREF_2P0V;

    struct qcc74x_adc_channel_s chan;
    chan.pos_chan = ADC_CHANNEL_TSEN_P;
    chan.neg_chan = ADC_CHANNEL_GND;
    qcc74x_adc_init(adc, &adc_cfg);
    qcc74x_adc_channel_config(adc, &chan, 1);
    qcc74x_adc_tsen_init(adc, ADC_TSEN_MOD_INTERNAL_DIODE);

    struct wl_cfg_t *cfg = rfparam_cfg_get();
    if(cfg->param.tcal.en_tcal){
        tcal_enable = cfg->param.tcal.en_tcal;
    }

    mfg_print("mfg temp-power trim :%d,temp-capcode trim :%d\r\n",tcal_enable,cfg->param.tcap.en_tcap);

    extern EventGroupHandle_t xEventGroupMFG;
    xEventGroupSetBits( xEventGroupMFG, (1 << 0) );
    // mfg_print("xEventGroupSetBit 0\r\n");

    while (1) {
        if(g_mfg_tx_para.mfgmode == 1){
            // mfg_print("duty100 temp tirm not work\r\n");
            vTaskDelay(2000);
            continue;
        }
        if(tcal_val_fix==0){
            temperature=mfg_board_get_average_temp();
        }else{
            vTaskDelay(2000);
            temperature=tcal_val_fix_value;
        }
        if(tcal_debug){
            mfg_print("Temperature:%d\r\n",temperature);
        }

        if(tcal_enable){
            /*Do tcal*/
            if(temperature>=-100&&temperature<=250){
                wl_rf_tcal_handler(temperature);
                if(tcal_debug){
                    mfg_print("Do temp trim\r\n");
                }

            }else{
                mfg_print("Not do tcal:%d\r\n",(int)temperature);
            }
        }
#if 0
        else{
            /* Do a recovery to normal*/
            if(tcal_recovery==0){
                wl_rf_tcal_handler(35);
                tcal_recovery=1;
            }
        }
#endif

        if(cfg->param.tcap.en_tcap){
            if(temperature < cfg->param.tcap.tcap_tsen[0]){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[0],cfg->param.tcap.tcap_cap[0]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[0]);
            }else if((cfg->param.tcap.tcap_tsen[0] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[1])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[1],cfg->param.tcap.tcap_cap[1]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[1]);
            }else if((cfg->param.tcap.tcap_tsen[1] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[2])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[2],cfg->param.tcap.tcap_cap[2]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[2]);
            }else if((cfg->param.tcap.tcap_tsen[2] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[3])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[3],cfg->param.tcap.tcap_cap[3]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[3]);
            }else if((cfg->param.tcap.tcap_tsen[3] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[4])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[4],cfg->param.tcap.tcap_cap[4]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[4]);
            }else if((cfg->param.tcap.tcap_tsen[4] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[5])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[5],cfg->param.tcap.tcap_cap[5]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[5]);
            }else if((cfg->param.tcap.tcap_tsen[5] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[6])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[6],cfg->param.tcap.tcap_cap[6]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[6]);
            }else if((cfg->param.tcap.tcap_tsen[6] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[7])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[7],cfg->param.tcap.tcap_cap[7]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[7]);
            }else if((cfg->param.tcap.tcap_tsen[7] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[8])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[8],cfg->param.tcap.tcap_cap[8]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[8]);
            }else if((cfg->param.tcap.tcap_tsen[8] <= temperature) && (temperature < cfg->param.tcap.tcap_tsen[9])){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[9],cfg->param.tcap.tcap_cap[9]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[9]);
            }else if(cfg->param.tcap.tcap_tsen[9] <= temperature ){
                AON_Set_Xtal_CapCode(cfg->param.tcap.tcap_cap[10],cfg->param.tcap.tcap_cap[10]);
                mfg_print("Temperature:%d,capcode %d\r\n",temperature,cfg->param.tcap.tcap_cap[10]);
            }

        }

        vTaskDelay(MFG_TEMP_COMPENSATION_PERIOD-2000);
    }
}

#ifdef MFG_HTOL
static void qcc74x_mfg_htol_mod_sel(uint8_t *mod, uint8_t *mcs, uint16_t *len)
{
    static uint8_t mod_sel = 0;
    static uint8_t mcs_ax = 0;
    static uint8_t mcs_b = 0;

    mod_sel %= 2;
    *mod = mod_sel;
    if (mod_sel == 0) {
        mcs_ax %= 10;
        *mcs = mcs_ax;
        mcs_ax++;
        *len = 4096;
    } else if (mod_sel == 1) {
        mcs_b %= 4;
        *mcs = mcs_b + 1;
        mcs_b++;
        if(*mcs == 0){
            *len = 101;
        }else if(*mcs == 1){
            *len = 202;
        }else if(*mcs == 2){
            *len = 556;
        }else if(*mcs == 3){
            *len = 1111;
        }
    }
    mod_sel++;
}

void mfg_rx_start(void)
{
    uint8_t data[10];
    memset(data, 0, sizeof(data));
    memcpy(data, "r:s", strlen("r:s"));
    xQueueSendToBack(Message_Queue, data, 0);
}

void mfg_rx_stop(void)
{
    uint8_t data[10];
    memset(data, 0, sizeof(data));
    memcpy(data, "r:g", strlen("r:g"));
    xQueueSendToBack(Message_Queue, data, 0);
}

void mfg_channel_switch(uint8_t chan)
{
    uint8_t data[10];
    memset(data, 0, sizeof(data));
    data[0] = 'c';
    itoa(chan, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);
}


void mfg_tx11ax_start_raw(uint8_t mcs, uint16_t len, uint8_t power)
{
    uint8_t data[10];

    /* V0 */
    //memset(data,0,sizeof(data));
    //memcpy(data,"V0",strlen("V0"));
    //xQueueSendToBack( Message_Queue, data, 0 );

    /* p16 */
    memset(data, 0, sizeof(data));
    data[0] = 'p';
    itoa(power, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);

    /* d50 */
    memset(data, 0, sizeof(data));
    memcpy(data, "d50", strlen("d50"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* l767 */
    memset(data, 0, sizeof(data));
    data[0] = 'l';
    itoa(len, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);

    /* f1000 */
    memset(data, 0, sizeof(data));
    memcpy(data, "f1000", strlen("f1000"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* x0 */
    memset(data, 0, sizeof(data));
    memcpy(data, "x0", strlen("x0"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* mlm2+mcs */
    memset(data, 0, sizeof(data));
    memcpy(data, "Q000", strlen("mlm2"));
    data[4] = mcs + 0x30;
    xQueueSendToBack(Message_Queue, data, 0);

    /* t1 */
    memset(data, 0, sizeof(data));
    memcpy(data, "t1", strlen("t1"));
    xQueueSendToBack(Message_Queue, data, 0);
}


void mfg_tx11n_start_raw(uint8_t mcs, uint16_t len, uint8_t power)
{
    uint8_t data[10];

    /* V0 */
    //memset(data,0,sizeof(data));
    //memcpy(data,"V0",strlen("V0"));
    //xQueueSendToBack( Message_Queue, data, 0 );

    /* p16 */
    memset(data, 0, sizeof(data));
    data[0] = 'p';
    itoa(power, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);

    /* d50 */
    memset(data, 0, sizeof(data));
    memcpy(data, "d50", strlen("d50"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* l767 */
    memset(data, 0, sizeof(data));
    data[0] = 'l';
    itoa(len, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);

    /* f1000 */
    memset(data, 0, sizeof(data));
    memcpy(data, "f1000", strlen("f1000"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* x0 */
    memset(data, 0, sizeof(data));
    memcpy(data, "x0", strlen("x0"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* mlm2+mcs */
    memset(data, 0, sizeof(data));
    memcpy(data, "mlm2", strlen("mlm2"));
    data[4] = mcs + 0x30;
    xQueueSendToBack(Message_Queue, data, 0);

    /* t1 */
    memset(data, 0, sizeof(data));
    memcpy(data, "t1", strlen("t1"));
    xQueueSendToBack(Message_Queue, data, 0);
}

void mfg_tx11b_start_raw(uint8_t mcs, uint16_t len, uint8_t power)
{
    uint8_t data[10];

    /* V0 */
    //memset(data,0,sizeof(data));
    //memcpy(data,"V0",strlen("V0"));
    //xQueueSendToBack( Message_Queue, data, 0 );

    /* p16 */
    memset(data, 0, sizeof(data));
    data[0] = 'p';
    itoa(power, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);

    /* d50 */
    memset(data, 0, sizeof(data));
    memcpy(data, "d50", strlen("d50"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* l202 */
    memset(data, 0, sizeof(data));
    data[0] = 'l';
    itoa(len, (char *)&data[1], 10);
    xQueueSendToBack(Message_Queue, data, 0);

    /* f500 */
    memset(data, 0, sizeof(data));
    memcpy(data, "f500", strlen("f500"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* x0 */
    memset(data, 0, sizeof(data));
    memcpy(data, "x0", strlen("x0"));
    xQueueSendToBack(Message_Queue, data, 0);

    /* B+mcs */
    memset(data, 0, sizeof(data));
    data[0] = 'B';
    data[1] = mcs + 0x30;
    xQueueSendToBack(Message_Queue, data, 0);

    /* t1 */
    memset(data, 0, sizeof(data));
    memcpy(data, "t1", strlen("t1"));
    xQueueSendToBack(Message_Queue, data, 0);
}

void mfg_tx_stop(void)
{
    uint8_t data[10];

    /* t0 */
    memset(data, 0, sizeof(data));
    memcpy(data, "t0", strlen("t0"));
    xQueueSendToBack(Message_Queue, data, 0);
}
void qcc74x_mfg_htofw_entry(void *pvParameters)
{
    static uint32_t led_on = 0;
    static uint8_t chan_no = 1;
    static uint32_t htolfw_running_cnt = 1;
    uint8_t mod_sel = 0;
    uint8_t mcs = 0;
    uint16_t len = QCC74x_HTOL_TX_FRAME_LEN;
    uint8_t count = 0;
    uint8_t mbist_count = 0;

    GLB_GPIO_Cfg_Type cfg;

    cfg.drive = 1;
    cfg.smtCtrl = 1;
    cfg.gpioFun = GPIO_FUN_GPIO;
    cfg.gpioPin = GLB_GPIO_PIN_30;
    cfg.gpioMode = GPIO_MODE_OUTPUT;
    cfg.pullType = GPIO_PULL_NONE;
    cfg.outputMode = 0;
    GLB_GPIO_Init(&cfg);
    GLB_GPIO_Write(GLB_GPIO_PIN_30, 0);

    vTaskDelay(500);

    while (1) {
        //rf calibratin entry
        if (0 == (htolfw_running_cnt % QCC74x_HTOL_RFCAL_LOOP_CNT)) {
            //mfg_rf_cal();
            htolfw_running_cnt = 1;
        }
        htolfw_running_cnt++;

        //chan switch
        chan_no %= QCC74x_HTOL_CH_NUM;
        mfg_channel_switch(chan_no);



        //tx_start
        qcc74x_mfg_htol_mod_sel(&mod_sel, &mcs, &len);
        mfg_print("htol mod sel %u, %u, %u\r\n", mod_sel, mcs, len);
        if (mod_sel == 0) {
            mfg_tx11ax_start_raw(mcs, len, QCC74x_HTOL_TX_11AX_POWER);
        } else if (mod_sel == 1) {
            mfg_tx11b_start_raw(mcs, len, QCC74x_HTOL_TX_11B_POWER);
        }
        vTaskDelay(QCC74x_HTOL_TX_DUR_MS);
        mfg_tx_stop();

        //rx_start
        mfg_rx_start();
        vTaskDelay(QCC74x_HTOL_RX_DUR_MS);
        mfg_rx_stop();
        //qcc74x_wdt_feed();
        struct qcc74x_device_s *wdg;
        wdg = qcc74x_device_get_by_name("watchdog");
        qcc74x_wdg_reset_countervalue(wdg);
        chan_no++;
        count++;
        if(count % 4 == 0){
            count = 0;
            if (led_on) {
                led_on = 0;
            } else {
                led_on = 1;
            }
            GLB_GPIO_Write(GLB_GPIO_PIN_30, led_on);
            mbist_count++;
            if(mbist_count == 30){
                mbist_count = 0;
                mfg_print("mbist test\r\n");
                QCC74x_WR_WORD(0x20000300,0x0);
                QCC74x_WR_WORD(0x20000304,0x0);
                QCC74x_WR_WORD(0x20001480,0x0);
                QCC74x_WR_WORD(0x20009200,0x1);
                QCC74x_WR_WORD(0x20050100,0x0);

                QCC74x_WR_WORD(0x20000300,0x1ff);
                QCC74x_WR_WORD(0x20000304,0x101);
                QCC74x_WR_WORD(0x20001480,0x3);
                QCC74x_WR_WORD(0x20009200,0x1ff);
                QCC74x_WR_WORD(0x20050100,0x3);
            }
        }

    }
    vTaskDelete(NULL);
}

#endif
void mfg_main(void *pvParameters)
{
    uint32_t i=0;
    uint8_t cmd;
    uint8_t *cmd_data=NULL;
    BaseType_t xStatus;
    uint8_t unicast_init=0;
#ifdef MFG_HTOL
    static StackType_t qcc74x_mfg_htol_stack[512];
    static StaticTask_t qcc74x_mfg_htol_task;
#endif


#ifdef MFG_HTOL
    mfg_print("[OS] Starting qcc74x_mfg_htol task...\r\n");
    xTaskCreateStatic(qcc74x_mfg_htofw_entry, (char *)"qcc74x_htol_txrx", 512, NULL, 14, qcc74x_mfg_htol_stack, &qcc74x_mfg_htol_task);
#endif
    //vTaskDelay(1000);
#ifdef MFG_RAM
    qcc74x_sdio_init(NULL);
    /* sdu_host_check(1); */
#endif

    otp_init();

    const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 2000 );
    EventBits_t uxBits;
    extern EventGroupHandle_t xEventGroupMFG;
    uxBits = xEventGroupWaitBits(
            xEventGroupMFG,         //
            (1 << 0) | (1 << 1),    //
            pdTRUE,                 //
            pdTRUE,                 //
            xMaxBlockTime           //
    );

    if( (uxBits & ((1 << 0) | (1 << 1))) == ((1 << 0) | (1 << 1)) )
    {
        mfg_print("mfg task is running\r\n");
    }

    while(1){

        if( uxQueueMessagesWaiting( Message_Queue ) == 0 ){
            continue;
        }

        xStatus = xQueueReceive( Message_Queue, mfg_cmd, 1 );
        if( xStatus != pdPASS ){
            mfg_print( "Could not receive from the queue.\r\n" );
            continue;
        }

        if (at_command_input(mfg_cmd)) {
            continue;
        }

        if (strncmp(mfg_cmd,"AT+",3) == 0) {
            continue;
        }
        
        cmd=mfg_cmd[0];
        cmd_data=&mfg_cmd[1];

#if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
        if (mfg_cmd[0] != BT_LE_HCI_CMD)
#endif
        {
            mfg_print("[mfg fw] %s OK\r\n",(char *)mfg_cmd);
        }

        if(0==mfg_cmd_pre_deal(mfg_cmd)){
            continue;
        }

        for(i=0;i<sizeof(mfg_cmds_handler)/sizeof(mfg_cmds_handler[0]);i++){
            if(mfg_cmds_handler[i].cmd==cmd){
                if(mfg_cmds_handler[i].cmd_process!=NULL){
                    mfg_cmds_handler[i].cmd_process(cmd_data,0);
                }else{
                    mfg_print("cmd %c handler not found\r\n",cmd);
                }
                break;
            }
        }

        if(i>=sizeof(mfg_cmds_handler)/sizeof(mfg_cmds_handler[0])){
            mfg_print("cmd %c not found\r\n",cmd);
        }
    }
}









