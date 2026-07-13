
#if __has_include("board_overlay.h")
/* Use board_overlay.c instead of this file */
#else

#ifdef CONFIG_CONSOLE_WO
#include "qcc74x_wo.h"
#else
#include "qcc74x_uart.h"
#endif
#include "qcc74x_gpio.h"
#include "qcc74x_clock.h"
#include "qcc74x_rtc.h"
#include "qcc74x_flash.h"
#include "qcc74x_acomp.h"
#include "qcc74x_efuse.h"
#include "board.h"
#include "qcc743_tzc_sec.h"

#include "qcc743_glb.h"

#include "board_flash_psram.h"
#include "qcc743_sys.h"

#include "mem.h"

#ifdef CONFIG_BSP_SDH_SDCARD
#include "sdh_sdcard.h"
#endif

extern void log_start(void);

extern uint32_t __HeapBase;
extern uint32_t __HeapLimit;
extern uint32_t __psram_heap_base;
extern uint32_t __psram_limit;

extern uint32_t __psram_data_start__;
extern uint32_t __psram_data_end__;
extern uint32_t __psram_noinit_data_start__;
extern uint32_t __psram_noinit_data_end__;

#ifdef CONFIG_CONSOLE_WO
static struct qcc74x_device_s *wo;
#else
static struct qcc74x_device_s *uart0;
#endif

#if (defined(CONFIG_LUA) || defined(CONFIG_QCC74xLOG) || defined(CONFIG_FATFS))
static struct qcc74x_device_s *rtc;
#endif

static void ATTR_CLOCK_SECTION __attribute__((noinline)) system_clock_init(void)
{
#if 1
    /* wifipll/audiopll */
    GLB_Power_On_XTAL_And_PLL_CLK(GLB_XTAL_40M, GLB_PLL_WIFIPLL | GLB_PLL_AUPLL);
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_TOP_WIFIPLL_320M);
    HBN_Set_MCU_XCLK_Sel(HBN_MCU_XCLK_XTAL);
#else
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_RC32M);
    GLB_Power_On_XTAL_And_PLL_CLK(GLB_XTAL_40M, GLB_PLL_WIFIPLL);
    GLB_Config_AUDIO_PLL_To_384M();
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_TOP_AUPLL_DIV1);
    GLB_Set_MCU_System_CLK_Div(0, 3);
#endif
    CPU_Set_MTimer_CLK(ENABLE, QCC74x_MTIMER_SOURCE_CLOCK_MCU_XCLK, Clock_System_Clock_Get(QCC74x_SYSTEM_CLOCK_XCLK) / 1000000 - 1);

#ifdef CONFIG_WIFI6
    /* enable wifi clock */
    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);
#endif
}

#ifdef CONFIG_ANTENNA_CONTROL
#include "antenna_al.h"
#define ANTENNA_NUM         2

static struct qcc74x_device_s *antenna_gpio = NULL;
static uint8_t g_antenna_gpio_pin;
static int board_antenna_hal_init(void);
static int board_antenna_hal_switch(antenna_id_t antenna);
static uint8_t board_antenna_hal_get_count(void);
static void board_antenna_hal_deinit(void);
static void board_antenna_log(ant_log_level_t level, ant_log_category_t category, const char *fmt, va_list args);
extern int board_antenna_init(bool dynamic_div_enabled, bool static_div_enabled, int pin);

/* Antenna HAL operations structure */
static antenna_hal_ops_t board_antenna_ops = {
    .init = board_antenna_hal_init,
    .switch_antenna = board_antenna_hal_switch,
    .get_antenna_count = board_antenna_hal_get_count,
    .deinit = board_antenna_hal_deinit,
    .log = board_antenna_log,  // Optional: Set to NULL to disable logging
    .dynamic_div_enabled = false,  // Enable for dynamic diversity
    .static_div_enabled = false,  // Enable for static diversity
};
#endif 

