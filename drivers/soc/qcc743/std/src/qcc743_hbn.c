#include "qcc743_hbn.h"
#include "qcc74x_acomp.h"
#include "qcc743_glb.h"
#include "qcc74x_xip_sflash.h"

/** @addtogroup  QCC743_Peripheral_Driver
 *  @{
 */

/** @addtogroup  HBN
 *  @{
 */

/** @defgroup  HBN_Private_Macros
 *  @{
 */
#define HBN_CLK_SET_DUMMY_WAIT \
    {                          \
        __NOP();               \
        __NOP();               \
        __NOP();               \
        __NOP();               \
        __NOP();               \
        __NOP();               \
        __NOP();               \
        __NOP();               \
    }

/*@} end of group HBN_Private_Macros */

/** @defgroup  HBN_Private_Types
 *  @{
 */

/*@} end of group HBN_Private_Types */

/** @defgroup  HBN_Private_Variables
 *  @{
 */
#ifndef QCC74x_USE_HAL_DRIVER
static intCallback_Type *hbnInt0CbfArra[HBN_OUT0_INT_MAX] = { NULL };
static intCallback_Type *hbnInt1CbfArra[HBN_OUT1_INT_MAX] = { NULL };
#endif

/*@} end of group HBN_Private_Variables */

/** @defgroup  HBN_Global_Variables
 *  @{
 */

/*@} end of group HBN_Global_Variables */

/** @defgroup  HBN_Private_Fun_Declaration
 *  @{
 */

/*@} end of group HBN_Private_Fun_Declaration */

/** @defgroup  HBN_Private_Functions
 *  @{
 */

/*@} end of group HBN_Private_Functions */

/** @defgroup  HBN_Public_Functions
 *  @{
 */

/****************************************************************************/ /**
 * @brief  Enter HBN
 *
 * @param  cfg: HBN APP Config
 *
 * @return None
 *
*******************************************************************************/
void ATTR_TCM_SECTION HBN_Mode_Enter(HBN_APP_CFG_Type *cfg)
{
#ifndef BOOTROM
    uint32_t valLow = 0, valHigh = 0;
    uint64_t val;

    if (cfg->useXtal32k) {
        HBN_32K_Sel(HBN_32K_XTAL);
    } else {
        HBN_32K_Sel(HBN_32K_RC);
        HBN_Power_Off_Xtal_32K();
    }

    /* always disable HBN pin pull up/down to reduce PDS/HBN current, 0x4000F014[16]=0 */
    HBN_Hw_Pu_Pd_Cfg(DISABLE);

    HBN_Pin_WakeUp_Mask(~(cfg->gpioWakeupSrc));
    if (cfg->gpioWakeupSrc != 0) {
        HBN_GPIO_INT_Enable(cfg->gpioTrigType);
    }

    /* HBN RTC config and enable */
    HBN_Clear_RTC_Counter();
    if (cfg->sleepTime != 0) {
        HBN_Get_RTC_Timer_Val(&valLow, &valHigh);
        val = valLow + ((uint64_t)valHigh << 32);
        val += cfg->sleepTime;
        HBN_Set_RTC_Timer(HBN_RTC_INT_DELAY_0T, val & 0xffffffff, val >> 32, HBN_RTC_COMP_BIT0_39);
        HBN_Enable_RTC_Counter();
    }

    HBN_Power_Down_Flash(cfg->flashCfg);

    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_RC32M);

    HBN_Enable(cfg->gpioWakeupSrc, cfg->ldoLevel, cfg->hbnLevel, cfg->dcdcPuSeq);
#endif
}

/****************************************************************************/ /**
 * @brief  power down and switch clock
 *
 * @param  flashCfg: None
 *
 * @return None
 *
*******************************************************************************/
void ATTR_TCM_SECTION HBN_Power_Down_Flash(spi_flash_cfg_type *flashCfg)
{
    spi_flash_cfg_type bhFlashCfg;

    if (flashCfg == NULL) {
        L1C_DCache_Invalid_By_Addr(QCC743_FLASH_XIP_BASE + 8 + 4, sizeof(spi_flash_cfg_type));
        qcc74x_xip_sflash_read_via_cache_need_lock(QCC743_FLASH_XIP_BASE + 8 + 4, (uint8_t *)(&bhFlashCfg), sizeof(spi_flash_cfg_type), 0, SF_CTRL_FLASH_BANK0);
        L1C_DCache_Invalid_By_Addr(QCC743_FLASH_XIP_BASE + 8 + 4, sizeof(spi_flash_cfg_type));

        qcc74x_sf_ctrl_set_owner(SF_CTRL_OWNER_SAHB);
        qcc74x_sflash_reset_continue_read(&bhFlashCfg);
    } else {
        qcc74x_sf_ctrl_set_owner(SF_CTRL_OWNER_SAHB);
        qcc74x_sflash_reset_continue_read(flashCfg);
    }

    qcc74x_sflash_powerdown();
}

/****************************************************************************/ /**
 * @brief  Enable HBN mode
 *
 * @param  aGPIOIeCfg: AON GPIO input enable config. Bit(s) of Wakeup GPIO(s) must not be set to
 *                     0(s),say when use GPIO7 as wake up pin,aGPIOIeCfg should be 0x01.
 * @param  ldoLevel: LDO volatge level, this parameter can be one of the following values:
 *           @arg HBN_LDO_LEVEL_0P70V
 *           @arg HBN_LDO_LEVEL_0P75V
 *           @arg HBN_LDO_LEVEL_0P80V
 *           @arg HBN_LDO_LEVEL_0P85V
 *           @arg HBN_LDO_LEVEL_0P90V
 *           @arg HBN_LDO_LEVEL_0P95V
 *           @arg HBN_LDO_LEVEL_1P00V
 *           @arg HBN_LDO_LEVEL_1P05V
 *           @arg HBN_LDO_LEVEL_1P10V
 *           @arg HBN_LDO_LEVEL_1P15V
 *           @arg HBN_LDO_LEVEL_1P20V
 *           @arg HBN_LDO_LEVEL_1P25V
 *           @arg HBN_LDO_LEVEL_1P30V
 *           @arg HBN_LDO_LEVEL_1P35V
 * @param  hbnLevel: HBN work level, this parameter can be one of the following values:
 *           @arg HBN_LEVEL_0
 *           @arg HBN_LEVEL_1
 *
 * @return None
 *
*******************************************************************************/
void ATTR_TCM_SECTION HBN_Enable(uint32_t aGPIOIeCfg, uint8_t ldoLevel, uint8_t hbnLevel, uint8_t dcdcPuSeq)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_LDO_LEVEL_TYPE(ldoLevel));
    CHECK_PARAM(IS_HBN_LEVEL_TYPE(hbnLevel));

    /* Setting from guide */
    /* HBN_SRAM Retion */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_SRAM);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_RETRAM_RET);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_RETRAM_SLP);
    QCC74x_WR_REG(HBN_BASE, HBN_SRAM, tmpVal);

    /* AON GPIO IE */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_REG_EN_HW_PU_PD);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);

    /* HBN mode LDO level */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_LDO11_AON_VOUT_SEL, ldoLevel);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_LDO11_RT_VOUT_SEL, ldoLevel);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    /* power on dcdc18 sequence  */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    if (dcdcPuSeq) {
        tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_PU_DCDC18_AON);
    } else {
        tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PU_DCDC18_AON);
    }
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    /* Select RC32M */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL, 0);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);
    __NOP();
    __NOP();
    __NOP();
    __NOP();

    /* Set HBN flag */
    QCC74x_WR_REG(HBN_BASE, HBN_RSV0, HBN_STATUS_ENTER_FLAG);

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    /* Set HBN level, (HBN_PWRDN_HBN_RAM not use) */
    switch (hbnLevel) {
        case HBN_LEVEL_0:
            tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PWRDN_HBN_CORE);
            break;

        case HBN_LEVEL_1:
            tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_PWRDN_HBN_CORE);
            break;

        default:
            break;
    }
    /* Set power on option:0 for por reset twice for robust 1 for reset only once*/
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PWR_ON_OPTION);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    /* Enable HBN mode */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_MODE);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    while (1) {
        arch_delay_ms(1000);
    }
