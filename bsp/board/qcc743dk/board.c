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
#include "qcc743_sys.h"

#include "mem.h"

#ifdef CONFIG_BSP_SDH_SDCARD
#include "sdh_sdcard.h"
#endif

extern void log_start(void);

extern uint32_t __HeapBase;
extern uint32_t __HeapLimit;

#ifdef CONFIG_CONSOLE_WO
static struct qcc74x_device_s *wo;
#else
static struct qcc74x_device_s *uart0;
#endif

#if (defined(CONFIG_LUA) || defined(CONFIG_QCC74xLOG) || defined(CONFIG_FATFS))
static struct qcc74x_device_s *rtc;
#endif

static void system_clock_init(void)
{
#if 1
    /* wifipll/audiopll */
    GLB_Power_On_XTAL_And_PLL_CLK(GLB_XTAL_40M, GLB_PLL_WIFIPLL | GLB_PLL_AUPLL);
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_TOP_WIFIPLL_320M);
#else
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_RC32M);
    GLB_Power_On_XTAL_And_PLL_CLK(GLB_XTAL_40M, GLB_PLL_WIFIPLL);
    GLB_Config_AUDIO_PLL_To_384M();
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_TOP_AUPLL_DIV1);
    GLB_Set_MCU_System_CLK_Div(0, 3);
#endif
    CPU_Set_MTimer_CLK(ENABLE, QCC74x_MTIMER_SOURCE_CLOCK_MCU_XCLK, Clock_System_Clock_Get(QCC74x_SYSTEM_CLOCK_XCLK) / 1000000 - 1);
}

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
    // tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, GLB_CGEN_S1_RSVD13, 1); // ungate sdu&usb

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

void qcc74x_show_flashinfo(void)
{
    spi_flash_cfg_type flashCfg;
    uint8_t *pFlashCfg = NULL;
    uint32_t flashSize = 0;
    uint32_t flashCfgLen = 0;
    uint32_t flashJedecId = 0;

    flashJedecId = qcc74x_flash_get_jedec_id();
    flashSize = qcc74x_flash_get_size();
    qcc74x_flash_get_cfg(&pFlashCfg, &flashCfgLen);
    arch_memcpy((void *)&flashCfg, pFlashCfg, flashCfgLen);
    printf("======== flash cfg ========\r\n");
    printf("flash size 0x%08X\r\n", flashSize);
    printf("jedec id     0x%06X\r\n", flashJedecId);
    printf("mid              0x%02X\r\n", flashCfg.mid);
    printf("iomode           0x%02X\r\n", flashCfg.io_mode);
    printf("clk delay        0x%02X\r\n", flashCfg.clk_delay);
    printf("clk invert       0x%02X\r\n", flashCfg.clk_invert);
    printf("read reg cmd0    0x%02X\r\n", flashCfg.read_reg_cmd[0]);
    printf("read reg cmd1    0x%02X\r\n", flashCfg.read_reg_cmd[1]);
    printf("write reg cmd0   0x%02X\r\n", flashCfg.write_reg_cmd[0]);
    printf("write reg cmd1   0x%02X\r\n", flashCfg.write_reg_cmd[1]);
    printf("qe write len     0x%02X\r\n", flashCfg.qe_write_reg_len);
    printf("cread support    0x%02X\r\n", flashCfg.c_read_support);
    printf("cread code       0x%02X\r\n", flashCfg.c_read_mode);
    printf("burst wrap cmd   0x%02X\r\n", flashCfg.burst_wrap_cmd);
    printf("===========================\r\n");
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
}
#endif

static qcc74x_efuse_device_info_type device_info;
int board_device_info_version()
{
    return device_info.version;
}

#if defined(CONFIG_ANTI_ROLLBACK) && !defined(CONFIG_BOOT2)
extern const qcc74xverinf_t app_ver;
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

#ifdef CONFIG_ANTENNA_CONTROL
#define ANTENNA_NUM 2
#define ANTENNA_ID_0 0
#define ANTENNA_ID_1 1
#define ANTENNA_GPIO_PIN GPIO_PIN_30

static struct qcc74x_device_s *antenna_gpio;

int board_antenna_set(uint8_t antenna_id)
{
    if (!antenna_gpio) {
        return -1;
    }

    printf("[board] switch antenna to %d\r\n", antenna_id);

    if (antenna_id == ANTENNA_ID_0) {
        qcc74x_gpio_set(antenna_gpio, ANTENNA_GPIO_PIN);
    } else if (antenna_id == ANTENNA_ID_1) {
        qcc74x_gpio_reset(antenna_gpio, ANTENNA_GPIO_PIN);
    }

    return 0;
}

