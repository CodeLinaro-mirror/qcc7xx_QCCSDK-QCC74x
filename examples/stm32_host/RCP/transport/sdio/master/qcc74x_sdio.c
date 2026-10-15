/* qcc74x_sdio.c
 *
 * QCC74x SDIO module provides read / write communication with QCC74x MCU over
 * SDIO bus. The module uses the underlying generic SDIO transport to
 * communicate with the Qualcomm-specific functions and register on the MCU
 * side.
 */
#include <stdio.h>
#include <string.h>
#include <rcp_config.h>

#include "qcc74x_sdio.h"
#include "log.h"
#include "sdio_hal.h"

extern void delay_us(uint32_t us);

static bool qcc74x_write_reg(struct qcc74x_sdio *hd, uint32_t reg, uint8_t val);
static bool qcc74x_read_reg(struct qcc74x_sdio *hd, uint32_t reg, uint8_t *val);
static bool qcc74x_read_reg_16(struct qcc74x_sdio *hd, uint32_t reg, uint16_t *val);
static bool qcc74x_read_reg_24(struct qcc74x_sdio *hd, uint32_t reg, uint32_t *val);
static void sdio_func_callback(uint8_t func, void *data);
static int qcc74x_get_read_port(struct qcc74x_sdio *hd);
static bool sdio_hw_init(struct qcc74x_sdio *hd);
bool qcc74x_sdio_get_irq_status(struct qcc74x_sdio *hd, uint8_t *status);

bool qcc74x_sdio_init(struct qcc74x_sdio *hd)
{
	if (!sdio_hal_init(&hd->sdio)) {
		return false;
	}

	//HAL_Delay(50);

	return sdio_hw_init(hd);
}

bool qcc74x_sdio_reinit(struct qcc74x_sdio *hd)
{
	if (!sdio_hal_reinit(&hd->sdio)) {
		return false;
	}

	//HAL_Delay(50);

	/* Drop the cached port bitmaps. Device side has reset its port
	 * counters too, so any stale bits from before the reset would make
	 * subsequent qcc74x_sdio_write/read pick the wrong port. The next
	 * write/read will refill the bitmap from the device's bitmap
	 * register. */
	hd->curr_write_bitmap = 0;
	hd->curr_read_bitmap  = 0;

	return sdio_hw_init(hd);
}