#endif
}

/****************************************************************************/ /**
 * @brief  Reset HBN mode
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Reset(void)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    /* Reset HBN mode */
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_SW_RST);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_SW_RST);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_SW_RST);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Select GPADC clk source
 *
 * @param  clkSel: GPADC clock type selection, this parameter can be one of the following values:
 *           @arg HBN_GPADC_CLK_32M
 *           @arg HBN_GPADC_CLK_F32K
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Set_GPADC_CLK_Sel(uint8_t clkSel)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_GPADC_CLK_TYPE(clkSel));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_PIR_CFG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_GPADC_CS, clkSel);
    QCC74x_WR_REG(HBN_BASE, HBN_PIR_CFG, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  get HBN bod out state
 *
 * @param  None
 *
 * @return SET or RESET
 *
*******************************************************************************/
QCC74x_Sts_Type HBN_Get_BOD_OUT_State(void)
{
    return QCC74x_GET_REG_BITS_VAL(QCC74x_RD_REG(HBN_BASE, HBN_BOR_CFG), HBN_R_BOD_OUT) ? SET : RESET;
}

/****************************************************************************/ /**
 * @brief  set HBN bod config
 *
 * @param  enable: ENABLE or DISABLE, if enable, Power up Brown Out Reset
 * @param  threshold: bod threshold, this parameter can be one of the following values:
 *           @arg HBN_BOD_THRES_2P05V
 *           @arg HBN_BOD_THRES_2P10V
 *           @arg HBN_BOD_THRES_2P15V
 *           @arg HBN_BOD_THRES_2P20V
 *           @arg HBN_BOD_THRES_2P25V
 *           @arg HBN_BOD_THRES_2P30V
 *           @arg HBN_BOD_THRES_2P35V
 *           @arg HBN_BOD_THRES_2P40V
 * @param  mode: bod work mode with por, this parameter can be one of the following values:
 *           @arg HBN_BOD_MODE_POR_INDEPENDENT
 *           @arg HBN_BOD_MODE_POR_RELEVANT
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Set_BOD_Config(uint8_t enable, uint8_t threshold, uint8_t mode)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_BOD_THRES_TYPE(threshold));
    CHECK_PARAM(IS_HBN_BOD_MODE_TYPE(mode));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_BOR_CFG);
    if (enable) {
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PU_BOD, 1);
    } else {
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PU_BOD, 0);
    }
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_BOD_VTH, threshold);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_BOD_SEL, mode);
    QCC74x_WR_REG(HBN_BASE, HBN_BOR_CFG, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN get reset event
 *
 * @param[out]  event
 * [4] : bor_out_ event
 * [3] : pwr_rst_n event
 * [2] : sw_rst event
 * [1] : por_out event
 * [0] : watch dog reset
 *
*******************************************************************************/
void HBN_Get_Reset_Event(uint8_t *event)
{
    uint32_t tmpVal;
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    *event = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_RESET_EVENT);
}
/****************************************************************************/ /**
 * @brief  HBN clear reset event
 *
*******************************************************************************/
void HBN_Clr_Reset_Event(void)
{
    uint32_t tmpVal;
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal |= (1 << 13);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);
}

/****************************************************************************/ /**
 * @brief  HBN set ldo11aon voltage out
 *
 * @param  ldoLevel: LDO volatge level, this parameter can be one of the following values:
 *           @arg HBN_LDO_LEVEL_0P70V
 *           @arg HBN_LDO_LEVEL_0P75V
 *           @arg HBN_LDO_LEVEL_0P80V
 *           @arg HBN_LDO_LEVEL_0P85V
 *           @arg HBN_LDO_LEVEL_0P90V
 *           @arg HBN_LDO_LEVEL_0P95V
 *           @arg HBN_LDO_LEVEL_1P00V
 *           @arg HBN_LDO_LEVEL_1P05V
 *           @arg HBN_LDO_LEVEL_1P10V
 *           @arg HBN_LDO_LEVEL_1P15V
 *           @arg HBN_LDO_LEVEL_1P20V
 *           @arg HBN_LDO_LEVEL_1P25V
 *           @arg HBN_LDO_LEVEL_1P30V
 *           @arg HBN_LDO_LEVEL_1P35V
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Ldo11_Aon_Vout(uint8_t ldoLevel)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_LDO_LEVEL_TYPE(ldoLevel));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_SW_LDO11_AON_VOUT_SEL, ldoLevel);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN set ldo11rt voltage out
 *
 *
 * @param  ldoLevel: LDO volatge level, this parameter can be one of the following values:
 *           @arg HBN_LDO_LEVEL_0P70V
 *           @arg HBN_LDO_LEVEL_0P75V
 *           @arg HBN_LDO_LEVEL_0P80V
 *           @arg HBN_LDO_LEVEL_0P85V
 *           @arg HBN_LDO_LEVEL_0P90V
 *           @arg HBN_LDO_LEVEL_0P95V
 *           @arg HBN_LDO_LEVEL_1P00V
 *           @arg HBN_LDO_LEVEL_1P05V
 *           @arg HBN_LDO_LEVEL_1P10V
 *           @arg HBN_LDO_LEVEL_1P15V
 *           @arg HBN_LDO_LEVEL_1P20V
 *           @arg HBN_LDO_LEVEL_1P25V
 *           @arg HBN_LDO_LEVEL_1P30V
 *           @arg HBN_LDO_LEVEL_1P35V
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Ldo11_Rt_Vout(uint8_t ldoLevel)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_LDO_LEVEL_TYPE(ldoLevel));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_SW_LDO11_RT_VOUT_SEL, ldoLevel);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN set ldo11soc voltage out
 *
 * @param  ldoLevel: LDO volatge level, this parameter can be one of the following values:
 *           @arg HBN_LDO_LEVEL_0P70V
 *           @arg HBN_LDO_LEVEL_0P75V
 *           @arg HBN_LDO_LEVEL_0P80V
 *           @arg HBN_LDO_LEVEL_0P85V
 *           @arg HBN_LDO_LEVEL_0P90V
 *           @arg HBN_LDO_LEVEL_0P95V
 *           @arg HBN_LDO_LEVEL_1P00V
 *           @arg HBN_LDO_LEVEL_1P05V
 *           @arg HBN_LDO_LEVEL_1P10V
 *           @arg HBN_LDO_LEVEL_1P15V
 *           @arg HBN_LDO_LEVEL_1P20V
 *           @arg HBN_LDO_LEVEL_1P25V
 *           @arg HBN_LDO_LEVEL_1P30V
 *           @arg HBN_LDO_LEVEL_1P35V
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Ldo11_Soc_Vout(uint8_t ldoLevel)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_LDO_LEVEL_TYPE(ldoLevel));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_SW_LDO11SOC_VOUT_SEL_AON, ldoLevel);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN set ldo11 all voltage out
 *
 * @param  ldoLevel: LDO volatge level, this parameter can be one of the following values:
 *           @arg HBN_LDO_LEVEL_0P70V
 *           @arg HBN_LDO_LEVEL_0P75V
 *           @arg HBN_LDO_LEVEL_0P80V
 *           @arg HBN_LDO_LEVEL_0P85V
 *           @arg HBN_LDO_LEVEL_0P90V
 *           @arg HBN_LDO_LEVEL_0P95V
 *           @arg HBN_LDO_LEVEL_1P00V
 *           @arg HBN_LDO_LEVEL_1P05V
 *           @arg HBN_LDO_LEVEL_1P10V
 *           @arg HBN_LDO_LEVEL_1P15V
 *           @arg HBN_LDO_LEVEL_1P20V
 *           @arg HBN_LDO_LEVEL_1P25V
 *           @arg HBN_LDO_LEVEL_1P30V
 *           @arg HBN_LDO_LEVEL_1P35V
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Ldo11_All_Vout(uint8_t ldoLevel)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_LDO_LEVEL_TYPE(ldoLevel));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_SW_LDO11_AON_VOUT_SEL, ldoLevel);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_SW_LDO11_RT_VOUT_SEL, ldoLevel);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_SW_LDO11SOC_VOUT_SEL_AON, ldoLevel);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN select 32K
 *
 * @param  clkType: HBN 32k clock type, this parameter can be one of the following values:
 *           @arg HBN_32K_RC
 *           @arg HBN_32K_XTAL
 *           @arg HBN_32K_DIG
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_32K_Sel(uint8_t clkType)
{
    uint32_t tmpVal;

    /* Check the parameters */
    CHECK_PARAM(IS_HBN_32K_CLK_TYPE(clkType));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_F32K_SEL, clkType);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Select uart clock source
 *
 * @param  clkSel: uart clock type selection, this parameter can be one of the following values:
 *           @arg HBN_UART_CLK_MCU_BCLK
 *           @arg HBN_UART_CLK_MUXPLL_160M
 *           @arg HBN_UART_CLK_XCLK
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Set_UART_CLK_Sel(uint8_t clkSel)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_UART_CLK_TYPE(clkSel));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    if ((HBN_UART_CLK_MCU_BCLK == clkSel) || (HBN_UART_CLK_MUXPLL_160M == clkSel)) {
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_UART_CLK_SEL2, 0);
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_UART_CLK_SEL, clkSel);
    } else {
        /* mcu_bclk as default */
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_UART_CLK_SEL2, 1);
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_UART_CLK_SEL, 0);
    }

    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  get xclk clock source Select
 *
 * @param  None
 *
 * @return xclk clock type selection
 *