#ifndef LP_APP
static void peripheral_clock_init(void)
{
    PERIPHERAL_CLOCK_ADC_DAC_ENABLE();
    PERIPHERAL_CLOCK_SEC_ENABLE();
    PERIPHERAL_CLOCK_DMA0_ENABLE();
    PERIPHERAL_CLOCK_UART0_ENABLE();
    PERIPHERAL_CLOCK_UART1_ENABLE();
    PERIPHERAL_CLOCK_SPI0_ENABLE();
    PERIPHERAL_CLOCK_I2C0_ENABLE();
    PERIPHERAL_CLOCK_PWM0_ENABLE();
    PERIPHERAL_CLOCK_TIMER0_1_WDG_ENABLE();
    PERIPHERAL_CLOCK_IR_ENABLE();
    PERIPHERAL_CLOCK_I2S_ENABLE();
    PERIPHERAL_CLOCK_USB_ENABLE();
    PERIPHERAL_CLOCK_CAN_ENABLE();
    PERIPHERAL_CLOCK_AUDIO_ENABLE();
    PERIPHERAL_CLOCK_CKS_ENABLE();

    GLB_Set_UART_CLK(ENABLE, HBN_UART_CLK_XCLK, 0);
    GLB_Set_SPI_CLK(ENABLE, GLB_SPI_CLK_MCU_MUXPLL_160M, 0);
    GLB_Set_DBI_CLK(ENABLE, GLB_SPI_CLK_MCU_MUXPLL_160M, 0);
    GLB_Set_I2C_CLK(ENABLE, GLB_I2C_CLK_XCLK, 0);
    GLB_Set_ADC_CLK(ENABLE, GLB_ADC_CLK_XCLK, 1);
    GLB_Set_DIG_CLK_Sel(GLB_DIG_CLK_XCLK);
    GLB_Set_DIG_512K_CLK(ENABLE, ENABLE, 0x4E);
    GLB_Set_PWM1_IO_Sel(GLB_PWM1_IO_SINGLE_END);
    GLB_Set_IR_CLK(ENABLE, GLB_IR_CLK_SRC_XCLK, 19);
    GLB_Set_CAM_CLK(ENABLE, GLB_CAM_CLK_WIFIPLL_96M, 3);

    GLB_Set_PKA_CLK_Sel(GLB_PKA_CLK_MCU_MUXPLL_160M);
#ifdef CONFIG_BSP_SDH_SDCARD
    PERIPHERAL_CLOCK_SDH_ENABLE();
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_EXT_SDH);
#endif

    GLB_Set_USB_CLK_From_WIFIPLL(1);
    GLB_Swap_MCU_SPI_0_MOSI_With_MISO(0);
}
#else
static void peripheral_clock_init_lp(void)
{
    uint32_t tmpVal = 0;

    /* clk gate,except DMA&CPU&UART0&SF&EMI&WIFI&EFUSE */
    tmpVal = 0;
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_M_CPU, 1); // ungate cpu
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_M_DMA, 1); // ungate dma
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_M_SEC, 1); // ungate sec
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_M_SDU, 1); // ungate sdu
    QCC74x_WR_REG(GLB_BASE, GLB_CGEN_CFG0, tmpVal);

    tmpVal = 0;
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_RSVD0, 1); // ungate dma and gpio set&clr register's bclk
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EF_CTRL, 1); // ungate ef_ctrl
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_SF_CTRL, 1); // ungate sf_ctrl
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_DMA, 1); // ungate dma
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_UART0, 1); // ungate uart0
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_UART1, 1); // ungate uart1
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_SEC_ENG, 1); // ungate sev_eng

    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_UART2, 1); // ungate uart2
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_I2C1, 1); // ungate i2c1
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_DBI, 1); // ungate dbi
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_CKS, 1); // ungate cks
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_IR, 1); // ungate ir
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_TIMER, 1); // ungate timer
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_PWM, 1); // ungate pwm
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_I2C, 1); // ungate i2c
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_SPI, 1); // ungate spi
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_TZ, 1); // ungate tz
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_SEC_DBG, 1); // ungate sec_dbg
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_GPIP, 1); // ungate gpip
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_RF_TOP, 1); // ungate rf_top
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_RSVD12, 1); // ungate audio
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1A_RSVD11, 1); // ungate i2s
#if (defined(CONFIG_NETHUB_PROFILE_SDIO))
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_RSVD13, 1); // ungate sdu&usb
#endif
    QCC74x_WR_REG(GLB_BASE, GLB_CGEN_CFG1, tmpVal);

    tmpVal = 0;
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S2_WIFI, 1); // ungate wifi
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_EMI_MISC, 1); // ungate emi_misc
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_PIO, 1); // ungate pec

    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_DMA2, 1); // ungate dma2
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_EMAC, 1); // ungate emac
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_SDH, 1); // ungate sdh
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_AUDIO, 1); // ungate audio
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_MIX2, 1); // ungate mix2
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_USB, 1); // ungate usb
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_PSRAM_CTRL, 1); // ungate psram_ctrl
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_EXT_PSRAM0_CTRL, 1); // ungate psram0_ctrl
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S3_M1542, 1); // ungate m1542
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S3_M154, 1); // ungate m154
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S3_BT_BLE2, 1); // ungate bt_ble2
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S3_BT_BLE, 1); // ungate bt_ble

    QCC74x_WR_REG(GLB_BASE, GLB_CGEN_CFG2, tmpVal);

    GLB_Set_UART_CLK(ENABLE, HBN_UART_CLK_XCLK, 0);
    
    GLB_Set_PKA_CLK_Sel(GLB_PKA_CLK_MCU_MUXPLL_160M);

    qcc74x_group0_request_aes_access(qcc74x_device_get_by_name("aes"));
    qcc74x_aes_link_init(qcc74x_device_get_by_name("aes"));
    qcc74x_pka_init(qcc74x_device_get_by_name("pka"));
    qcc74x_group0_request_sha_access(qcc74x_device_get_by_name("sha"));
    qcc74x_sha_link_init(qcc74x_device_get_by_name("sha"));

