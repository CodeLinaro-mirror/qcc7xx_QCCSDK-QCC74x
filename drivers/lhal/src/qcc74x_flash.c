#if defined(QCC74x_undef)
#include "qcc74x_undef_memorymap.h"
#include "qcc74x_undef_glb.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_memorymap.h"
#include "qcc74x_undef_glb.h"
#elif defined(QCC743)
#include "qcc743_memorymap.h"
#include "qcc743_glb.h"
#include "qcc743_ef_cfg.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_memorymap.h"
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_ef_cfg.h"
#include "qcc74x_undef_l1c.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_memorymap.h"
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_ef_cfg.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_memorymap.h"
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_ef_cfg.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_ef_ctrl.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_sflash_ext.h"
#include "qcc74x_undef_xip_sflash_ext.h"
#include "qcc74x_undef_sf_cfg_ext.h"
#elif defined(QCC74x_undef)
#include "qcc74x_undef_glb.h"
#include "qcc74x_undef_xip_sflash_ext.h"
#include "qcc74x_undef_sf_cfg_ext.h"
#endif
#include "qcc74x_xip_sflash.h"
#include "qcc74x_sf_cfg.h"
#include "qcc74x_sflash.h"
#include "qcc74x_flash.h"
#include "hardware/sf_ctrl_reg.h"
#include "qcc74x_efuse.h"

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
static uint32_t flash1_size = 4 * 1024 * 1024;
static uint32_t flash2_size = 2 * 1024 * 1024;
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
static uint32_t g_jedec_id2 = 0;
#endif
#endif
static uint32_t g_jedec_id = 0;

__WEAK int ATTR_TCM_SECTION qcc74x_flash_resource_lock(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return -ENOSYS;
}

__WEAK void ATTR_TCM_SECTION qcc74x_flash_resource_unlock(void)
{
}

static int ATTR_TCM_SECTION qcc74x_flash_op_enter(uintptr_t *irq_flags)
{
#ifndef CONFIG_DISABLE_FLASH_OP_IRQ_SAVE_RESTORE
    *irq_flags = qcc74x_irq_save();
    return 0;
#else
    (void)irq_flags;
    return qcc74x_flash_resource_lock(QCC74x_FLASH_RESOURCE_WAIT_FOREVER);
#endif
}

static void ATTR_TCM_SECTION qcc74x_flash_op_exit(uintptr_t irq_flags)
{
#ifndef CONFIG_DISABLE_FLASH_OP_IRQ_SAVE_RESTORE
    qcc74x_irq_restore(irq_flags);
#else
    (void)irq_flags;
    qcc74x_flash_resource_unlock();
#endif
}