*******************************************************************************/
uint8_t ATTR_CLOCK_SECTION HBN_Get_MCU_XCLK_Sel(void)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL);

    return (uint8_t)(tmpVal & 0x1);
}

/****************************************************************************/ /**
 * @brief  Select xclk clock source
 *
 * @param  xclk: xclk clock type selection, this parameter can be one of the following values:
 *           @arg HBN_MCU_XCLK_RC32M
 *           @arg HBN_MCU_XCLK_XTAL
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Set_MCU_XCLK_Sel(uint8_t xclk)
{
    uint32_t tmpVal;
    uint32_t tmpVal2;

    CHECK_PARAM(IS_HBN_MCU_XCLK_TYPE(xclk));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal2 = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL);
    switch (xclk) {
        case HBN_MCU_XCLK_RC32M:
            tmpVal2 &= (~(1 << 0));
            break;
        case HBN_MCU_XCLK_XTAL:
            tmpVal2 |= (1 << 0);
            break;
        default:
            break;
    }
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL, tmpVal2);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);
    HBN_CLK_SET_DUMMY_WAIT;

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  get root clock selection
 *
 * @param  None
 *
 * @return root clock selection, this return value can be one of the following values:
 *           @arg HBN_MCU_ROOT_CLK_XCLK
 *           @arg HBN_MCU_ROOT_CLK_PLL
 *
*******************************************************************************/
uint8_t ATTR_CLOCK_SECTION HBN_Get_MCU_Root_CLK_Sel(void)
{
    uint32_t tmpVal = 0;
    uint32_t rootClkSel = 0;

    /* root_clock_select = hbn_root_clk_sel[1] */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    rootClkSel = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL) >> 1;
    if (0 == rootClkSel) {
        return HBN_MCU_ROOT_CLK_XCLK;
    } else {
        return HBN_MCU_ROOT_CLK_PLL;
    }
}

/****************************************************************************/ /**
 * @brief  Select root clk source
 *
 * @param  rootClk: root clock type selection, this parameter can be one of the following values:
 *           @arg HBN_MCU_ROOT_CLK_XCLK
 *           @arg HBN_MCU_ROOT_CLK_PLL
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Set_MCU_Root_CLK_Sel(uint8_t rootClk)
{
    uint32_t tmpVal;
    uint32_t tmpVal2;

    CHECK_PARAM(IS_HBN_MCU_ROOT_CLK_TYPE(rootClk));

    /* root_clock_select = hbn_root_clk_sel[1] */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal2 = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL);
    switch (rootClk) {
        case HBN_MCU_ROOT_CLK_XCLK:
            tmpVal2 &= 0x1; //[1]=0
            break;
        case HBN_MCU_ROOT_CLK_PLL:
            tmpVal2 |= 0x2; //[1]=1
            break;
        default:
            break;
    }
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_ROOT_CLK_SEL, tmpVal2);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);
    HBN_CLK_SET_DUMMY_WAIT;

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  set HBN_RAM sleep mode
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_HRAM_slp(void)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_SRAM);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_RETRAM_SLP);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_RETRAM_RET);
    QCC74x_WR_REG(HBN_BASE, HBN_SRAM, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  set HBN_RAM retension mode
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_HRAM_Ret(void)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_SRAM);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_RETRAM_SLP);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_RETRAM_RET);
    QCC74x_WR_REG(HBN_BASE, HBN_SRAM, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set xtal32k_capbank
 *
 * @param  value
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Set_Xtal_32K_Capbank(uint8_t value)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_XTAL32K);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_XTAL32K_CAPBANK, value);
    QCC74x_WR_REG(HBN_BASE, HBN_XTAL32K, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set xtal32k's inverter amplify strength
 *
 * @param  value
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Set_Xtal_32K_Inverter_Amplify_Strength(uint8_t value)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_XTAL32K);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_XTAL32K_INV_STRE, value);
    QCC74x_WR_REG(HBN_BASE, HBN_XTAL32K, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set xtal32k_regulator
 *
 * @param  level
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Set_Xtal_32K_Regulator(uint8_t level)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_XTAL32K);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_XTAL32K_REG, level);
    QCC74x_WR_REG(HBN_BASE, HBN_XTAL32K, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Power on XTAL 32K
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 * @note can't use GPIO16&17 after calling this function
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Power_On_Xtal_32K(void)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_XTAL32K);

    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_XTAL32K_HIZ_EN);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_XTAL32K_INV_STRE, 3);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_PU_XTAL32K);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_PU_XTAL32K_BUF);
    QCC74x_WR_REG(HBN_BASE, HBN_XTAL32K, tmpVal);

    /* Delay >1s */
    arch_delay_us(1100);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Power off XTAL 32K
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 * @note can use GPIO16&17 after calling this function
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Power_Off_Xtal_32K(void)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_XTAL32K);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_XTAL32K_HIZ_EN);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PU_XTAL32K);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PU_XTAL32K_BUF);
    QCC74x_WR_REG(HBN_BASE, HBN_XTAL32K, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Keep RC32K On during rtc power domain off
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 * @note  Don't turn off rc32k, which move to RTC Domain
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Keep_On_RC32K(void)
{
#ifndef BOOTROM
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_PU_RC32K);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Power off RC3K during rtc power domain off
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Power_Off_RC32K(void)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_GLB);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PU_RC32K);
    QCC74x_WR_REG(HBN_BASE, HBN_GLB, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Trim RC32K
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Trim_Ldo33VoutTrim(void)
{
    qcc74x_ef_ctrl_com_trim_t trim;
    int32_t tmpVal = 0;
    struct qcc74x_device_s *ef_ctrl;

    ef_ctrl = qcc74x_device_get_by_name("ef_ctrl");
    qcc74x_ef_ctrl_read_common_trim(ef_ctrl, "ldo33_trim", &trim, 1);
    if (trim.en) {
        if (trim.parity == qcc74x_ef_ctrl_get_trim_parity(trim.value, 4)) {
            tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_VBAT_LDO);
            tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_LDO33_VOUT_TRIM_AON, trim.value);
            QCC74x_WR_REG(HBN_BASE, HBN_VBAT_LDO, tmpVal);
            arch_delay_us(2);
            return SUCCESS;
        }
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Trim RC32K
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_CLOCK_SECTION HBN_Trim_RC32K(void)
{
    qcc74x_ef_ctrl_com_trim_t trim;
    int32_t tmpVal = 0;
    struct qcc74x_device_s *ef_ctrl;

    ef_ctrl = qcc74x_device_get_by_name("ef_ctrl");
    qcc74x_ef_ctrl_read_common_trim(ef_ctrl, "rc32k", &trim, 1);
    if (trim.en) {
        if (trim.parity == qcc74x_ef_ctrl_get_trim_parity(trim.value, 10)) {
            tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RC32K_CTRL0);
            tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_RC32K_CODE_FR_EXT, trim.value);
            tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_RC32K_EXT_CODE_EN);
            QCC74x_WR_REG(HBN_BASE, HBN_RC32K_CTRL0, tmpVal);
            arch_delay_us(2);
            return SUCCESS;
        }
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Get HBN status flag
 *
 * @param  None
 *
 * @return HBN status flag value
 *