#ifdef CONFIG_BSP_SDH_SDCARD
    PERIPHERAL_CLOCK_SDH_ENABLE();
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_EXT_SDH);
#endif

#ifdef CONFIG_BSP_USB
    PERIPHERAL_CLOCK_USB_ENABLE();
    GLB_Set_USB_CLK_From_WIFIPLL(1);
#endif

    PERIPHERAL_CLOCK_ADC_DAC_ENABLE();
    GLB_Set_ADC_CLK(ENABLE, GLB_ADC_CLK_XCLK, 1);
}
#endif

#if defined(CONFIG_ANTI_ROLLBACK) && !defined(CONFIG_BOOT2)
extern const qcc74x_verinf_t app_ver;
uint8_t efuse_version = 0xFF;

static void qcc74x_check_anti_rollback(void){
    if(0 != qcc74x_get_app_version_from_efuse(&efuse_version)){
        printf("error! can't read app version in efuse\r\n");
        while(1){
        }
    }else{
        printf("app version in efuse is: %d\r\n", efuse_version);
    }

    if(app_ver.anti_rollback < efuse_version){
        printf("app version in application is: %d, less than app version in efuse, the application should not run up\r\n", app_ver.anti_rollback);
    }else{
        printf("app version in application is: %d, not less than app version in efuse, the application should run up\r\n", app_ver.anti_rollback);
    }

    /* change app version in efuse to app_ver.anti_rollback, default is 0 */
    if(app_ver.anti_rollback > efuse_version){
        qcc74x_set_app_version_to_efuse(app_ver.anti_rollback);//be attention! app version in efuse is incremental(from 0 to 128), and cannot be reduced forever
        printf("update app version in efuse to %d\r\n", app_ver.anti_rollback);

        /* check app version in efuse */
        if(0 != qcc74x_get_app_version_from_efuse(&efuse_version)){
            printf("error! can't read app version in efuse\r\n");
            while(1){
            }
        }else{
            printf("app version in efuse is: %d\r\n", efuse_version);
        }
    }
}
#endif