static spi_flash_cfg_type g_flash_cfg = {
    .reset_c_read_cmd = 0xff,
    .reset_c_read_cmd_size = 3,
    .mid = 0xc8,

    .de_burst_wrap_cmd = 0x77,
    .de_burst_wrap_cmd_dmy_clk = 0x3,
    .de_burst_wrap_data_mode = SF_CTRL_DATA_4_LINES,
    .de_burst_wrap_data = 0xF0,

    /*reg*/
    .write_enable_cmd = 0x06,
    .wr_enable_index = 0x00,
    .wr_enable_bit = 0x01,
    .wr_enable_read_reg_len = 0x01,

    .qe_index = 1,
    .qe_bit = 0x01,
    .qe_write_reg_len = 0x01,
    .qe_read_reg_len = 0x1,

    .busy_index = 0,
    .busy_bit = 0x00,
    .busy_read_reg_len = 0x1,
    .release_powerdown = 0xab,

    .read_reg_cmd[0] = 0x05,
    .read_reg_cmd[1] = 0x35,
    .write_reg_cmd[0] = 0x01,
    .write_reg_cmd[1] = 0x31,

    .fast_read_qio_cmd = 0xeb,
    .fr_qio_dmy_clk = 16 / 8,
    .c_read_support = 0,
    .c_read_mode = 0x20,

    .burst_wrap_cmd = 0x77,
    .burst_wrap_cmd_dmy_clk = 0x3,
    .burst_wrap_data_mode = SF_CTRL_DATA_4_LINES,
    .burst_wrap_data = 0x40,
    /*erase*/
    .chip_erase_cmd = 0xc7,
    .sector_erase_cmd = 0x20,
    .blk32_erase_cmd = 0x52,
    .blk64_erase_cmd = 0xd8,
    /*write*/
    .page_program_cmd = 0x02,
    .qpage_program_cmd = 0x32,
    .qpp_addr_mode = SF_CTRL_ADDR_1_LINE,

    .io_mode = 0x11,
    .clk_delay = 0,
    .clk_invert = 0x03,

    .reset_en_cmd = 0x66,
    .reset_cmd = 0x99,
    .c_rexit = 0xff,
    .wr_enable_write_reg_len = 0x00,

    /*id*/
    .jedec_id_cmd = 0x9f,
    .jedec_id_cmd_dmy_clk = 0,
#if defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    .qpi_jedec_id_cmd = 0x9f,
    .qpi_jedec_id_cmd_dmy_clk = 0x00,
#else
    .enter_32bits_addr_cmd = 0xb7,
    .exit_32bits_addr_cmd = 0xe9,
#endif
    .sector_size = 4,
    .page_size = 256,

    /*read*/
    .fast_read_cmd = 0x0b,
    .fr_dmy_clk = 8 / 8,
    .qpi_fast_read_cmd = 0x0b,
    .qpi_fr_dmy_clk = 8 / 8,
    .fast_read_do_cmd = 0x3b,
    .fr_do_dmy_clk = 8 / 8,
    .fast_read_dio_cmd = 0xbb,
    .fr_dio_dmy_clk = 0,
    .fast_read_qo_cmd = 0x6b,
    .fr_qo_dmy_clk = 8 / 8,

    .qpi_fast_read_qio_cmd = 0xeb,
    .qpi_fr_qio_dmy_clk = 16 / 8,
    .qpi_page_program_cmd = 0x02,
    .write_vreg_enable_cmd = 0x50,

    /* qpi mode */
    .enter_qpi = 0x38,
    .exit_qpi = 0xff,

    /*AC*/
    .time_e_sector = 300,
    .time_e_32k = 1200,
    .time_e_64k = 1200,
    .time_page_pgm = 5,
    .time_ce = 33 * 1000,
    .pd_delay = 20,
    .qe_data = 0,
};
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
static spi_flash_cfg_type g_flash2_cfg = {
    .reset_c_read_cmd = 0xff,
    .reset_c_read_cmd_size = 3,
    .mid = 0xc8,

    .de_burst_wrap_cmd = 0x77,
    .de_burst_wrap_cmd_dmy_clk = 0x3,
    .de_burst_wrap_data_mode = SF_CTRL_DATA_4_LINES,
    .de_burst_wrap_data = 0xF0,

    /*reg*/
    .write_enable_cmd = 0x06,
    .wr_enable_index = 0x00,
    .wr_enable_bit = 0x01,
    .wr_enable_read_reg_len = 0x01,

    .qe_index = 1,
    .qe_bit = 0x01,
    .qe_write_reg_len = 0x01,
    .qe_read_reg_len = 0x1,

    .busy_index = 0,
    .busy_bit = 0x00,
    .busy_read_reg_len = 0x1,
    .release_powerdown = 0xab,

    .read_reg_cmd[0] = 0x05,
    .read_reg_cmd[1] = 0x35,
    .write_reg_cmd[0] = 0x01,
    .write_reg_cmd[1] = 0x31,

    .fast_read_qio_cmd = 0xeb,
    .fr_qio_dmy_clk = 16 / 8,
    .c_read_support = 0,
    .c_read_mode = 0x20,

    .burst_wrap_cmd = 0x77,
    .burst_wrap_cmd_dmy_clk = 0x3,
    .burst_wrap_data_mode = SF_CTRL_DATA_4_LINES,
    .burst_wrap_data = 0x40,
    /*erase*/
    .chip_erase_cmd = 0xc7,
    .sector_erase_cmd = 0x20,
    .blk32_erase_cmd = 0x52,
    .blk64_erase_cmd = 0xd8,
    /*write*/
    .page_program_cmd = 0x02,
    .qpage_program_cmd = 0x32,
    .qpp_addr_mode = SF_CTRL_ADDR_1_LINE,

    .io_mode = 0x10,
    .clk_delay = 0,
    .clk_invert = 0x03,

    .reset_en_cmd = 0x66,
    .reset_cmd = 0x99,
    .c_rexit = 0xff,
    .wr_enable_write_reg_len = 0x00,

    /*id*/
    .jedec_id_cmd = 0x9f,
    .jedec_id_cmd_dmy_clk = 0,
    .enter_32bits_addr_cmd = 0xb7,
    .exit_32bits_addr_cmd = 0xe9,
    .sector_size = 4,
    .page_size = 256,

    /*read*/
    .fast_read_cmd = 0x0b,
    .fr_dmy_clk = 8 / 8,
    .qpi_fast_read_cmd = 0x0b,
    .qpi_fr_dmy_clk = 8 / 8,
    .fast_read_do_cmd = 0x3b,
    .fr_do_dmy_clk = 8 / 8,
    .fast_read_dio_cmd = 0xbb,
    .fr_dio_dmy_clk = 0,
    .fast_read_qo_cmd = 0x6b,
    .fr_qo_dmy_clk = 8 / 8,

    .qpi_fast_read_qio_cmd = 0xeb,
    .qpi_fr_qio_dmy_clk = 16 / 8,
    .qpi_page_program_cmd = 0x02,
    .write_vreg_enable_cmd = 0x50,

    /* qpi mode */
    .enter_qpi = 0x38,
    .exit_qpi = 0xff,

    /*AC*/
    .time_e_sector = 300,
    .time_e_32k = 1200,
    .time_e_64k = 1200,
    .time_page_pgm = 5,
    .time_ce = 33 * 1000,
    .pd_delay = 20,
    .qe_data = 0,
};