*******************************************************************************/
uint32_t HBN_Get_Status_Flag(void)
{
    return QCC74x_RD_REG(HBN_BASE, HBN_RSV0);
}

/****************************************************************************/ /**
 * @brief  Set HBN status flag
 *
 * @param  flag: Status Flag
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Set_Status_Flag(uint32_t flag)
{
    QCC74x_WR_REG(HBN_BASE, HBN_RSV0, flag);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get HBN wakeup address
 *
 * @param  None
 *
 * @return HBN wakeup address
 *
*******************************************************************************/
uint32_t HBN_Get_Wakeup_Addr(void)
{
    return QCC74x_RD_REG(HBN_BASE, HBN_RSV1);
}

/****************************************************************************/ /**
 * @brief  Set HBN wakeup address
 *
 * @param  addr: HBN wakeup address
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Set_Wakeup_Addr(uint32_t addr)
{
    QCC74x_WR_REG(HBN_BASE, HBN_RSV1, addr);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get core reboot flag
 *
 * @param  None
 *
 * @return repower flag
 *
*******************************************************************************/
uint8_t ATTR_TCM_SECTION HBN_Get_Core_Unhalt_Config(void)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV2);
    if (HBN_RELEASE_CORE_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_RELEASE_CORE)) {
        return QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_CORE_UNHALT);
    }

    return 0x00;
}

/****************************************************************************/ /**
 * @brief  Set core reboot flag
 *
 * @param  core: core ID
 * @param  hcfg: reboot cfg
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Core_Reboot_Config(uint8_t core, uint8_t hcfg)
{
    uint32_t tmpVal = 0;
    uint8_t unhalt = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV2);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_RELEASE_CORE, HBN_RELEASE_CORE_FLAG);
    unhalt = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_CORE_UNHALT);
    if (hcfg) {
        unhalt |= (1 << core);
    } else {
        unhalt &= ~(1 << core);
    }
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_CORE_UNHALT, unhalt);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV2, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get user boot config
 *
 * @param  None
 *
 * @return user boot config
 *
*******************************************************************************/
uint8_t HBN_Get_User_Boot_Config(void)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV2);
    if (HBN_RELEASE_CORE_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_RELEASE_CORE)) {
        return QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_USER_BOOT_SEL);
    }

    return 0x00;
}