void qcc74x_show_log(void)
{
    printf("\r\n");
    printf("\r\n");
    printf("Build:%s,%s\r\n", __TIME__, __DATE__);
}

const char* qcc74x_sys_version(const char ***ctx)
{
    extern uint8_t _version_info_section_start;
    extern uint8_t _version_info_section_end;
    const char ** const version_section_start = (const char**)&_version_info_section_start;
    const char ** const version_section_end = (const char**)&_version_info_section_end;
    const char *version_str;

    //init
    if (NULL == (*ctx)) {
        (*ctx) = version_section_start;
    }
    //check the end
    if (version_section_end == (*ctx)) {
        return NULL;
    }
    version_str = (**ctx);
    *ctx = (*ctx) + 1;
    return version_str;
}

void qcc74x_show_component_version(void)
{
    const char **ctx = NULL;
    const char *version_str;

    puts("Version of used components:\r\n");
    while ((version_str = qcc74x_sys_version(&ctx))) {
        puts("\tVersion: ");
        puts(version_str);
        puts("\r\n");
    }
}

void qcc74x_show_chipinfo(void)
{
    qcc74x_efuse_device_info_type device_info;
    qcc74x_efuse_get_device_info(&device_info);

    printf("======== chip info ========\r\n");
    printf("package          %s\r\n", device_info.package_name);
    printf("flash_info       %s\r\n", device_info.flash_info_name);
    printf("psram_info       %s\r\n", device_info.psram_info_name);
    printf("version          A%d\r\n", device_info.version);
}

#ifdef CONFIG_CONSOLE_WO
extern void qcc74x_wo_set_console(struct qcc74x_device_s *dev);
#else
extern void qcc74x_uart_set_console(struct qcc74x_device_s *dev);
#endif

static void console_init()
{
#ifdef CONFIG_CONSOLE_WO
    wo = qcc74x_device_get_by_name("wo");
    qcc74x_wo_uart_init(wo, 2000000, GPIO_PIN_21);
    qcc74x_wo_set_console(wo);
#else
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_21, GPIO_UART_FUNC_UART0_TX);
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_22, GPIO_UART_FUNC_UART0_RX);

    struct qcc74x_uart_config_s cfg = {0};
    cfg.baudrate = 2000000;
    cfg.data_bits = UART_DATA_BITS_8;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.parity = UART_PARITY_NONE;
    cfg.flow_ctrl = 0;
    cfg.tx_fifo_threshold = 7;
    cfg.rx_fifo_threshold = 7;
    cfg.bit_order = UART_LSB_FIRST;

    uart0 = qcc74x_device_get_by_name("uart0");

    qcc74x_uart_init(uart0, &cfg);
    qcc74x_uart_set_console(uart0);
#endif
}

#ifdef LP_APP
void board_recovery(void)
{
    system_clock_init();
    peripheral_clock_init_lp();
    console_init();
#ifdef CONFIG_ANTENNA_CONTROL
    board_antenna_init(board_antenna_ops.dynamic_div_enabled, 
                       board_antenna_ops.static_div_enabled, 
                       g_antenna_gpio_pin);
#endif
}
#endif

void ram_heap_init(void)
{
    size_t heap_len;

    mem_manager_init();

    heap_len = ((size_t)&__HeapLimit - (size_t)&__HeapBase);
    mm_register_heap(MM_HEAP_OCRAM_0, "OCRAM", MM_ALLOCATOR_TLSF, &__HeapBase, heap_len);

#ifdef CONFIG_PSRAM
    board_psram_x8_init();

    heap_len = ((size_t)&__psram_limit - (size_t)&__psram_heap_base);
#ifndef CONFIG_PSRAM_SKIP_REGISTER_HEAP
    mm_register_heap(MM_HEAP_PSRAM_0, "PSRAM", MM_ALLOCATOR_TLSF, &__psram_heap_base, heap_len);
#endif

    printf("dynamic memory init success\r\n"
           "  ocram heap size: %d Kbyte, \r\n"
           "  psram heap size: %d Kbyte\r\n",
           ((size_t)&__HeapLimit - (size_t)&__HeapBase) / 1024,
           ((size_t)&__psram_limit - (size_t)&__psram_heap_base) / 1024);
#else
    if (&__psram_data_end__ - &__psram_data_start__ > 0 || &__psram_noinit_data_end__ - &__psram_noinit_data_start__ > 0) {
        puts("psram data already exists, please enable CONFIG_PSRAM\r\n");
        while (1) {}
    }

    printf("dynamic memory init success\r\n"
           "  ocram heap size: %d Kbyte \r\n",
           ((size_t)&__HeapLimit - (size_t)&__HeapBase) / 1024);
#endif
}