static qcc74x_efuse_device_info_type device_info;
#endif

#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
uint32_t qcc74x_flash2_get_jedec_id(void)
{
    uint32_t jid = 0;

    jid = ((g_jedec_id2 & 0xff) << 16) + (g_jedec_id2 & 0xff00) + ((g_jedec_id2 & 0xff0000) >> 16);
    return jid;
}
#endif

/**
 * @brief get flash size from flash jedec id
 *
 * @return QCC74x_Err_Type
 */
static uint32_t ATTR_TCM_SECTION flash_get_size_from_jedecid(uint32_t jedec_id)
{
    uint8_t flash_size_level = 0;
    uint32_t flash_size = 0;
    uint32_t jid = 0;

    jid = ((jedec_id & 0xff) << 16) + (jedec_id & 0xff00) + ((jedec_id & 0xff0000) >> 16);

    if (jid == 0) {
        return 0;
    }

    flash_size_level = (jid & 0x1f);
    flash_size_level -= 0x13;
    flash_size = (1 << flash_size_level) * 512 * 1024;

    return flash_size;
}

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
static int flash_get_clock_delay(spi_flash_cfg_type *cfg)
{
    uint32_t reg_base = 0;
    uint32_t regval = 0;

    reg_base = QCC74x_SF_CTRL_BASE;

    regval = getreg32(reg_base + SF_CTRL_0_OFFSET);
    /* bit0-3 for clk delay */
    if (regval & SF_CTRL_SF_IF_READ_DLY_EN) {
        cfg->clk_delay = ((regval & SF_CTRL_SF_IF_READ_DLY_N_MASK) >> SF_CTRL_SF_IF_READ_DLY_N_SHIFT) + 1;
    } else {
        cfg->clk_delay = 0;
    }
    cfg->clk_invert = 0;
    /* bit0 for clk invert */
    cfg->clk_invert |= (((regval & SF_CTRL_SF_CLK_OUT_INV_SEL) ? 1 : 0) << 0);
    /* bit1 for rx clk invert */
    cfg->clk_invert |= (((regval & SF_CTRL_SF_CLK_SF_RX_INV_SEL) ? 1 : 0) << 1);

    regval = getreg32(reg_base + SF_CTRL_SF_IF_IO_DLY_1_OFFSET);
    /* bit4-6 for do delay */
    cfg->clk_delay |= (((regval & SF_CTRL_SF_IO_0_DO_DLY_SEL_MASK) >> SF_CTRL_SF_IO_0_DO_DLY_SEL_SHIFT) << 4);
    /* bit2-4 for di delay */
    cfg->clk_invert |= (((regval & SF_CTRL_SF_IO_0_DI_DLY_SEL_MASK) >> SF_CTRL_SF_IO_0_DI_DLY_SEL_SHIFT) << 2);
    /* bit5-7 for oe delay */
    cfg->clk_invert |= (((regval & SF_CTRL_SF_IO_0_OE_DLY_SEL_MASK) >> SF_CTRL_SF_IO_0_OE_DLY_SEL_SHIFT) << 5);

    return 0;
}
#endif

static void ATTR_TCM_SECTION flash_set_qspi_enable(spi_flash_cfg_type *p_flash_cfg)
{
    if ((p_flash_cfg->io_mode & 0x0f) == SF_CTRL_QO_MODE || (p_flash_cfg->io_mode & 0x0f) == SF_CTRL_QIO_MODE) {
        qcc74x_sflash_qspi_enable(p_flash_cfg);
    }
}