/****************************************************************************/ /**
 * @brief  Set user boot config
 *
 * @param  ubCfg: user boot config
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Set_User_Boot_Config(uint8_t ubCfg)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV2);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_RELEASE_CORE, HBN_RELEASE_CORE_FLAG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_USER_BOOT_SEL, ubCfg);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV2, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get ldo18io power delay config
 *
 * @param  pwrOffDly: power off delay
 * @param  pwrOnDly: power on delay
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_Ldo18io_Power_Delay_Config(uint16_t *pwrOffDly, uint16_t *pwrOnDly)
{
    uint32_t tmpVal = 0;

    if ((NULL == pwrOffDly) || (NULL == pwrOnDly)) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV2);
    if (HBN_LDO18IO_POWER_DLY_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_LDO18IO_POWER_DLY_STS)) {
        *pwrOffDly = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_LDO18IO_POWER_OFF_DLY);
        *pwrOnDly = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_LDO18IO_POWER_ON_DLY);
        return SUCCESS;
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Set ldo18io power delay config
 *
 * @param  pwrOffDly: power off delay
 * @param  pwrOnDly: power on delay
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Ldo18io_Power_Delay_Config(uint16_t pwrOffDly, uint16_t pwrOnDly)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV2);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_LDO18IO_POWER_DLY_STS, HBN_LDO18IO_POWER_DLY_FLAG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_LDO18IO_POWER_OFF_DLY, pwrOffDly);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_LDO18IO_POWER_ON_DLY, pwrOnDly);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV2, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set Xtal type
 *
 * @param  xtalType:Xtal type
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Xtal_Type(uint8_t xtalType)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_XTAL_STS, HBN_XTAL_FLAG_VALUE);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_XTAL_TYPE, xtalType);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV3, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get Xtal type
 *
 * @param  xtalType:Xtal type
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_Xtal_Type(uint8_t *xtalType)
{
    uint32_t tmpVal = 0;

    if (NULL == xtalType) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    if (HBN_XTAL_FLAG_VALUE == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_XTAL_STS)) {
        *xtalType = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_XTAL_TYPE);
        return SUCCESS;
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Get Xtal value
 *
 * @param  xtalVal:Xtal value
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_Xtal_Value(uint32_t *xtalVal)
{
    uint32_t tmpVal = 0;
    uint8_t xtalType = 0;

    if (NULL == xtalVal) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    if (HBN_XTAL_FLAG_VALUE == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_XTAL_STS)) {
        xtalType = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_XTAL_TYPE);
        switch (xtalType) {
            case GLB_XTAL_NONE:
                *xtalVal = 0;
                break;
            case GLB_XTAL_24M:
                *xtalVal = 24000000;
                break;
            case GLB_XTAL_32M:
                *xtalVal = 32000000;
                break;
            case GLB_XTAL_38P4M:
                *xtalVal = 38400000;
                break;
            case GLB_XTAL_40M:
                *xtalVal = 40000000;
                break;
            case GLB_XTAL_26M:
                *xtalVal = 26000000;
                break;
            case GLB_XTAL_RC32M:
                *xtalVal = 32000000;
                break;
            default:
                *xtalVal = 0;
                break;
        }
        return SUCCESS;
    }

    *xtalVal = 0;
    return ERROR;
}

#if 0
/****************************************************************************/ /**
 * @brief  Set Flash Power Delay
 *
 * @param  flashPwrDly:flash power delay
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Flash_Power_Delay(uint8_t flashPwrDly)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_STS, HBN_FLASH_POWER_DLY_FLAG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_DLY, flashPwrDly);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV3, tmpVal);

    return SUCCESS;
}
#endif

/****************************************************************************/ /**
 * @brief  Get Flash Power Delay
 *
 * @param  flashPwrDly:flash power delay
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_Flash_Power_Delay(uint8_t *flashPwrDly)
{
    uint32_t tmpVal = 0;

    if (NULL == flashPwrDly) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    if (HBN_FLASH_POWER_DLY_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_STS)) {
        *flashPwrDly = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_DLY);
        return SUCCESS;
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Set Reset Reason
 *
 * @param  rstReason:Reset Reason
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Reset_Reason(uint16_t rstReason)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_STS, HBN_RESET_REASON_FLAG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_DLY, rstReason);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV3, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set Reset Reason
 *
 * @param  flashPwrDly:flash power delay
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_Reset_Reason(uint16_t *rstReason)
{
    uint32_t tmpVal = 0;

    if (NULL == rstReason) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    if (HBN_RESET_REASON_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_STS)) {
        *rstReason = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_FLASH_POWER_DLY);
        return SUCCESS;
    }

    return ERROR;

}
/****************************************************************************/ /**
 * @brief  Set HBN Gpio Keep
 *
 * @param  gpioKeep:HBN gpio keep reg cfg
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_Gpio_Keep(uint8_t gpioKeep)
{
#ifndef BOOTROM
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_GPIO_KEEP_STS, HBN_GPIO_KEEP_FLAG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_GPIO_KEEP_PIN, gpioKeep);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV3, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get HBN GPIO Keep bit
 *
 * @param  gpioKeep:HBN gpio keep reg cfg
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_Gpio_Keep(uint8_t *gpioKeep)
{
    uint32_t tmpVal = 0;

    if (NULL == gpioKeep) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    if (HBN_GPIO_KEEP_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_GPIO_KEEP_STS)) {
        *gpioKeep = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_GPIO_KEEP_PIN);
        return SUCCESS;
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Clear HBN GPIO Keep bit
 *
 * @param  gpioKeep:HBN gpio keep reg cfg
 *
 * @return SUCCESS
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Clear_Gpio_Keep(uint8_t gpioKeep)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_PAD_CTRL_0);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_CR_GPIO_KEEP_EN, (gpioKeep & 0x7));
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_REG_AON_GPIO_ISO_MODE, ((gpioKeep >> 3) & 0x1));
    QCC74x_WR_REG(HBN_BASE, HBN_PAD_CTRL_0, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set PDS Gpio Keep
 *
 * @param  gpioKeep:PDS gpio keep reg cfg
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_PDS_Gpio_Keep(uint8_t gpioKeep)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, PDS_GPIO_KEEP_STS, PDS_GPIO_KEEP_FLAG);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, PDS_GPIO_KEEP_PIN, gpioKeep);
    QCC74x_WR_REG(HBN_BASE, HBN_RSV3, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Get PDS GPIO Keep bit
 *
 * @param  gpioKeep:PDS gpio keep reg cfg
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_PDS_Gpio_Keep(uint8_t *gpioKeep)
{
    uint32_t tmpVal = 0;

    if (NULL == gpioKeep) {
        return ERROR;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RSV3);
    if (PDS_GPIO_KEEP_FLAG == QCC74x_GET_REG_BITS_VAL(tmpVal, PDS_GPIO_KEEP_STS)) {
        *gpioKeep = QCC74x_GET_REG_BITS_VAL(tmpVal, PDS_GPIO_KEEP_PIN);
        return SUCCESS;
    }

    return ERROR;
}

/****************************************************************************/ /**
 * @brief  Clear PDS GPIO Keep bit
 *
 * @param  gpioKeep:PDS gpio keep reg cfg
 *
 * @return SUCCESS
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Clear_PDS_Gpio_Keep(uint8_t gpioKeep)
{
    uint32_t tmpVal = 0;

    tmpVal = QCC74x_RD_REG(PDS_BASE, PDS_CTL5);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, PDS_CR_PDS_GPIO_KEEP_EN, (gpioKeep & 0x7));
    QCC74x_WR_REG(PDS_BASE, PDS_CTL5, tmpVal);

    tmpVal = QCC74x_RD_REG(PDS_BASE, PDS_CTL);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, PDS_CR_PDS_GPIO_ISO_MODE, ((gpioKeep >> 3) & 0x1));
    /* don't entry PDS */
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, PDS_START_PS);
    QCC74x_WR_REG(PDS_BASE, PDS_CTL, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN clear RTC timer counter
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Clear_RTC_Counter(void)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    /* Clear RTC control bit0 */
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal & 0xfffffffe);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN clear RTC timer counter
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Enable_RTC_Counter(void)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    /* Set RTC control bit0 */
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal | 0x01);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN set RTC timer configuration
 *
 * @param  delay: RTC interrupt delay 32 clocks, this parameter can be one of the following values:
 *           @arg HBN_RTC_INT_DELAY_32T
 *           @arg HBN_RTC_INT_DELAY_0T
 * @param  compValLow: RTC interrupt commpare value low 32 bits
 * @param  compValHigh: RTC interrupt commpare value high 32 bits
 * @param  compMode: RTC interrupt commpare
 *                   mode:HBN_RTC_COMP_BIT0_39,HBN_RTC_COMP_BIT0_23,HBN_RTC_COMP_BIT13_39
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Set_RTC_Timer(uint8_t delay, uint32_t compValLow, uint32_t compValHigh, uint8_t compMode)
{
    uint32_t tmpVal;

    /* Check the parameters */
    CHECK_PARAM(IS_HBN_RTC_INT_DELAY_TYPE(delay));

    QCC74x_WR_REG(HBN_BASE, HBN_TIME_L, compValLow);
    QCC74x_WR_REG(HBN_BASE, HBN_TIME_H, compValHigh & 0xff);

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    /* Set interrupt delay option */
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_RTC_DLY_OPTION, delay);
    /* Set RTC compare mode */
    tmpVal |= (compMode << 1);
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN get RTC timer count value
 *
 * @param  valLow: RTC count value pointer for low 32 bits
 * @param  valHigh: RTC count value pointer for high 8 bits
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Get_RTC_Timer_Val(uint32_t *valLow, uint32_t *valHigh)
{
    uint32_t tmpVal;

    /* Tigger RTC val read */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RTC_TIME_H);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_RTC_TIME_LATCH);
    QCC74x_WR_REG(HBN_BASE, HBN_RTC_TIME_H, tmpVal);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_RTC_TIME_LATCH);
    QCC74x_WR_REG(HBN_BASE, HBN_RTC_TIME_H, tmpVal);

    /* Read RTC val */
    *valLow = QCC74x_RD_REG(HBN_BASE, HBN_RTC_TIME_L);
    *valHigh = (QCC74x_RD_REG(HBN_BASE, HBN_RTC_TIME_H) & 0xff);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN Re-Cal RC32K
 *
 * @param  expected_counter: Expected rtc counter
 * @param  actual_counter: Actual rtc counter
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Recal_RC32K(int32_t expected_counter, int32_t actual_counter)
{
    int32_t tmpVal = 0;
    int32_t current = 0;
    int32_t delta = 0;

    delta = actual_counter - expected_counter;

    /* normalize to 1s count */
    delta = (delta * 32768) / expected_counter;

    if ((delta < 32) && (delta > -32)) {
        return -1;
    }
    if (delta < -320) {
        delta = -320;
    } else if (delta > 320) {
        delta = 320;
    }

    if (delta >= 64 || delta <= -64) {
        delta = delta / 64;
    } else {
        delta = delta / 32;
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_RC32K_CTRL0);
    current = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_RC32K_CODE_FR_EXT);
    current += delta;
    current &= ((1U << HBN_RC32K_CODE_FR_EXT_LEN) - 1);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_RC32K_CODE_FR_EXT, current);
    QCC74x_WR_REG(HBN_BASE, HBN_RC32K_CTRL0, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN clear RTC timer interrupt,this function must be called to clear delayed rtc IRQ
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Clear_RTC_INT(void)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_CTL);
    /* Clear RTC commpare:bit1-3 for clearing Delayed RTC IRQ */
    QCC74x_WR_REG(HBN_BASE, HBN_CTL, tmpVal & 0xfffffff1);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN enable GPIO interrupt
 *
 * @param  gpioIntTrigType: HBN GPIO interrupt trigger type, this parameter can be one of the following values:
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_FALLING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_RISING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_LOW_LEVEL
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_HIGH_LEVEL
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_RISING_FALLING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_FALLING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_RISING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_LOW_LEVEL
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_HIGH_LEVEL
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_GPIO_INT_Enable(uint8_t gpioIntTrigType)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    /* Check the parameters */
    CHECK_PARAM(IS_HBN_GPIO_INT_TRIGGER_TYPE(gpioIntTrigType));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_MODE, gpioIntTrigType);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN disable GPIO interrupt
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_GPIO_INT_Disable(void)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_MASK, 0);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN get interrupt status
 *
 * @param  irqType: HBN interrupt type, this parameter can be one of the following values:
 *           @arg HBN_INT_GPIO16
 *           @arg HBN_INT_GPIO17
 *           @arg HBN_INT_GPIO18
 *           @arg HBN_INT_GPIO19
 *           @arg HBN_INT_RTC
 *           @arg HBN_INT_BOD
 *           @arg HBN_INT_ACOMP0
 *           @arg HBN_INT_ACOMP1
 *
 * @return SET or RESET
 *