static bool sdio_hw_init(struct qcc74x_sdio *hd)
{
	enum sdio_bus_width bus_width = RCP_SDIO_BUS_WIDE ? SDIO_BUS_WIDTH_4B : SDIO_BUS_WIDTH_1B;
	uint8_t val;

	if (RCP_SDIO_FREQUENCY_HZ > 25000000U) {
		if (!sdio_set_speed_mode(&hd->sdio, SDIO_SPEED_SDR25)) {
			LOG_ERR("Failed to switch SDIO card to high-speed (SDR25)");
			return false;
		}
	}
    if (!sdio_set_frequency(&hd->sdio, RCP_SDIO_FREQUENCY_HZ)) {
        LOG_ERR("Failed to reconfigure SDIO host clock to %u Hz", RCP_SDIO_FREQUENCY_HZ);
        return false;
    }

	if (!sdio_enable_func(&hd->sdio, QCC74x_SDIO_FUNC)) {
		LOG_ERR("Failed to enable SDIO func: %d", QCC74x_SDIO_FUNC);
		return false;
	}
	if (!sdio_set_block_size(&hd->sdio, QCC74x_SDIO_FUNC, QCC74x_SDIO_BLOCK_SIZE)) {
		LOG_ERR("Failed to set SDIO default block size");
		return false;
	}
	hd->curr_blocksize = QCC74x_SDIO_BLOCK_SIZE;

	/* Configure interrupts clear on read. */
//    val = QCC74x_HOST_IRQ_RX_RDY;
//	if (!qcc74x_write_reg(hd, QCC74x_REG_HOST_IRQ_RSR, val)) {
//		LOG_ERR("Failed to configure QCC74x interrupts");
//		return false;
//	}

	val = QCC74x_MASTER_READY & QCC74x_MASTER_READY_MASK;
	if (!qcc74x_write_reg(hd, QCC74x_REG_SCRATCH_0_0, val)) {
		LOG_ERR("Failed to send master ready signal!");
		return false;
	}

	/* Unmask interrupts */
	val = QCC74x_HOST_IRQ_RX_RDY | QCC74x_HOST_IRQ_EVENT_1;
	if (!qcc74x_write_reg(hd, QCC74x_REG_HOST_IRQ_MASK, val)) {
		LOG_ERR("Failed to unmask QCC74x interrupts");
		return false;
	}

	if (!qcc74x_read_reg_24(hd, QCC74x_REG_IOPORT, &hd->rw_port_base)) {
		LOG_ERR("Failed to read IO port register");
		return false;
	}

	/* read max size */
	if (!qcc74x_read_reg(hd, QCC74x_REG_SCRATCH_0_1, &val)) {
		LOG_ERR("Failed to read IO port register");
		return false;
	}
	if (val) {
		hd->write_max_size = val * QCC74x_SIZE_CONSULT_MULTIPLE;
	} else {
		hd->write_max_size = QCC74x_SDIO_MAX_MSG_SIZE;
	}
	hd->read_max_size = QCC74x_SDIO_MAX_MSG_SIZE;

	if (!qcc74x_read_reg(hd, QCC74x_REG_SCRATCH_0_3, &val)) {
		LOG_ERR("Failed to read IO port register");
		return false;
	}
	hd->curr_write_out = (val >> 4) & QCC74x_SDIO_PORT_MASK;
	hd->curr_read_out  = (val) & QCC74x_SDIO_PORT_MASK;

	if (!qcc74x_read_reg(hd, QCC74x_REG_SCRATCH_0_2, &val)) {
		LOG_ERR("Failed to read IO port register");
		return false;
	}
	hd->rd_len_compress = (val & QCC74x_FLAG_RD_LEN_COMPRESS_SUP) ? true:false;

	if (!sdio_set_bus_width(&hd->sdio, bus_width)) {
		LOG_ERR("Failed to set bus width for SDIO\n");
		return false;
	}

	if (!sdio_enable_int(&hd->sdio, QCC74x_SDIO_FUNC, sdio_func_callback, hd)) {
		LOG_ERR("Failed to enable function interrupts");
		return false;
	}

	return true;
}

bool qcc74x_sdio_detect(struct qcc74x_sdio *hd)
{
	return sdio_hal_rescan(&hd->sdio);
}

bool qcc74x_sdio_alive(struct qcc74x_sdio *hd)
{
    uint8_t cccr_revision;

    if (hd == NULL || hd->sdio.rca == 0U) {
        return false;
    }

    return sdio_cmd52(&hd->sdio,
                      SDIO_CCCR_FUNCTION_0,
                      SDIO_CCCR_REVISION_REG,
                      SDIO_READ,
                      &cccr_revision);
}

bool qcc74x_sdio_register_irq_cb(struct qcc74x_sdio *hd, qcc74x_sdio_irq_cb cb)
{
	if (!cb) {
		return false;
	}
	hd->irq_cb = cb;
	return true;
}