void board_init(void)
{
    int ret = -1;
    uintptr_t flag;
    uint32_t xtal_value = 0;

    flag = qcc74x_irq_save();

#ifndef CONFIG_BOARD_FLASH_INIT_SKIP
    ret = qcc74x_flash_init();
#endif

    system_clock_init();

#ifndef LP_APP
    peripheral_clock_init();
#else
    peripheral_clock_init_lp();
#endif

    qcc74x_irq_initialize();
#ifdef CONFIG_WIFI6
    extern void interrupt0_handler(void);
    qcc74x_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    qcc74x_irq_enable(WIFI_IRQn);
#endif

    console_init();

    ram_heap_init();

    qcc74x_show_log();
    qcc74x_show_component_version();

#if defined(CONFIG_ANTI_ROLLBACK) && !defined(CONFIG_BOOT2)
    qcc74x_check_anti_rollback();
#endif

    /* chip info dump */
    qcc74x_show_chipinfo();

    if (ret != 0) {
        printf("flash init fail!!!\r\n");
    }
    printf("dynamic memory init success, ocram heap size = %d Kbyte \r\n", ((size_t)&__HeapLimit - (size_t)&__HeapBase) / 1024);

    printf("uart  sig1:%08x, sig2:%08x\r\n", getreg32(GLB_BASE + GLB_UART_CFG1_OFFSET), getreg32(GLB_BASE + GLB_UART_CFG2_OFFSET));
    printf("clock gen1:%08x, gen2:%08x\r\n", getreg32(GLB_BASE + GLB_CGEN_CFG1_OFFSET), getreg32(GLB_BASE + GLB_CGEN_CFG2_OFFSET));

    HBN_Get_Xtal_Value(&xtal_value);
    printf("xtal:%dHz(%s)\r\n", xtal_value, ((getreg32(AON_BASE + AON_XTAL_CFG_OFFSET) >> 3) & 0x01) ? "oscillator" : "crystal");
    
    log_start();

#if (defined(CONFIG_LUA) || defined(CONFIG_QCC74xLOG) || defined(CONFIG_FATFS))
    rtc = qcc74x_device_get_by_name("rtc");
#endif

#ifdef CONFIG_MBEDTLS
    extern void qcc74x_sec_mutex_init(void);
    qcc74x_sec_mutex_init();
#endif
    qcc74x_irq_restore(flag);

    printf("board init done\r\n");
    printf("===========================\r\n");
}

#ifdef CONFIG_QCC74xLOG
__attribute__((weak)) uint64_t qcc74xlog_clock(void)
{
    return qcc74x_mtimer_get_time_us();
}

__attribute__((weak)) uint32_t qcc74xlog_time(void)
{
    return QCC74x_RTC_TIME2SEC(qcc74x_rtc_get_time(rtc));
}

__attribute__((weak)) char *qcc74xlog_thread(void)
{
    return "";
}
#endif

#ifdef CONFIG_LUA
__attribute__((weak)) clock_t luaport_clock(void)
{
    return (clock_t)qcc74x_mtimer_get_time_us();
}

__attribute__((weak)) time_t luaport_time(time_t *seconds)
{
    time_t t = (time_t)QCC74x_RTC_TIME2SEC(qcc74x_rtc_get_time(rtc));
    if (seconds != NULL) {
        *seconds = t;
    }

    return t;
}
#endif