static void ATTR_TCM_SECTION flash_set_l1c_wrap(spi_flash_cfg_type *p_flash_cfg)
{
    if ((p_flash_cfg->io_mode & 0x1f) == SF_CTRL_QIO_MODE) {
#if defined(QCC74x_undef)
        qcc74x_l1c_set_wrap(1);
#elif defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
        L1C_Set_Wrap(ENABLE);
#endif
        qcc74x_sflash_set_burst_wrap(p_flash_cfg);
    } else {
#if defined(QCC74x_undef)
        qcc74x_l1c_set_wrap(0);
#elif defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
        L1C_Set_Wrap(DISABLE);
#endif
        qcc74x_sflash_disable_burst_wrap(p_flash_cfg);
    }
}

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
static void ATTR_TCM_SECTION qcc74x_flash_set_cmds(spi_flash_cfg_type *p_flash_cfg)
{
    struct sf_ctrl_cmds_cfg cmds_cfg;

    cmds_cfg.ack_latency = 1;
    cmds_cfg.cmds_core_en = 1;
    cmds_cfg.cmds_en = 1;
    cmds_cfg.cmds_wrap_mode = 1;
    cmds_cfg.cmds_wrap_len = 9;

    if ((p_flash_cfg->io_mode & 0x1f) == SF_CTRL_QIO_MODE) {
        cmds_cfg.cmds_wrap_mode = 2;
        cmds_cfg.cmds_wrap_len = 2;
    }
    qcc74x_sf_ctrl_cmds_set(&cmds_cfg, 0);
}
#endif

/**
 * @brief flash_config_init
 *
 * @return int
 */
static int ATTR_TCM_SECTION flash_config_init(spi_flash_cfg_type *p_flash_cfg, uint8_t *jedec_id)
{
    int ret = -1;
    uint8_t is_aes_enable = 0;
    uint32_t jid = 0;
    uint32_t offset = 0;
    uintptr_t flag = 0;

    ret = qcc74x_flash_op_enter(&flag);
    if (ret != 0) {
        return ret;
    }
    qcc74x_xip_sflash_opt_enter(&is_aes_enable);
    qcc74x_xip_sflash_state_save(p_flash_cfg, &offset, 0, 0);
    qcc74x_sflash_get_jedecid(p_flash_cfg, (uint8_t *)&jid);
    arch_memcpy(jedec_id, (uint8_t *)&jid, 3);
    jid &= 0xFFFFFF;
    g_jedec_id = jid;
    ret = qcc74x_sf_cfg_get_flash_cfg_need_lock_ext(jid, p_flash_cfg, 0, 0);
    if (ret == 0) {
        p_flash_cfg->mid = (jid & 0xff);
    }

    /* Winbond W25Q32JW-IQ not support continue read */
    if (jid == 0x1660EF) {
        p_flash_cfg->io_mode = SF_CTRL_QIO_MODE;
        p_flash_cfg->c_read_support = 0x00; 
    }

#ifdef CONFIG_FLASH_2LINE
#if (CONFIG_FLASH_2LINE != 0x11) && (CONFIG_FLASH_2LINE != 0x13)
#error flash 2 line only supports 0x11 or 0x13
#endif
    p_flash_cfg->io_mode = CONFIG_FLASH_2LINE;
    p_flash_cfg->c_read_support = 0x00;
#endif
    /* Set flash controler from p_flash_cfg */
#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    qcc74x_flash_set_cmds(p_flash_cfg);
#endif
    flash_set_qspi_enable(p_flash_cfg);
    flash_set_l1c_wrap(p_flash_cfg);
#if defined(QCC74x_undef)
    qcc74x_xip_sflash_state_restore_ext(p_flash_cfg, offset, 0, 0);
#else
    qcc74x_xip_sflash_state_restore(p_flash_cfg, offset, 0, 0);
#endif
    qcc74x_xip_sflash_opt_exit(is_aes_enable);
    qcc74x_flash_op_exit(flag);

    return ret;
}

#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
/**
 * @brief flash2 init
 *
 * @return int
 */