int qcc74x_sdio_write(struct qcc74x_sdio *hd, uint8_t *data, uint16_t len, int retry_cnt)
{
	int retry = 0;
	uint8_t bitmap_byte;
	uint8_t wr_port = hd->curr_write_out & QCC74x_SDIO_PORT_MASK;
	uint16_t wr_mask = (uint16_t)(1U << wr_port);
	uint32_t bitmap_reg = QCC74x_REG_WRITE_BITMAP + ((wr_port >= 8U) ? 1U : 0U);
	uint8_t bitmap_shift = (wr_port >= 8U) ? 8U : 0U;

	while ((hd->curr_write_bitmap & wr_mask) == 0U) {
		if (!qcc74x_read_reg(hd, bitmap_reg, &bitmap_byte)) {
			return -EIO;
		}
		hd->curr_write_bitmap |= (uint16_t)bitmap_byte << bitmap_shift;
		if ((hd->curr_write_bitmap & wr_mask) != 0U) {
			break;
		}
		if (retry < retry_cnt) {
			retry += 1;
			delay_us(10);
		} else {
			return -EBUSY;
		}
	}

	if (len > hd->write_max_size) {
		return -ENOMEM;
	}
	LOG_DBG("qcc74_sdio_write_port:%d bitmap:%x rd_port:%d\r\n", len, hd->curr_write_bitmap, wr_port);

	bool ret = sdio_cmd53_write(&hd->sdio, QCC74x_SDIO_FUNC, hd->rw_port_base + wr_port, data, len);
	if (!ret) {
		return -EIO;
	}

	hd->curr_write_bitmap &= (uint16_t)~wr_mask;
	hd->curr_write_out += 1;
	return len;
}

int qcc74x_sdio_read(struct qcc74x_sdio *hd, uint8_t *data, uint16_t len, int retry_cnt)
{
	int retry = 0;
	uint8_t bitmap_byte;
	uint16_t read_len = 0;
	uint8_t reg[2] = {0};
	uint8_t rd_port = hd->curr_read_out & QCC74x_SDIO_PORT_MASK;
	uint16_t rd_mask = (uint16_t)(1U << rd_port);
	uint32_t bitmap_reg = QCC74x_REG_READ_BITMAP + ((rd_port >= 8U) ? 1U : 0U);
	uint8_t bitmap_shift = (rd_port >= 8U) ? 8U : 0U;

	while ((hd->curr_read_bitmap & rd_mask) == 0U) {
		if (!qcc74x_read_reg(hd, bitmap_reg, &bitmap_byte)) {
			return -EIO;
		}
		hd->curr_read_bitmap |= (uint16_t)bitmap_byte << bitmap_shift;
		if ((hd->curr_read_bitmap & rd_mask) != 0U) {
			break;
		}
		if (retry < retry_cnt) {
			retry += 1;
			delay_us(10);
		} else {
			return -EBUSY;
		}
	}

	if (hd->rd_len_compress) {
		if (!qcc74x_read_reg(hd, QCC74x_REG_DATA_LEN_PORTx(rd_port), &reg[0])) {
			return -EIO;
		}
		if (reg[0] & 0x1) {
			if (!qcc74x_read_reg(hd, QCC74x_REG_DATA_LEN_PORTx(rd_port) + 1, &reg[1])) {
				return -EIO;
			}
		}
		read_len = (uint16_t)reg[1] << 8 | reg[0];
		if (read_len & 0x2) {
			/* block mode */
			read_len = (read_len >> 2) * hd->curr_blocksize;
		} else {
			/* byte mode */
			read_len = (read_len >> 2);
		}
	} else {
		if (!qcc74x_read_reg_16(hd, QCC74x_REG_DATA_LEN_PORTx(rd_port), &read_len)) {
			return -EIO;
		}
	}

	if (read_len > hd->curr_blocksize) {
		read_len = (read_len + hd->curr_blocksize - 1) & ~(hd->curr_blocksize - 1);
	}

	if (read_len == 0U) {
		LOG_DBG("SDIO RX invalid zero len: rd_port=%u rd_mask=0x%04x curr_bitmap=0x%04x "
			"bitmap_byte=0x%02x len_reg=0x%02x%02x compress=%u curr_read_out=%u",
			(unsigned int)rd_port, (unsigned int)rd_mask,
			(unsigned int)hd->curr_read_bitmap, (unsigned int)bitmap_byte,
			(unsigned int)reg[1], (unsigned int)reg[0],
			hd->rd_len_compress ? 1U : 0U,
			(unsigned int)hd->curr_read_out);
		return -EIO;
	}

	if (read_len > len) {
		return -ENOMEM;
	}
	if (read_len > hd->read_max_size) {
		return -ENOMEM;
	}
	if (!sdio_cmd53_read(&hd->sdio, QCC74x_SDIO_FUNC, hd->rw_port_base + rd_port, data, read_len)) {
		LOG_ERR("sdio_read err\r\n");
		return -EIO;
	}

	LOG_DBG("qcc74x_sdio_read:%d bitmap:%x rd_port:%d\r\n", read_len, hd->curr_read_bitmap, rd_port);

	hd->curr_read_bitmap &= (uint16_t)~rd_mask;
	hd->curr_read_out += 1;

	return read_len;
}