#ifdef CONFIG_FATFS
#include "qcc74x_timestamp.h"
__attribute__((weak)) uint32_t get_fattime(void)
{
    qcc74x_timestamp_t tm;

    qcc74x_timestamp_utc2time(QCC74x_RTC_TIME2SEC(qcc74x_rtc_get_time(rtc)), &tm);

    return ((uint32_t)(tm.year - 1980) << 25) /* Year 2015 */
           | ((uint32_t)tm.mon << 21)         /* Month 1 */
           | ((uint32_t)tm.mday << 16)        /* Mday 1 */
           | ((uint32_t)tm.hour << 11)        /* Hour 0 */
           | ((uint32_t)tm.min << 5)          /* Min 0 */
           | ((uint32_t)tm.sec >> 1);         /* Sec 0 */
}
#endif

#ifdef CONFIG_SHELL
#include "shell.h"

static void show_sys_versoin_cmd(int argc, char **argv)
{
    qcc74x_show_component_version();
}
SHELL_CMD_EXPORT_ALIAS(show_sys_versoin_cmd, sysver, show sys version);

extern int qcc74x_sys_reset_por();

static void reboot_cmd(int argc, char **argv)
{
    qcc74x_sys_reset_por();
}
SHELL_CMD_EXPORT_ALIAS(reboot_cmd, reboot, reboot);

#define MFG_CONFIG_REG (0x2000F100)
#define MFG_CONFIG_VAL ("0mfg")

void mfg_config(void)
{
    union _reg_t {
        uint8_t byte[4];
        uint32_t word;
    } mfg = {
        .byte = MFG_CONFIG_VAL,
    };

    *(volatile uint32_t *)(MFG_CONFIG_REG) = mfg.word;
}

static void mfg_cmd(int argc, char **argv)
{
    mfg_config();
    qcc74x_sys_reset_por();
}
SHELL_CMD_EXPORT_ALIAS(mfg_cmd, mfg, mfg);

#ifdef LP_APP
#include "qcc74x_lp.h"

static void test_io_wakeup_status(uint8_t io_num)
{
    int wakeup_mode;

    wakeup_mode = qcc74x_lp_wakeup_io_get_mode(io_num);

    if (wakeup_mode) {
        /* user can add app process logic */
#if 0
        if(io_num == WAKEUP_BUTTON){
            /* deal a button wakeup event */
        }else if(io_num == WAKEUP_FINGER_INPUT){
            /* deal fingerprint input wake up event */
        }else if(io_num == WAKEUP_USB_INPUT){
            /* deal USB plug in wake up event */
        }
#endif
        printf("GPIO %d wakeup: ", io_num);
        if (wakeup_mode == QCC74x_LP_IO_WAKEUP_MODE_LOW) {
            printf("level low\r\n");
        } else if (wakeup_mode == QCC74x_LP_IO_WAKEUP_MODE_HIGH) {
            printf("level high\r\n");
        } else if (wakeup_mode == QCC74x_LP_IO_WAKEUP_MODE_FALLING) {
            printf("edge falling\r\n");
        } else if (wakeup_mode == QCC74x_LP_IO_WAKEUP_MODE_RISING) {
            printf("edge rising\r\n");
        } else {
            printf("unkown error: %d\r\n", wakeup_mode);
        }
    }
}

static void test_wakeup_io_callback(uint64_t wake_up_io_bits)
{
    printf("io wakeup bits 0x%llX\r\n", wake_up_io_bits);

    /* wakeup io status dump */
    for (uint8_t i = 0; i < QCC74x_LP_WAKEUP_IO_MAX_NUM; i++) {
        test_io_wakeup_status(i);
    }
}