static int ATTR_TCM_SECTION flash2_init(void)
{
    int stat = -1;
    uint8_t flash2_enable = 0;
    uint32_t ret = 0;
    uint32_t jid = 0;

    struct sf_ctrl_bank2_cfg sf_bank2_cfg;
    struct sf_ctrl_cmds_cfg cmds_cfg;

    sf_bank2_cfg.sbus2_select = 1;
    sf_bank2_cfg.bank2_rx_clk_invert_src = 0;
    sf_bank2_cfg.bank2_rx_clk_invert_sel = 0;
    sf_bank2_cfg.bank2_delay_src = 0;
    sf_bank2_cfg.bank2_clk_delay = 1;
    sf_bank2_cfg.do_delay = 0;
    sf_bank2_cfg.di_delay = 0;
    sf_bank2_cfg.oe_delay = 0;
    sf_bank2_cfg.remap = SF_CTRL_REMAP_4MB;
    sf_bank2_cfg.remap_lock = 1;

    cmds_cfg.ack_latency = 1;
    cmds_cfg.cmds_core_en = 1;
    cmds_cfg.cmds_en = 1;
    cmds_cfg.cmds_wrap_mode = 1;
    cmds_cfg.cmds_wrap_len = SF_CTRL_WRAP_LEN_4096;

    if (device_info.flash_info == 3) {
        /* memoryInfo==3, embedded 4MB+2MB flash */
        flash2_enable = 1;
    }

    if (flash2_enable > 0) {
        qcc74x_sf_cfg_sbus2_flash_init(SF_IO_EMB_SWAP_IO3IO0_AND_SF2, &sf_bank2_cfg);
        qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
        ret = qcc74x_sf_cfg_flash_identify_ext(0, SF_IO_EMB_SWAP_IO3IO0_AND_SF2, 0, &g_flash2_cfg, 0, SF_CTRL_FLASH_BANK1);
        if ((ret & QCC74x_FLASH_ID_VALID_FLAG) == 0) {
            return -1;
        }
        g_flash2_cfg.io_mode = 0x11;
        g_flash2_cfg.c_read_support = 0;
        g_flash2_cfg.c_read_mode = 0xff;
        qcc74x_sflash_get_jedecid(&g_flash2_cfg, (uint8_t *)&jid);
        jid &= 0xFFFFFF;
        g_jedec_id2 = jid;

        qcc74x_sf_ctrl_cmds_set(&cmds_cfg, SF_CTRL_FLASH_BANK1);
        stat = qcc74x_sflash_xip_read_enable(&g_flash2_cfg, (g_flash2_cfg.io_mode & 0xf), 0, SF_CTRL_FLASH_BANK1);
        if (0 != stat) {
            return -1;
        }
        qcc74x_sf_ctrl_sbus2_revoke_replace();

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
        flash2_size = flash_get_size_from_jedecid(g_jedec_id2);
#endif
    }

    return 0;
}
#endif

/**
 * @brief multi flash adapter
 *
 * @return int
 */
int ATTR_TCM_SECTION qcc74x_flash_init(void)
{
    int ret = -1;
    uint32_t jedec_id = 0;
#if defined(QCC74x_undef) || defined(QCC74x_undef)
    uint8_t clk_delay = 1;
    uint8_t clk_invert = 1;
    uintptr_t flag;
#if defined(QCC74x_undef)
    qcc74x_efuse_device_info_type device_info;
#endif
#endif

#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
    qcc74x_efuse_get_device_info(&device_info);
#endif

#if defined(QCC74x_undef) || defined(QCC74x_undef)
#if defined(QCC74x_undef)
    qcc74x_efuse_get_device_info(&device_info);
    if (device_info.flash_info == 5 && device_info.psram_info == 1) {
        *(volatile uint32_t *)0x40000130 |= (1U << 16);
        *(volatile uint32_t *)0x40000134 |= (1U << 16);
    }
#endif
    ret = qcc74x_flash_op_enter(&flag);
    if (ret != 0) {
        return ret;
    }
#if defined(QCC74x_undef)
    qcc74x_sflash_cache_flush();
#else
    L1C_Cache_Flush();
#endif
    qcc74x_sf_cfg_get_flash_cfg_need_lock_ext(jedec_id, &g_flash_cfg, 0, 0);
#if defined(QCC74x_undef)
    qcc74x_sflash_cache_flush();
#else
    L1C_Cache_Flush();
#endif
    qcc74x_flash_op_exit(flag);
    if (g_flash_cfg.mid != 0xff && g_flash_cfg.mid != 0x00) {
        return 0;
    }
    clk_delay = g_flash_cfg.clk_delay;
    clk_invert = g_flash_cfg.clk_invert;

    ret = flash_config_init(&g_flash_cfg, (uint8_t *)&jedec_id);

    g_flash_cfg.clk_delay = clk_delay;
    g_flash_cfg.clk_invert = clk_invert;
#else
#if defined(QCC74x_undef)
    jedec_id = qcc74x_glb_get_flash_id_value();
#else
    jedec_id = GLB_Get_Flash_Id_Value();
#endif
    if (jedec_id != 0) {
        ret = qcc74x_sf_cfg_get_flash_cfg_need_lock_ext(jedec_id, &g_flash_cfg, 0, 0);
        if (ret == 0) {
            g_jedec_id = jedec_id;
            g_flash_cfg.mid = (jedec_id & 0xff);
            flash_get_clock_delay(&g_flash_cfg);
#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
            flash1_size = flash_get_size_from_jedecid(g_jedec_id);
#endif
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
            flash2_init();
#else
#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
            flash2_size = 0;
#endif
#endif
            return 0;
        }
    }

    ret = flash_config_init(&g_flash_cfg, (uint8_t *)&jedec_id);

    flash_get_clock_delay(&g_flash_cfg);
#if defined(QCC74x_undef)
    qcc74x_glb_set_flash_id_value(g_jedec_id);
#else
    GLB_Set_Flash_Id_Value(g_jedec_id);
#endif
#endif

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    flash1_size = flash_get_size_from_jedecid(g_jedec_id);
#endif

#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
    flash2_init();
#else
#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    flash2_size = 0;
#endif
#endif

    return ret;
}