*******************************************************************************/
QCC74x_Sts_Type HBN_Get_INT_State(uint8_t irqType)
{
    uint32_t tmpVal;

    /* Check the parameters */
    CHECK_PARAM(IS_HBN_INT_TYPE(irqType));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_STAT);

    if (tmpVal & (1 << irqType)) {
        return SET;
    } else {
        return RESET;
    }
}

/****************************************************************************/ /**
 * @brief  HBN get pin wakeup mode value
 *
 * @param  None
 *
 * @return HBN pin wakeup mode value
 *
*******************************************************************************/
uint8_t HBN_Get_Pin_Wakeup_Mode(void)
{
    return QCC74x_GET_REG_BITS_VAL(QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE), HBN_PIN_WAKEUP_MODE);
}

/****************************************************************************/ /**
 * @brief  HBN clear interrupt status
 *
 * @param  irqType: HBN interrupt type, this parameter can be one of the following values:
 *           @arg HBN_INT_GPIO16
 *           @arg HBN_INT_GPIO17
 *           @arg HBN_INT_GPIO18
 *           @arg HBN_INT_GPIO19
 *           @arg HBN_INT_RTC
 *           @arg HBN_INT_BOD
 *           @arg HBN_INT_ACOMP0
 *           @arg HBN_INT_ACOMP1
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Clear_IRQ(uint8_t irqType)
{
    uint32_t tmpVal;

    CHECK_PARAM(IS_HBN_INT_TYPE(irqType));

    /* set clear bit */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_CLR);
    tmpVal |= (1 << irqType);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_CLR, tmpVal);

    /* unset clear bit */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_CLR);
    tmpVal &= (~(1 << irqType));
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_CLR, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN hardware pullup or pulldown configuration
 *
 * @param  enable: ENABLE or DISABLE
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Hw_Pu_Pd_Cfg(uint8_t enable)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    if (enable) {
        tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_REG_EN_HW_PU_PD);
    } else {
        tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_REG_EN_HW_PU_PD);
    }
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN wakeup pin mask configuration
 *
 * @param  maskVal: mask value
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Pin_WakeUp_Mask(uint8_t maskVal)
{
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_MASK, maskVal);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN aon pad ctrl configuration
 *
 * @param  aonPadCtl1: ctrl value 1
 * @param  aonpadCtl2: ctrl value 2
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Aon_Pad_Ctrl(uint32_t aonPadCtl1, uint32_t aonPadCtl2)
{
#ifndef BOOTROM
    QCC74x_WR_REG(HBN_BASE, HBN_PAD_CTRL_0, aonPadCtl1);
    QCC74x_WR_REG(HBN_BASE, HBN_PAD_CTRL_1, aonPadCtl2);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN aon pad cfg configuration
 *
 * @param  aonPadHwCtrlEn: hw control aon pad enable
 * @param  aonGpio: aon pad number, this parameter can be one of the following values:
 *           @arg HBN_AON_PAD_GPIO16
 *           @arg HBN_AON_PAD_GPIO17
 *           @arg HBN_AON_PAD_GPIO18
 *           @arg HBN_AON_PAD_GPIO19
 * @param  aonPadCfg: aon pad configuration
 *
 * @return SUCCESS or ERROR
 *
 * @note   Pu and Pd not depend on IE. IE ,Pu and Pd depend on ctrlEn
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Aon_Pad_Cfg(uint8_t aonPadHwCtrlEn, uint8_t aonGpio, HBN_AON_PAD_CFG_Type *aonPadCfg)
{
    uint32_t tmpVal0;
    uint32_t tmpVal1;
    uint32_t enAonCtrlGpio;
    uint32_t aonPadIeSmt;
    uint32_t aonPadPu;
    uint32_t aonPadPd;
    uint32_t aonPadOe;

    CHECK_PARAM(IS_HBN_AON_PAD_TYPE(aonGpio));

    if (GLB_PACKAGE_TYPE_QFN56 != GLB_Get_Package_Type()) {
        if ((aonGpio == HBN_AON_PAD_GPIO18) || (aonGpio == HBN_AON_PAD_GPIO19)) {
            return ERROR;
        }
    }

    if (NULL == aonPadCfg) {
        return ERROR;
    }

    /* hbn_pad_ctrl_0 */
    tmpVal0 = QCC74x_RD_REG(HBN_BASE, HBN_PAD_CTRL_0);
    enAonCtrlGpio = QCC74x_GET_REG_BITS_VAL(tmpVal0, HBN_REG_EN_AON_CTRL_GPIO);
    if (aonPadCfg->ctrlEn) {
        enAonCtrlGpio |= (1 << aonGpio);
    } else {
        enAonCtrlGpio &= ~(1 << aonGpio);
    }
    tmpVal0 = QCC74x_SET_REG_BITS_VAL(tmpVal0, HBN_REG_EN_AON_CTRL_GPIO, enAonCtrlGpio);
    aonPadIeSmt = QCC74x_GET_REG_BITS_VAL(tmpVal0, HBN_REG_AON_PAD_IE_SMT);
    if (aonPadCfg->ie) {
        aonPadIeSmt |= (1 << aonGpio);
    } else {
        aonPadIeSmt &= ~(1 << aonGpio);
    }
    tmpVal0 = QCC74x_SET_REG_BITS_VAL(tmpVal0, HBN_REG_AON_PAD_IE_SMT, aonPadIeSmt);
    QCC74x_WR_REG(HBN_BASE, HBN_PAD_CTRL_0, tmpVal0);

    /* hbn_pad_ctrl_1 */
    tmpVal1 = QCC74x_RD_REG(HBN_BASE, HBN_PAD_CTRL_1);
    aonPadPu = QCC74x_GET_REG_BITS_VAL(tmpVal1, HBN_REG_AON_PAD_PU);
    if (aonPadCfg->pullUp) {
        aonPadPu |= (1 << aonGpio);
    } else {
        aonPadPu &= ~(1 << aonGpio);
    }
    tmpVal1 = QCC74x_SET_REG_BITS_VAL(tmpVal1, HBN_REG_AON_PAD_PU, aonPadPu);
    aonPadPd = QCC74x_GET_REG_BITS_VAL(tmpVal1, HBN_REG_AON_PAD_PD);
    if (aonPadCfg->pullDown) {
        aonPadPd |= (1 << aonGpio);
    } else {
        aonPadPd &= ~(1 << aonGpio);
    }
    tmpVal1 = QCC74x_SET_REG_BITS_VAL(tmpVal1, HBN_REG_AON_PAD_PD, aonPadPd);
    aonPadOe = QCC74x_GET_REG_BITS_VAL(tmpVal1, HBN_REG_AON_PAD_OE);
    if (aonPadCfg->oe) {
        aonPadOe |= (1 << aonGpio);
    } else {
        aonPadOe &= ~(1 << aonGpio);
    }
    tmpVal1 = QCC74x_SET_REG_BITS_VAL(tmpVal1, HBN_REG_AON_PAD_OE, aonPadOe);
    QCC74x_WR_REG(HBN_BASE, HBN_PAD_CTRL_1, tmpVal1);

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN Set AON_HW control aon pad or not
 *
 * @param  aonPadHwCtrlEn: hw control aon pad enable
 * @param  aonGpio: aon pad number, this parameter can be one of the following values:
 *           @arg HBN_AON_PAD_GPIO16
 *           @arg HBN_AON_PAD_GPIO17
 *           @arg HBN_AON_PAD_GPIO18
 *           @arg HBN_AON_PAD_GPIO19
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type ATTR_TCM_SECTION HBN_Aon_Pad_Cfg_Set(uint8_t aonPadHwCtrlEn, uint8_t aonGpio)
{
#ifndef BOOTROM
    uint32_t tmpVal;
    uint32_t enAonCtrlGpio;

    CHECK_PARAM(IS_HBN_AON_PAD_TYPE(aonGpio));

    /* hbn_pad_ctrl_0 */
    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_PAD_CTRL_0);
    enAonCtrlGpio = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_REG_EN_AON_CTRL_GPIO);

    if (aonPadHwCtrlEn) {
        enAonCtrlGpio |= (1 << aonGpio);
    } else {
        enAonCtrlGpio &= ~(1 << aonGpio);
    }
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_REG_EN_AON_CTRL_GPIO, enAonCtrlGpio);

    QCC74x_WR_REG(HBN_BASE, HBN_PAD_CTRL_0, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN enable ACOMP interrupt
 *
 * @param  acompId: HBN Acomp ID
 * @param  edge: HBN acomp interrupt edge type, this parameter can be one of the following values:
 *           @arg HBN_ACOMP_INT_EDGE_POSEDGE
 *           @arg HBN_ACOMP_INT_EDGE_NEGEDGE
 *           @arg HBN_ACOMP_INT_EDGE_POSEDGE_NEGEDGE
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Enable_AComp_IRQ(uint8_t acompId, uint8_t edge)
{
    uint32_t tmpVal;
    uint32_t tmpVal2;

    CHECK_PARAM(IS_AON_ACOMP_ID_TYPE(acompId));
    CHECK_PARAM(IS_HBN_ACOMP_INT_EDGE_TYPE(edge));

    if (acompId == AON_ACOMP0_ID) {
        tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
        tmpVal2 = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP0_EN);
        tmpVal2 = tmpVal2 | edge;
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP0_EN, tmpVal2);
        QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
    } else if (acompId == AON_ACOMP1_ID) {
        tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
        tmpVal2 = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP1_EN);
        tmpVal2 = tmpVal2 | edge;
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP1_EN, tmpVal2);
        QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
    }

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN disable ACOMP interrupt
 *
 * @param  acompId: HBN Acomp ID
 * @param  edge: HBN acomp interrupt edge type, this parameter can be one of the following values:
 *           @arg HBN_ACOMP_INT_EDGE_POSEDGE
 *           @arg HBN_ACOMP_INT_EDGE_NEGEDGE
 *           @arg HBN_ACOMP_INT_EDGE_POSEDGE_NEGEDGE
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Disable_AComp_IRQ(uint8_t acompId, uint8_t edge)
{
    uint32_t tmpVal;
    uint32_t tmpVal2;

    CHECK_PARAM(IS_AON_ACOMP_ID_TYPE(acompId));
    CHECK_PARAM(IS_HBN_ACOMP_INT_EDGE_TYPE(edge));

    if (acompId == (uint8_t)AON_ACOMP0_ID) {
        tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
        tmpVal2 = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP0_EN);
        tmpVal2 = tmpVal2 & (~edge);
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP0_EN, tmpVal2);
        QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
    } else if (acompId == (uint8_t)AON_ACOMP1_ID) {
        tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
        tmpVal2 = QCC74x_GET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP1_EN);
        tmpVal2 = tmpVal2 & (~edge);
        tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_IRQ_ACOMP1_EN, tmpVal2);
        QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
    }

    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN enable BOD interrupt
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Enable_BOD_IRQ(void)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_IRQ_BOR_EN);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN disable BOD interrupt
 *
 * @param  None
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Disable_BOD_IRQ(void)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_IRQ_BOR_EN);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN out0 install interrupt callback
 *
 * @param  intType: HBN out0 interrupt type, this parameter can be one of the following values:
 *           @arg HBN_OUT0_INT_GPIO16
 *           @arg HBN_OUT0_INT_GPIO17
 *           @arg HBN_OUT0_INT_GPIO18
 *           @arg HBN_OUT0_INT_GPIO19
 *           @arg HBN_OUT0_INT_RTC
 *           @arg HBN_OUT0_INT_MAX
 * @param  cbFun: HBN out0 interrupt callback
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
#ifndef QCC74x_USE_HAL_DRIVER
QCC74x_Err_Type HBN_Out0_Callback_Install(uint8_t intType, intCallback_Type *cbFun)
{
    /* Check the parameters */
    CHECK_PARAM(IS_HBN_OUT0_INT_TYPE(intType));

#ifndef QCC74x_USE_HAL_DRIVER
    Interrupt_Handler_Register(HBN_OUT0_IRQn, HBN_OUT0_IRQHandler);
#endif

    hbnInt0CbfArra[intType] = cbFun;

    return SUCCESS;
}
#endif