void cmd_io_test(char *buf, int len, int argc, char **argv)
{
    static qcc74x_lp_io_cfg_t lp_wake_io_cfg = {
        /* input enable, use @ref QCC74x_LP_IO_INPUT_EN */
        .io_0_15_ie = QCC74x_LP_IO_INPUT_ENABLE,
        .io_16_ie = QCC74x_LP_IO_INPUT_ENABLE,
        .io_17_ie = QCC74x_LP_IO_INPUT_ENABLE,
        .io_18_ie = QCC74x_LP_IO_INPUT_ENABLE,
        .io_19_ie = QCC74x_LP_IO_INPUT_ENABLE,
        .io_20_34_ie = QCC74x_LP_IO_INPUT_ENABLE,
        /* trigger mode */
        .io_0_7_pds_trig_mode = QCC74x_LP_PDS_IO_TRIG_SYNC_FALLING_EDGE,          /* use @ref QCC74x_LP_PDS_IO_TRIG */
        .io_8_15_pds_trig_mode = QCC74x_LP_PDS_IO_TRIG_SYNC_HIGH_LEVEL,           /* use @ref QCC74x_LP_PDS_IO_TRIG */
        .io_16_19_aon_trig_mode = QCC74x_LP_AON_IO_TRIG_SYNC_RISING_FALLING_EDGE, /* aon io, use @ref QCC74x_LP_AON_IO_TRIG, full mode support */
        .io_20_27_pds_trig_mode = QCC74x_LP_PDS_IO_TRIG_SYNC_FALLING_EDGE,        /* use @ref QCC74x_LP_PDS_IO_TRIG */
        .io_28_34_pds_trig_mode = QCC74x_LP_PDS_IO_TRIG_SYNC_FALLING_EDGE,        /* use @ref QCC74x_LP_PDS_IO_TRIG */
        /* resistors */
        .io_0_15_res = QCC74x_LP_IO_RES_PULL_UP,
        .io_16_res = QCC74x_LP_IO_RES_NONE,
        .io_17_res = QCC74x_LP_IO_RES_NONE,
        .io_18_res = QCC74x_LP_IO_RES_PULL_UP,
        .io_19_res = QCC74x_LP_IO_RES_PULL_UP,
        .io_20_34_res = QCC74x_LP_IO_RES_PULL_DOWN,
        /* wake up unmask */
        .io_wakeup_unmask = 0,
    };

    /* wake up unmask */
    lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 0); /* gpio 0 */
    // lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 10); /* gpio 10 */
    lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 18); /* gpio 18 */
    // lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 19); /* gpio 19 */
    // lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 20); /* gpio 20 */

    lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 31); /* gpio 31 */
    // lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 32); /* gpio 32 */
    // lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 33);     /* gpio 33 */
    // lp_wake_io_cfg.io_wakeup_unmask |= ((uint64_t)1 << 34);     /* gpio 34 */

    qcc74x_lp_io_wakeup_cfg(&lp_wake_io_cfg);

    /* register io wakeup callback */
    qcc74x_lp_wakeup_io_int_register(test_wakeup_io_callback);
}

static void test_acomp_wakeup_status(uint8_t acomp_num)
{
    int wakeup_mode;

    wakeup_mode = qcc74x_lp_wakeup_acomp_get_mode(acomp_num);

    if (wakeup_mode) {
        printf("ACOMP %d wakeup: ", acomp_num);
        if (wakeup_mode == QCC74x_LP_ACOMP_WAKEUP_MODE_FALLING) {
            printf("edge falling\r\n");
        } else if (wakeup_mode == QCC74x_LP_ACOMP_WAKEUP_MODE_RISING) {
            printf("edge rising\r\n");
        } else {
            printf("unkown error: %d\r\n", wakeup_mode);
        }
    }
}

static void test_wakeup_acomp_callback(uint32_t wake_up_acomp_bits)
{
    printf("acomp wakeup bits 0x%02X\r\n", wake_up_acomp_bits);

    for (uint8_t i = 0; i < QCC74x_LP_WAKEUP_ACOMP_MAX_NUM; i++) {
        test_acomp_wakeup_status(i);
    }
}