uint32_t qcc74x_flash_get_jedec_id(void)
{
    return g_jedec_id;
}

uint32_t qcc74x_flash_get_size(void)
{
    return flash_get_size_from_jedecid(g_jedec_id);
}

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
uint32_t qcc74x_flash2_get_size(void)
{
    return flash_get_size_from_jedecid(g_jedec_id2);
}
#endif
#endif

void ATTR_TCM_SECTION qcc74x_flash_get_cfg(uint8_t **cfg_addr, uint32_t *len)
{
    *cfg_addr = (uint8_t *)&g_flash_cfg;
    *len = sizeof(spi_flash_cfg_type);
}

void ATTR_TCM_SECTION qcc74x_flash_set_iomode(uint8_t iomode)
{
    uintptr_t flag = 0;
    uint8_t is_aes_enable = 0;
    uint32_t offset = 0;

    if (qcc74x_flash_op_enter(&flag) != 0) {
        return;
    }
    qcc74x_xip_sflash_opt_enter(&is_aes_enable);
    qcc74x_xip_sflash_state_save(&g_flash_cfg, &offset, 0, 0);

    g_flash_cfg.io_mode &= ~0x1f;
    if (iomode & 4) {
        g_flash_cfg.io_mode |= iomode;
    } else {
        g_flash_cfg.io_mode |= 0x10;
        g_flash_cfg.io_mode |= iomode;
    }

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    qcc74x_flash_set_cmds(&g_flash_cfg);
#endif
    flash_set_qspi_enable(&g_flash_cfg);
    flash_set_l1c_wrap(&g_flash_cfg);
#if defined(QCC74x_undef)
    qcc74x_xip_sflash_state_restore_ext(&g_flash_cfg, offset, 0, 0);
#else
    qcc74x_xip_sflash_state_restore(&g_flash_cfg, offset, 0, 0);
#endif
    qcc74x_xip_sflash_opt_exit(is_aes_enable);
    qcc74x_flash_op_exit(flag);
}

ATTR_TCM_SECTION uint32_t qcc74x_flash_get_image_offset(void)
{
    return qcc74x_sf_ctrl_get_flash_image_offset(0, 0);
}

/**
 * @brief erase flash via sbus
 *
 * @param flash absolute startaddr
 * @param flash absolute endaddr
 * @return int
 */
int ATTR_TCM_SECTION qcc74x_flash_erase(uint32_t startaddr, uint32_t len)
{
    int stat = -1;
    uintptr_t flag = 0;

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    if ((startaddr + len) > (flash1_size + flash2_size)) {
        return -ENOMEM;
    }

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }

    if ((startaddr + len) <= flash1_size) {
        stat = qcc74x_xip_sflash_erase_need_lock(&g_flash_cfg, startaddr, len, 0, 0);
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
    } else if (startaddr >= flash1_size) {
        qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
        stat = qcc74x_sflash_erase(&g_flash2_cfg, startaddr, startaddr + len - 1);
        qcc74x_sf_ctrl_sbus2_revoke_replace();
    } else {
        stat = qcc74x_xip_sflash_erase_need_lock(&g_flash_cfg, startaddr, flash1_size - startaddr, 0, 0);
        if (stat == 0) {
            qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
            stat = qcc74x_sflash_erase(&g_flash2_cfg, flash1_size, startaddr + len - flash1_size - 1);
            qcc74x_sf_ctrl_sbus2_revoke_replace();
        }
    }
#else
    }
#endif
#else
    if (startaddr >= QCC74x_FLASH_XIP_END - QCC74x_FLASH_XIP_BASE) {
        return -ENOMEM;
    }

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }
#if defined(QCC74x_undef)
    stat = qcc74x_xip_sflash_erase_need_lock_ext(&g_flash_cfg, startaddr, startaddr + len - 1, 0, 0);
#elif defined(QCC74x_undef)
    uint8_t aes_enabled = 0;
    qcc74x_xip_sflash_opt_enter(&aes_enabled);
    stat = qcc74x_xip_sflash_erase_need_lock(&g_flash_cfg, startaddr, len, 0, 0);
    qcc74x_xip_sflash_opt_exit(aes_enabled);