uint8_t qcc74x_sdio_get_card_evt(struct qcc74x_sdio *hd)
{
	uint8_t evts;
	if (!qcc74x_read_reg(hd, QCC74x_REG_CARD_EVT, &evts)) {
			return 0;
	}
	return evts;
}

uint8_t qcc74x_sdio_set_card_evt(struct qcc74x_sdio *hd, uint8_t evts)
{
	uint8_t reg_val = evts;
	if (!qcc74x_write_reg(hd, QCC74x_REG_CARD_EVT, reg_val)) {
			return 0;
	}
	return evts;
}

uint16_t qcc74x_sdio_get_data_len(struct qcc74x_sdio *hd)
{
	uint16_t data_len;
	/* Check next read port. */
	int rd_port = qcc74x_get_read_port(hd);
	/* No data */
	if (rd_port < 0) {
		return 0;
	}
	uint8_t reg = QCC74x_REG_DATA_LEN_PORTx(rd_port);
	if (!qcc74x_read_reg_16(hd, reg, &data_len)) {
		return 0;
	}
	uint8_t data_mode = (uint8_t)(data_len & 0x03);
	if (data_mode == 0x00 || data_mode == 0x01) {
		/* Data less than block size - register stores amount of bytes */
		return (data_len >> 2);
	} else if (data_mode == 0x02 || data_mode == 0x03) {
		/* Data more than block size - register stores amount of blocks */
		return (data_len >> 2) * QCC74x_SDIO_BLOCK_SIZE;
	}
	return 0;
}

bool qcc74x_sdio_get_irq_status(struct qcc74x_sdio *hd, uint8_t *status)
{
    bool ret = false;
	uint8_t irq_status = 0;

	if (status == NULL) {
		return false;
	}

	if (!qcc74x_read_reg(hd, QCC74x_REG_HOST_IRQ_STATUS, &irq_status)) {
        goto _err;
	}
	//printf("qcc74x host irq status=0x%02x\r\n", irq_status);
	if (!qcc74x_write_reg(hd, QCC74x_REG_HOST_IRQ_STATUS,
			      (uint8_t)(~irq_status & QCC74x_HOST_IRQ_VALID_MASK))) {
        goto _err;
	}

    ret = true;
_err:
	sdio_hal_irq_clear(&hd->sdio);
	sdio_hal_enable_irq(&hd->sdio);
	*status = irq_status;
	return ret;
}

bool qcc74x_sdio_get_device_status(struct qcc74x_sdio *hd, uint8_t *status)
{
	if (status == NULL) {
		return false;
	}
	return qcc74x_read_reg(hd, QCC74x_REG_SCRATCH_1_0, status);
}

bool qcc74x_sdio_set_host_status(struct qcc74x_sdio *hd, uint8_t status)
{
	return qcc74x_write_reg(hd, QCC74x_REG_SCRATCH_1_1, status);
}