/****************************************************************************/ /**
 * @brief  HBN out1 install interrupt callback
 *
 * @param  intType: HBN out1 interrupt type, this parameter can be one of the following values:
 *           @arg HBN_OUT1_INT_BOD
 *           @arg HBN_OUT1_INT_ACOMP0
 *           @arg HBN_OUT1_INT_ACOMP1
 *           @arg HBN_OUT1_INT_MAX
 * @param  cbFun: HBN out1 interrupt callback
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
#ifndef QCC74x_USE_HAL_DRIVER
QCC74x_Err_Type HBN_Out1_Callback_Install(uint8_t intType, intCallback_Type *cbFun)
{
    /* Check the parameters */
    CHECK_PARAM(IS_HBN_OUT1_INT_TYPE(intType));

#ifndef QCC74x_USE_HAL_DRIVER
    Interrupt_Handler_Register(HBN_OUT1_IRQn, HBN_OUT1_IRQHandler);
#endif

    hbnInt1CbfArra[intType] = cbFun;

    return SUCCESS;
}
#endif

/****************************************************************************/ /**
 * @brief  HBN aon pad debbug pull config
 *
 * @param  puPdEn: Enable or disable aon pad pull down and pull up
 * @param  trigMode: trigger mode, this parameter can be one of the following values:
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_FALLING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_RISING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_LOW_LEVEL
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_HIGH_LEVEL
 *           @arg HBN_GPIO_INT_TRIGGER_SYNC_RISING_FALLING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_FALLING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_RISING_EDGE
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_LOW_LEVEL
 *           @arg HBN_GPIO_INT_TRIGGER_ASYNC_HIGH_LEVEL
 * @param  maskVal: int mask
 * @param  dlyEn: Enable or disable aon pad wakeup delay function
 * @param  dlySec: aon pad wakeup delay sec 1 to 7
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Aon_Pad_WakeUpCfg(QCC74x_Fun_Type puPdEn, uint8_t trigMode, uint32_t maskVal, QCC74x_Fun_Type dlyEn, uint8_t dlySec)
{
#ifndef BOOTROM
    uint32_t tmpVal;

    CHECK_PARAM(((dlySec >= 1) && (dlySec <= 7)));
    CHECK_PARAM(IS_HBN_GPIO_INT_TRIGGER_TYPE(trigMode));
    CHECK_PARAM((maskVal <= 0x3FF));

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_EN, dlyEn);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_SEL, dlySec);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_REG_EN_HW_PU_PD, puPdEn);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_MASK, maskVal);
    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_PIN_WAKEUP_MODE, trigMode);
    QCC74x_WR_REG(HBN_BASE, HBN_IRQ_MODE, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  Set Embedded Flash Pullup enabe or disable
 *
 * @param  cfg: Enable or disable
 *
 * @return SUCCESS or ERROR
 *
*******************************************************************************/
QCC74x_Err_Type HBN_Set_BOD_Cfg(HBN_BOD_CFG_Type *cfg)
{
#ifndef BOOTROM
    uint32_t tmpVal = 0;

    if (cfg->enableBodInt) {
        HBN_Enable_BOD_IRQ();
    } else {
        HBN_Disable_BOD_IRQ();
    }

    tmpVal = QCC74x_RD_REG(HBN_BASE, HBN_BOR_CFG);

    tmpVal = QCC74x_SET_REG_BITS_VAL(tmpVal, HBN_BOD_VTH, cfg->bodThreshold);
    if (cfg->enablePorInBod) {
        tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_BOD_SEL);
    } else {
        tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_BOD_SEL);
    }

    if (cfg->enableBod) {
        tmpVal = QCC74x_SET_REG_BIT(tmpVal, HBN_PU_BOD);
    } else {
        tmpVal = QCC74x_CLR_REG_BIT(tmpVal, HBN_PU_BOD);
    }

    QCC74x_WR_REG(HBN_BASE, HBN_BOR_CFG, tmpVal);