#else
    stat = qcc74x_xip_sflash_erase_need_lock(&g_flash_cfg, startaddr, len, 0, 0);
#endif
#endif

    qcc74x_flash_op_exit(flag);
    return stat;
}

/**
 * @brief write flash data via sbus
 *
 * @param flash absolute addr
 * @param data
 * @param len
 * @return int
 */
int ATTR_TCM_SECTION qcc74x_flash_write(uint32_t addr, uint8_t *data, uint32_t len)
{
    int stat = -1;
    uintptr_t flag = 0;

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    if ((addr + len) > (flash1_size + flash2_size)) {
        return -ENOMEM;
    }

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }

    if ((addr + len) <= flash1_size) {
        stat = qcc74x_xip_sflash_write_need_lock(&g_flash_cfg, addr, data, len, 0, 0);
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
    } else if (addr >= flash1_size) {
        qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
        stat = qcc74x_sflash_program(&g_flash2_cfg, SF_CTRL_DO_MODE, addr, data, len);
        qcc74x_sf_ctrl_sbus2_revoke_replace();
    } else {
        stat = qcc74x_xip_sflash_write_need_lock(&g_flash_cfg, addr, data, flash1_size - addr, 0, 0);
        if (stat == 0) {
            qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
            stat = qcc74x_sflash_program(&g_flash2_cfg, SF_CTRL_DO_MODE, flash1_size,
                                         data + (flash1_size - addr), addr + len - flash1_size);
            qcc74x_sf_ctrl_sbus2_revoke_replace();
        }
    }
#else
    }
#endif
#else
    if (addr >= QCC74x_FLASH_XIP_END - QCC74x_FLASH_XIP_BASE) {
        return -ENOMEM;
    }

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }
#if defined(QCC74x_undef)
    stat = qcc74x_xip_sflash_write_need_lock_ext(&g_flash_cfg, addr, data, len, 0, 0);
#elif defined(QCC74x_undef)
    uint8_t aes_enabled = 0;
    qcc74x_xip_sflash_opt_enter(&aes_enabled);
    stat = qcc74x_xip_sflash_write_need_lock(&g_flash_cfg, addr, data, len, 0, 0);
    qcc74x_xip_sflash_opt_exit(aes_enabled);
#else
    stat = qcc74x_xip_sflash_write_need_lock(&g_flash_cfg, addr, data, len, 0, 0);
#endif
#endif

    qcc74x_flash_op_exit(flag);
    return stat;
}

/**
 * @brief read flash data via sbus
 *
 * @param flash absolute addr
 * @param data
 * @param len
 * @return int
 */
int ATTR_TCM_SECTION qcc74x_flash_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    int stat = -1;
    uintptr_t flag = 0;

#if defined(QCC743) || defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
    if ((addr + len) > (flash1_size + flash2_size)) {
        return -ENOMEM;
    }

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }

    if ((addr + len) <= flash1_size) {
        stat = qcc74x_xip_sflash_read_need_lock(&g_flash_cfg, addr, data, len, 0, 0);
#ifdef QCC74x_SF_CTRL_SBUS2_ENABLE
    } else if (addr >= flash1_size) {
        qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
        stat = qcc74x_sflash_read(&g_flash2_cfg, SF_CTRL_DO_MODE, 0, addr, data, len);
        qcc74x_sf_ctrl_sbus2_revoke_replace();
    } else {
        stat = qcc74x_xip_sflash_read_need_lock(&g_flash_cfg, addr, data, flash1_size - addr, 0, 0);
        if (stat == 0) {
            qcc74x_sf_ctrl_sbus2_replace(SF_CTRL_PAD2);
            stat = qcc74x_sflash_read(&g_flash2_cfg, SF_CTRL_DO_MODE, 0, flash1_size,
                                      data + (flash1_size - addr), addr + len - flash1_size);
            qcc74x_sf_ctrl_sbus2_revoke_replace();
        }
    }
#else
    }
#endif
#else
    if (addr >= QCC74x_FLASH_XIP_END - QCC74x_FLASH_XIP_BASE) {
        return -ENOMEM;
    }

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }
#if defined(QCC74x_undef)
    stat = qcc74x_xip_sflash_read_need_lock_ext(&g_flash_cfg, addr, data, len, 0, 0);
#elif defined(QCC74x_undef)
    uint8_t aes_enabled = 0;
    qcc74x_xip_sflash_opt_enter(&aes_enabled);
    stat = qcc74x_xip_sflash_read_need_lock(&g_flash_cfg, addr, data, len, 0, 0);
    qcc74x_xip_sflash_opt_exit(aes_enabled);
#else
    stat = qcc74x_xip_sflash_read_need_lock(&g_flash_cfg, addr, data, len, 0, 0);