static bool qcc74x_write_reg(struct qcc74x_sdio *hd, uint32_t reg, uint8_t val)
{
	return sdio_cmd52(&hd->sdio, QCC74x_SDIO_FUNC, reg, SDIO_WRITE, &val);
}

static bool qcc74x_read_reg(struct qcc74x_sdio *hd, uint32_t reg, uint8_t *val)
{
	return sdio_cmd52(&hd->sdio, QCC74x_SDIO_FUNC, reg, SDIO_READ, val);
}

static bool qcc74x_read_reg_16(struct qcc74x_sdio *hd, uint32_t reg, uint16_t *val)
{
	*val = 0;
	for (int i = 0; i < 2; i++) {
		uint8_t tmp = 0;
		if (!qcc74x_read_reg(hd, reg++, &tmp)) {
			return false;
		}
		*val |= (tmp << (i * 8));
	}
	return true;
}

static bool qcc74x_read_reg_24(struct qcc74x_sdio *hd, uint32_t reg, uint32_t *val)
{
	*val = 0;
	for (int i = 0; i < 3; i++) {
		uint8_t tmp = 0;
		if (!qcc74x_read_reg(hd, reg++, &tmp)) {
			return false;
		}
		*val |= (tmp << (i * 8));
	}
	return true;
}

static void sdio_func_callback(uint8_t func, void *data)
{
	struct qcc74x_sdio *hd = data;

	if (hd->irq_cb) {
		hd->irq_cb();
	}
	/* Disable interrupts until they get processed using qcc74x_sdio_get_irq_status().
	 * Otherwise the interrupt will fire continously.
	 */
	sdio_hal_disable_irq(&hd->sdio);
}

static int qcc74x_get_read_port(struct qcc74x_sdio *hd)
{
	uint16_t rd_bitmap = 0;
	if (!qcc74x_read_reg_16(hd, QCC74x_REG_READ_BITMAP, &rd_bitmap)) {
		return -1;
	}

	/* No data to read */
	if (rd_bitmap == 0) {
		return -1;
	} else {
		return __builtin_ctz(rd_bitmap);
	}
}

void qcc74x_wakeup_gpio_notify(bool notify)
{
#if CONFIG_SDIO_OOB
	if (notify) {
		HAL_GPIO_WritePin(QCC74X_OOB_WAKEUP_GPIO_Port, QCC74X_OOB_WAKEUP_Pin, GPIO_PIN_RESET);
	} else {
        // high level wakeup device
		HAL_GPIO_WritePin(QCC74X_OOB_WAKEUP_GPIO_Port, QCC74X_OOB_WAKEUP_Pin, GPIO_PIN_SET);
	}
#else
	GPIO_InitTypeDef gpio = {0};

	if (notify) {
		gpio.Pin       = GPIO_PIN_10;
		gpio.Mode      = GPIO_MODE_AF_PP;
		gpio.Pull      = GPIO_PULLUP;
		gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
		gpio.Alternate = GPIO_AF12_SDMMC1;
		HAL_GPIO_Init(GPIOC, &gpio);
	} else {
		gpio.Pin   = GPIO_PIN_10;
		gpio.Mode  = GPIO_MODE_OUTPUT_PP;
		gpio.Pull  = GPIO_NOPULL;
		gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		HAL_GPIO_Init(GPIOC, &gpio);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);
	}
#endif
}

void read_bitmap(struct qcc74x_sdio *hd)
{
	uint8_t bitmap_byte[2];
	uint8_t irq;
	qcc74x_read_reg(hd, QCC74x_REG_READ_BITMAP, &bitmap_byte[0]);
	qcc74x_read_reg(hd, QCC74x_REG_READ_BITMAP+1, &bitmap_byte[1]);
	qcc74x_read_reg(hd, QCC74x_REG_HOST_IRQ_MASK, &irq);

	printf("read_bitmap:%x %x irq:%d\r\n", bitmap_byte[0], bitmap_byte[1], irq);
}