#endif
    return SUCCESS;
}

/****************************************************************************/ /**
 * @brief  HBN OUT0 interrupt handler
 *
 * @param  None
 *
 * @return None
 *
*******************************************************************************/
#ifndef QCC74x_USE_HAL_DRIVER
void HBN_OUT0_IRQHandler(void)
{
    uint8_t out0Int = HBN_OUT0_INT_GPIO16;

    /* GPIO and RTC */
    for (uint8_t gpioInt = HBN_INT_GPIO16; gpioInt <= HBN_INT_GPIO19; gpioInt++) {
        if (SET == HBN_Get_INT_State(gpioInt)) {
            HBN_Clear_IRQ(gpioInt);
            switch (gpioInt) {
                case HBN_INT_GPIO16:
                    out0Int = HBN_OUT0_INT_GPIO16;
                    break;
                case HBN_INT_GPIO17:
                    out0Int = HBN_OUT0_INT_GPIO17;
                    break;
                case HBN_INT_GPIO18:
                    out0Int = HBN_OUT0_INT_GPIO18;
                    break;
                case HBN_INT_GPIO19:
                    out0Int = HBN_OUT0_INT_GPIO19;
                    break;
                default:
                    break;
            }
            if (hbnInt0CbfArra[out0Int] != NULL) {
                hbnInt0CbfArra[out0Int]();
            }
        }
    }
    if (SET == HBN_Get_INT_State(HBN_INT_RTC)) {
        HBN_Clear_IRQ(HBN_INT_RTC);
        if (hbnInt0CbfArra[HBN_OUT0_INT_RTC] != NULL) {
            hbnInt0CbfArra[HBN_OUT0_INT_RTC]();
        }
    }
}
#endif

/****************************************************************************/ /**
 * @brief  HBN OUT1 interrupt handler
 *
 * @param  None
 *
 * @return None
 *
*******************************************************************************/
#ifndef QCC74x_USE_HAL_DRIVER
void HBN_OUT1_IRQHandler(void)
{
    /* BOD */
    if (SET == HBN_Get_INT_State(HBN_INT_BOD)) {
        HBN_Clear_IRQ(HBN_INT_BOD);
        if (hbnInt1CbfArra[HBN_OUT1_INT_BOD] != NULL) {
            hbnInt1CbfArra[HBN_OUT1_INT_BOD]();
        }
    }
    /* ACOMP0 */
    if (SET == HBN_Get_INT_State(HBN_INT_ACOMP0)) {
        HBN_Clear_IRQ(HBN_INT_ACOMP0);
        if (hbnInt1CbfArra[HBN_OUT1_INT_ACOMP0] != NULL) {
            hbnInt1CbfArra[HBN_OUT1_INT_ACOMP0]();
        }
    }
    /* ACOMP1 */
    if (SET == HBN_Get_INT_State(HBN_INT_ACOMP1)) {
        HBN_Clear_IRQ(HBN_INT_ACOMP1);
        if (hbnInt1CbfArra[HBN_OUT1_INT_ACOMP1] != NULL) {
            hbnInt1CbfArra[HBN_OUT1_INT_ACOMP1]();
        }
    }
}
#endif

/*@} end of group HBN_Public_Functions */

/*@} end of group HBN */

/*@} end of group QCC743_Peripheral_Driver */