#endif
#endif

    qcc74x_flash_op_exit(flag);
    return stat;
}

/**
 * @brief read flash unique id
 *
 * @param data
 * @param id_len
 * @return int
 */
int ATTR_TCM_SECTION qcc74x_flash_get_unique_id(uint8_t *data, uint8_t id_len)
{
    int stat = -1;
    uintptr_t flag = 0;

    stat = qcc74x_flash_op_enter(&flag);
    if (stat != 0) {
        return stat;
    }
    stat = qcc74x_xip_sflash_get_uniqueid_need_lock(&g_flash_cfg, data, id_len, 0, 0);
    qcc74x_flash_op_exit(flag);

    return stat;
}

int ATTR_TCM_SECTION qcc74x_flash_set_cache(uint8_t cont_read, uint8_t cache_enable, uint8_t cache_way_disable, uint32_t flash_offset)
{
    uint8_t is_aes_enable = 0;
    uint32_t tmp[1];
    int stat;

    qcc74x_sf_ctrl_set_owner(SF_CTRL_OWNER_SAHB);

    qcc74x_xip_sflash_opt_enter(&is_aes_enable);
    /* To make it simple, exit cont read anyway */
    qcc74x_sflash_reset_continue_read(&g_flash_cfg);

    if (g_flash_cfg.c_read_support == 0) {
        cont_read = 0;
    }

    if (cont_read == 1) {
        stat = qcc74x_sflash_read(&g_flash_cfg, g_flash_cfg.io_mode & 0xf, 1, 0x00000000, (uint8_t *)tmp, sizeof(tmp));

        if (0 != stat) {
            qcc74x_xip_sflash_opt_exit(is_aes_enable);
            return -1;
        }
    }

#if defined(QCC74x_undef) || defined(QCC74x_undef) || defined(QCC74x_undef)
#if defined(QCC74x_undef)
    qcc74x_sflash_cache_enable_set(0xf);
#else
    L1C_Cache_Enable_Set(0xf);
#endif

    if (cache_enable) {
        qcc74x_sf_ctrl_set_flash_image_offset(flash_offset, 0, 0);
        qcc74x_sflash_xip_read_enable(&g_flash_cfg, g_flash_cfg.io_mode & 0xf, cont_read, 0);
    }
#if defined(QCC74x_undef)
    qcc74x_sflash_cache_enable_set(cache_way_disable);
#else
    L1C_Cache_Enable_Set(cache_way_disable);
#endif
#else
    qcc74x_sf_ctrl_set_flash_image_offset(flash_offset, 0, 0);
    qcc74x_sflash_xip_read_enable(&g_flash_cfg, g_flash_cfg.io_mode & 0xf, cont_read, 0);
#endif

    qcc74x_xip_sflash_opt_exit(is_aes_enable);

    return 0;
}

void ATTR_TCM_SECTION qcc74x_flash_aes_init(struct qcc74x_flash_aes_config_s *config)
{
    uint8_t hw_key_enable = 0;

    if (config->key == NULL) {
        hw_key_enable = 1;
    }

    qcc74x_sf_ctrl_aes_set_key_be(config->region, (uint8_t *)config->key, config->keybits);
    qcc74x_sf_ctrl_aes_set_iv_be(config->region, (uint8_t *)config->iv, config->start_addr);
    qcc74x_sf_ctrl_aes_set_region(config->region, config->region_enable, hw_key_enable, config->start_addr, config->end_addr - 1, config->lock_enable);
}

void ATTR_TCM_SECTION qcc74x_flash_aes_enable(void)
{
    qcc74x_sf_ctrl_aes_enable_be();
    qcc74x_sf_ctrl_aes_enable();
}

void ATTR_TCM_SECTION qcc74x_flash_aes_disable(void)
{
    qcc74x_sf_ctrl_aes_disable();
}

void ATTR_TCM_SECTION qcc74x_flash_jump_encrypted_app(uint8_t index, uint32_t flash_addr, uint32_t len)
{
    void (*entry)(void);

    qcc74x_sf_ctrl_aes_set_region_offset(index, flash_addr);

    qcc74x_sf_ctrl_aes_set_region(index, 1 /*enable this region*/, 1 /*hardware key*/,
                                flash_addr,
                                flash_addr + len - 1,
                                0 /*lock*/);

    qcc74x_sf_ctrl_aes_enable_be();
    qcc74x_sf_ctrl_aes_enable();

    qcc74x_flash_set_cache(0, 1, 0, flash_addr);
    qcc74x_l1c_dcache_clean_invalidate_all();
    qcc74x_l1c_icache_invalid_all();

    entry = (void (*)(void))QCC74x_FLASH_XIP_BASE;
    entry();
}