int board_antenna_init(void)
{
    antenna_gpio = qcc74x_device_get_by_name("gpio");
    if (!antenna_gpio) {
        return -1;
    }

    qcc74x_gpio_init(antenna_gpio, ANTENNA_GPIO_PIN, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);

    return 0;
}

int board_antenna_num_get(void)
{
    return ANTENNA_NUM;
}
#endif

void board_init(void)
{
    int ret = -1;
    uintptr_t flag;
    size_t heap_len;

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

    console_init();

#if defined(CONFIG_ANTI_ROLLBACK) && !defined(CONFIG_BOOT2)
    qcc74x_check_anti_rollback();
#endif

    qcc74x_show_log();
    qcc74x_show_component_version();
    qcc74x_efuse_get_device_info(&device_info);
    printf("Current chip device version: %d \r\n", device_info.version);

    heap_len = ((size_t)&__HeapLimit - (size_t)&__HeapBase);
    kmem_init((void *)&__HeapBase, heap_len);

    if (ret != 0) {
        printf("flash init fail!!!\r\n");
    }
    qcc74x_show_flashinfo();
    printf("dynamic memory init success, ocram heap size = %d Kbyte \r\n", ((size_t)&__HeapLimit - (size_t)&__HeapBase) / 1024);

    printf("sig1:%08x\r\n", QCC74x_RD_REG(GLB_BASE, GLB_UART_CFG1));
    printf("sig2:%08x\r\n", QCC74x_RD_REG(GLB_BASE, GLB_UART_CFG2));
    printf("cgen1:%08x\r\n", getreg32(QCC74x_GLB_CGEN1_BASE));

    log_start();

#if (defined(CONFIG_LUA) || defined(CONFIG_QCC74xLOG) || defined(CONFIG_FATFS))
    rtc = qcc74x_device_get_by_name("rtc");
#endif
#ifdef CONFIG_MBEDTLS
    extern void qcc74x_sec_mutex_init(void);
    qcc74x_sec_mutex_init();
#endif
    qcc74x_irq_restore(flag);
}

void board_uartx_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");

    qcc74x_gpio_uart_init(gpio, GPIO_PIN_27, GPIO_UART_FUNC_UART1_TX);
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_28, GPIO_UART_FUNC_UART1_RX);
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_29, GPIO_UART_FUNC_UART1_CTS);
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_30, GPIO_UART_FUNC_UART1_RTS);
}