void cmd_acomp_test(char *buf, int len, int argc, char **argv)
{
    static qcc74x_lp_acomp_cfg_t lp_wake_acomp_cfg = {
        /* input enable, use @ref QCC74x_LP_ACOMP_EN */
        .acomp0_en = QCC74x_LP_ACOMP_ENABLE,
        .acomp1_en = QCC74x_LP_ACOMP_ENABLE,

        /* Map to pins num, range: 2, 3, 10, 12, 13, 14, 19 */
        .acomp0_io_num = 13,
        .acomp1_io_num = 14,

        /* trigger mode, use @ref QCC74x_LP_ACOMP_TRIG  */
        .acomp0_trig_mode = QCC74x_LP_ACOMP_TRIG_EDGE_FALLING,
        .acomp1_trig_mode = QCC74x_LP_ACOMP_TRIG_EDGE_FALLING_RISING,
    };

    qcc74x_lp_acomp_wakeup_cfg(&lp_wake_acomp_cfg);

    /* register acomp wakeup callback */
    qcc74x_lp_wakeup_acomp_int_register(test_wakeup_acomp_callback);
}
SHELL_CMD_EXPORT_ALIAS(cmd_io_test, io_test, cmd io_test);
SHELL_CMD_EXPORT_ALIAS(cmd_acomp_test, acomp_test, cmd acomp_test);
#endif /* LP_APP */

#endif /* CONFIG_SHELL */


#ifdef CONFIG_ANTENNA_CONTROL

/* Board-specific antenna HAL operations */
static int board_antenna_hal_init(void)
{

    return 0;
}

static int board_antenna_hal_switch(antenna_id_t antenna)
{
    if (!antenna_gpio) {
        return -1;
    }

    if (antenna == ANTENNA_0) {
        qcc74x_gpio_set(antenna_gpio, g_antenna_gpio_pin);
    } else if (antenna == ANTENNA_1) {
        qcc74x_gpio_reset(antenna_gpio, g_antenna_gpio_pin);
    }

    return 0;
}

static uint8_t board_antenna_hal_get_count(void)
{
    return ANTENNA_NUM;
}

static void board_antenna_hal_deinit(void)
{
    /* Nothing to do for GPIO-based implementation */
}

/* Board-specific logging implementation */
static void board_antenna_log(ant_log_level_t level, ant_log_category_t category, const char *fmt, va_list args)
{
    const char *level_str;
    const char *cat_str;

    /* Convert level to string */
    switch (level) {
        case ANT_LOG_ERROR: level_str = "ERR"; break;
        case ANT_LOG_WARN:  level_str = "WAR"; break;
        case ANT_LOG_INFO:  level_str = "INF"; break;
        case ANT_LOG_DEBUG: level_str = "DBG"; break;
        default: level_str = "UNK"; break;
    }

    /* Convert category to string */
    switch (category) {
        case ANT_CAT_STATIC:  cat_str = "S"; break;
        case ANT_CAT_DYNAMIC: cat_str = "D"; break;
        case ANT_CAT_HAL:     cat_str = "H"; break;
        default: cat_str = " "; break;
    }

    /* Print log prefix */
    printf("[ANT-%s:%s] ", cat_str, level_str);

    /* Print formatted message */
    vprintf(fmt, args);
    printf("\r\n");
}

int board_antenna_init(bool dynamic_div_enabled, bool static_div_enabled, int pin)
{
    board_antenna_ops.dynamic_div_enabled = dynamic_div_enabled;
    board_antenna_ops.static_div_enabled = static_div_enabled;
    if (dynamic_div_enabled == 0 && static_div_enabled == 0) {
        return 0;
    }
    antenna_gpio = qcc74x_device_get_by_name("gpio");
    if (!antenna_gpio) {
        return -1;
    }

    qcc74x_gpio_init(antenna_gpio, pin, GPIO_OUTPUT | GPIO_SMT_EN | GPIO_DRV_0);
    board_antenna_hal_switch(antenna_hal_get_current_antenna());
    g_antenna_gpio_pin = pin;
    return antenna_hal_init(&board_antenna_ops);
}
#endif

int board_device_info_version()
{
    static qcc74x_efuse_device_info_type device_info;
    static int init = 0;
    if(!init) {
        qcc74x_efuse_get_device_info(&device_info);
    }
    return device_info.version;
}

#endif /* __has_include("board_overlay.h") */