void board_i2c0_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    /* I2C0_SDA */
    qcc74x_gpio_init(gpio, GPIO_PIN_11, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    /* I2C0_SCL */
    qcc74x_gpio_init(gpio, GPIO_PIN_14, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
}

void board_spi0_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
  
    /* spi cs */ 
    qcc74x_gpio_init(gpio, GPIO_PIN_28, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
    /* spi clk */
    qcc74x_gpio_init(gpio, GPIO_PIN_29, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
    /* spi miso */
    qcc74x_gpio_init(gpio, GPIO_PIN_30, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
    /* spi mosi */
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
}

void board_spi0_gpio_3pin_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
 
    /* spi clk */
    qcc74x_gpio_init(gpio, GPIO_PIN_29, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
    /* spi miso */
    qcc74x_gpio_init(gpio, GPIO_PIN_30, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
    /* spi mosi */
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_FUNC_SPI0 | GPIO_ALTERNATE | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_1);
}

void board_pwm_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_init(gpio, GPIO_PIN_24, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_25, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_26, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_28, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_29, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_30, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_31, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
}

void board_adc_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    /* ADC_CH0 */
    qcc74x_gpio_init(gpio, GPIO_PIN_20, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH1 */
    qcc74x_gpio_init(gpio, GPIO_PIN_19, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH2 */
    qcc74x_gpio_init(gpio, GPIO_PIN_2, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH3 */
    qcc74x_gpio_init(gpio, GPIO_PIN_3, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH4 */
    qcc74x_gpio_init(gpio, GPIO_PIN_14, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH5 */
    qcc74x_gpio_init(gpio, GPIO_PIN_13, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH6 */
    qcc74x_gpio_init(gpio, GPIO_PIN_12, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH7 */
    qcc74x_gpio_init(gpio, GPIO_PIN_10, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH8 */
    qcc74x_gpio_init(gpio, GPIO_PIN_1, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH9 */
    qcc74x_gpio_init(gpio, GPIO_PIN_0, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH10 */
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* ADC_CH11 */
    qcc74x_gpio_init(gpio, GPIO_PIN_28, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
}

void board_dac_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    /* DAC_CHA */
    qcc74x_gpio_init(gpio, GPIO_PIN_3, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    /* DAC_CHB */
    qcc74x_gpio_init(gpio, GPIO_PIN_2, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
}

void board_emac_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_init(gpio, GPIO_PIN_25, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_26, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_28, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_29, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_30, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_31, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_32, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_33, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_34, GPIO_FUNC_EMAC | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
#if defined(QCC743)
    GLB_Config_AUDIO_PLL_To_400M();
    // GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_EMAC);
#endif
}

void board_sdh_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_init(gpio, GPIO_PIN_10, GPIO_FUNC_SDH | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    qcc74x_gpio_init(gpio, GPIO_PIN_11, GPIO_FUNC_SDH | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    qcc74x_gpio_init(gpio, GPIO_PIN_12, GPIO_FUNC_SDH | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    qcc74x_gpio_init(gpio, GPIO_PIN_13, GPIO_FUNC_SDH | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    qcc74x_gpio_init(gpio, GPIO_PIN_14, GPIO_FUNC_SDH | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    qcc74x_gpio_init(gpio, GPIO_PIN_15, GPIO_FUNC_SDH | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
}

void board_ir_gpio_init(void)
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_init(gpio, GPIO_PIN_10, GPIO_INPUT | GPIO_SMT_EN | GPIO_DRV_0);
    GLB_IR_RX_GPIO_Sel(GLB_GPIO_PIN_10);
}

void board_dvp_gpio_init(void)
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");

#if 1
    /* DVP0 GPIO init */
    /* I2C GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_0, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_1, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);

    /* Power down GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_2, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_reset(gpio, GPIO_PIN_2);

    /* Reset GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_3, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_reset(gpio, GPIO_PIN_3);
    qcc74x_mtimer_delay_ms(10);
    qcc74x_gpio_set(gpio, GPIO_PIN_3);
    qcc74x_mtimer_delay_ms(10);

    /* MCLK GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_23, GPIO_FUNC_CLKOUT | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);

    /* DVP0 GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_24, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_25, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_26, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_28, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_29, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_30, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_31, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_32, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_33, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_34, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
#else
    /* DVP1 GPIO init */
    /* I2C GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_28, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_27, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);

    /* Power down GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_29, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_reset(gpio, GPIO_PIN_29);

    /* Reset GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_30, GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_reset(gpio, GPIO_PIN_3);
    qcc74x_mtimer_delay_ms(10);
    qcc74x_gpio_set(gpio, GPIO_PIN_3);
    qcc74x_mtimer_delay_ms(10);

    /* MCLK GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_20, GPIO_FUNC_CLKOUT | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);

    /* DVP1 GPIO */
    qcc74x_gpio_init(gpio, GPIO_PIN_0, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_1, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_3, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_10, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_11, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_12, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_13, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_14, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_15, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_16, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_17, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
#endif
}

void board_i2s_gpio_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");
    qcc74x_gpio_init(gpio, GPIO_PIN_16, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_17, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_18, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    qcc74x_gpio_init(gpio, GPIO_PIN_19, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
}

void board_acomp_init()
{
    struct qcc74x_device_s *gpio;

    gpio = qcc74x_device_get_by_name("gpio");

    qcc74x_gpio_init(gpio, GPIO_PIN_13, GPIO_ANALOG | GPIO_PULL_NONE | GPIO_DRV_0);
    qcc74x_gpio_init(gpio, GPIO_PIN_14, GPIO_ANALOG | GPIO_PULL_NONE | GPIO_DRV_0);

    struct qcc74x_acomp_config_s acomp_cfg = {
        .mux_en = ENABLE,
        .pos_chan_sel = AON_ACOMP_CHAN_ADC0,
        .neg_chan_sel = AON_ACOMP_CHAN_VIO_X_SCALING_FACTOR_1,
        .vio_sel = DEFAULT_ACOMP_VREF_1V65,
        .scaling_factor = AON_ACOMP_SCALING_FACTOR_1,
        .bias_prog = AON_ACOMP_BIAS_POWER_MODE1,
        .hysteresis_pos_volt = AON_ACOMP_HYSTERESIS_VOLT_NONE,
        .hysteresis_neg_volt = AON_ACOMP_HYSTERESIS_VOLT_NONE,
    };

    acomp_cfg.pos_chan_sel = AON_ACOMP_CHAN_ADC5;
    qcc74x_acomp_init(AON_ACOMP0_ID, &acomp_cfg);
    acomp_cfg.pos_chan_sel = AON_ACOMP_CHAN_ADC4;
    qcc74x_acomp_init(AON_ACOMP1_ID, &acomp_cfg);
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

static void reboot_cmd(int argc, char **argv)
{
    qcc74x_sys_reset_por();
}

static void show_sys_versoin_cmd(int argc, char **argv)
{
    qcc74x_show_component_version();
}
SHELL_CMD_EXPORT_ALIAS(reboot_cmd, reboot, reboot);
SHELL_CMD_EXPORT_ALIAS(show_sys_versoin_cmd, sysver, show sys version);

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
