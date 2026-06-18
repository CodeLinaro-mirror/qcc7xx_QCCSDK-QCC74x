#include <stdio.h>
#include <rcp_bus.h>
#include <rcp_os.h>
#include "buffer_mgmt_ctrl.h"
#include "qcc74x_sdio.h"
#include "sdio_hal.h"
#include "rcp_stat.h"

#include "rcp_bus_sdio_master.h"

#include "log.h"

extern void delay_us(uint32_t us);

#define RCP_BUS_RETRIES 3
#define RCP_BUS_RECOVER_POLL_MS 100U
#define RCP_BUS_RECOVER_INIT_FREQ_HZ 400000U
#define RCP_SDIO_CTRL_TYPE_MASK    0x03
#define RCP_SDIO_CTRL_TYPE_PREPARE 0x01
#define RCP_SDIO_CTRL_TYPE_EXIT    0x02

/* Arbitrary value used by peer for sanity checking. */
#define RCP_BUS_SDIO_DATA_TLV_TYPE 0x55
/* Max raw SDIO message size that can be sent to / read from QCC */
#define RCP_BUS_SDIO_BUF_SIZE QCC74x_SDIO_MAX_MSG_SIZE
/* Maxium payload over bus (raw message size - TLV header size) */
#define RCP_BUS_SDIO_MAX_MSG_SIZE (RCP_BUS_SDIO_BUF_SIZE - 4)
struct {
	bool open;
	volatile bool irq_pending;
	uint8_t irq_status;
	struct qcc74x_sdio sdiohd;
} ctx;

static enum rcp_bus_status sdio_master_open(void);
static enum rcp_bus_status sdio_master_write(rcp_msg_packt_t *packt, uint16_t buf_len);
static int sdio_master_read(rcp_msg_packt_t *packt, uint16_t buf_len);
static enum rcp_bus_status sdio_master_close(void);
static enum rcp_bus_status sdio_master_get_caps(struct rcp_bus_caps *caps);
static uint16_t sdio_master_get_rx_len(void);
static enum rcp_bus_status sdio_master_sync_irq_locked(void);
static bool sdio_master_wait_card_present(void);
static bool sdio_master_recover(void);
static void qcc74x_irq_cb(void);

static struct rcp_bus_api sdio_master_api = {
		.open = sdio_master_open,
		.close = sdio_master_close,
		.write = sdio_master_write,
		.read = sdio_master_read,
		.get_caps = sdio_master_get_caps,
		.get_rx_len = sdio_master_get_rx_len,
		.recover = sdio_master_recover,
};

static void qcc74x_irq_cb(void)
{
	if (!ctx.open) {
		return;
	}

	ctx.irq_pending = true;
	rcp_bus_signal_rx_ready(RCP_BUS_SDIO);
}

bool rcp_bus_sdio_master_init(void)
{
	if (!qcc74x_sdio_init(&ctx.sdiohd)) {
		return false;
	}

	if (RCP_BUS_OK != rcp_bus_register(RCP_BUS_SDIO, &sdio_master_api)) {
		return false;
	}

	qcc74x_sdio_register_irq_cb(&ctx.sdiohd, qcc74x_irq_cb);

	printf("RCP SDIO bus registered.\n");
	return true;
}

bool rcp_bus_sdio_master_reinit(void)
{
	if (!qcc74x_sdio_reinit(&ctx.sdiohd)) {
		return false;
	}

	ctx.irq_pending = false;
	ctx.irq_status = 0;
	__rcp_bus_set_flags(RCP_BUS_SDIO, 0, RCP_BUS_FLAG_TX_PAUSED);
	qcc74x_sdio_register_irq_cb(&ctx.sdiohd, qcc74x_irq_cb);

	return true;
}

void rcp_bus_sdio_master_dump(void)
{
	printf("SDIO Master:\n");
	printf("  open: %s\n", ctx.open ? "true" : "false");
	printf("  irq_pending: %s\n", ctx.irq_pending ? "true" : "false");
	printf("  irq_status: 0x%02X\n", ctx.irq_status);
	sdio_hal_dump_regs(&ctx.sdiohd.sdio);
}

static enum rcp_bus_status sdio_master_open(void)
{
	ctx.open = true;
	ctx.irq_pending = false;
	ctx.irq_status = 0;
	__rcp_bus_set_flags(RCP_BUS_SDIO, 0, RCP_BUS_FLAG_TX_PAUSED);
	return RCP_BUS_OK;
}

static enum rcp_bus_status sdio_master_write(rcp_msg_packt_t *packt, uint16_t buf_len)
{
	uint16_t raw_len;
	uint16_t xfer_len;

	if (packt == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	raw_len = packt->len + sizeof(rcp_msg_packt_t);
	xfer_len = raw_len;

	LOG_HEXDUMP_DBG(packt, raw_len, "SDIO TX");

	if (xfer_len > QCC74x_SDIO_BLOCK_SIZE) {
		/* Align to block size */
		xfer_len = ((xfer_len + QCC74x_SDIO_BLOCK_SIZE - 1U) / QCC74x_SDIO_BLOCK_SIZE);
		xfer_len *= QCC74x_SDIO_BLOCK_SIZE;
		LOG_DBG("Data aligned to %d", xfer_len);
	}
	xfer_len = (uint16_t)((xfer_len + QCC74x_SDIO_DMA_ALIGN - 1U) &
			      ~(QCC74x_SDIO_DMA_ALIGN - 1U));

	if (xfer_len > RCP_BUS_SDIO_BUF_SIZE) {
		LOG_ERR("message (aligned to block size) too large %d > %d",
			xfer_len, RCP_BUS_SDIO_BUF_SIZE);
		return RCP_BUS_ERR_MEMORY;
	}

	int ret = qcc74x_sdio_write(&ctx.sdiohd, (uint8_t *)packt, xfer_len, RCP_BUS_RETRIES);
	if (ret < 0) {
		(ret == -EBUSY) ? (void)0 : LOG_INF("qcc74x_sdio_write fail: %d", ret);
		return (ret == -EBUSY) ? RCP_BUS_ERR_EMPTY : RCP_BUS_ERR_IO;
	}
	return RCP_BUS_OK;
}

static int sdio_master_read(rcp_msg_packt_t *packt, uint16_t buf_len)
{
	rcp_msg_packt_t *tlv;
	uint32_t packet_len = 0;
	int raw_len;

	if ((buf_len & (FRAME_BUFF_DMA_ALIGN - 1U)) != 0U) {
		LOG_ERR("DMA-safe buffer length is not %u-byte aligned: %u",
			(unsigned int)FRAME_BUFF_DMA_ALIGN, (unsigned int)buf_len);
		return RCP_BUS_ERR_INVALID;
	}

	enum rcp_bus_status status = sdio_master_sync_irq_locked();
	if (status != RCP_BUS_OK) {
		return status;
	}

	raw_len = qcc74x_sdio_read(&ctx.sdiohd, (uint8_t *)packt, buf_len, 0);
	if (raw_len <= 0) {
		return (raw_len == -EBUSY) ? RCP_BUS_ERR_EMPTY : RCP_BUS_ERR_IO;
	}

	tlv = packt;
	if (raw_len < (int)sizeof(*tlv)) {
		LOG_ERR("RX packet too small: %d", raw_len);
		return RCP_BUS_ERR_IO;
	}

	packet_len = (uint32_t)tlv->len + sizeof(*tlv);
	if (packet_len > (uint32_t)raw_len || packet_len > buf_len) {
		LOG_ERR("RX packet invalid length: pkt=%lu raw=%d", (unsigned long)packet_len, raw_len);
		return RCP_BUS_ERR_IO;
	}

	LOG_HEXDUMP_DBG(packt, packet_len, "SDIO RX");

	return packet_len;
}

static enum rcp_bus_status sdio_master_close(void)
{
	ctx.open = false;
	ctx.irq_pending = false;
	ctx.irq_status = 0;
	__rcp_bus_set_flags(RCP_BUS_SDIO, 0, RCP_BUS_FLAG_TX_PAUSED);
	//rcp_bus_clear_flags(RCP_BUS_SDIO, RCP_BUS_FLAG_RX_DATA);
	return RCP_BUS_OK;
}

static enum rcp_bus_status sdio_master_get_caps(struct rcp_bus_caps *caps)
{
	caps->mtu = (FRAME_BUFF_RCP_PACKET_LEN < RCP_BUS_SDIO_MAX_MSG_SIZE) ?
		FRAME_BUFF_RCP_PACKET_LEN : RCP_BUS_SDIO_MAX_MSG_SIZE;
	caps->active = qcc74x_sdio_alive(&ctx.sdiohd);

	return RCP_BUS_OK;
}

static uint16_t sdio_master_get_rx_len(void)
{
	uint16_t rx_len = 0;

	return rx_len;
}

static enum rcp_bus_status sdio_master_sync_irq_locked(void)
{
	rcp_os_disable_irq();
	if (!ctx.irq_pending) {
		rcp_os_enable_irq();
		return RCP_BUS_OK;
	}

	ctx.irq_pending = false;
	rcp_os_enable_irq();
	if (!qcc74x_sdio_get_irq_status(&ctx.sdiohd, &ctx.irq_status)) {
		return RCP_BUS_ERR_IO;
	}
    if (ctx.irq_status == QCC74x_HOST_IRQ_RX_RDY) {
        return RCP_BUS_OK;
    }

	uint8_t dev_status = 0;

	if (!qcc74x_sdio_get_device_status(&ctx.sdiohd, &dev_status)) {
		return RCP_BUS_ERR_IO;
	}

	switch (dev_status & RCP_SDIO_CTRL_TYPE_MASK) {
	case RCP_SDIO_CTRL_TYPE_EXIT:
		__rcp_bus_set_flags(RCP_BUS_SDIO, 0, RCP_BUS_FLAG_TX_PAUSED);
		(void)qcc74x_sdio_set_host_status(&ctx.sdiohd, 0);
		break;
	case RCP_SDIO_CTRL_TYPE_PREPARE:
		__rcp_bus_set_flags(RCP_BUS_SDIO, RCP_BUS_FLAG_TX_PAUSED, 0);
		if (!qcc74x_sdio_set_host_status(&ctx.sdiohd, dev_status)) {
			return RCP_BUS_ERR_IO;
		}
		break;
	default:
		break;
	}

	return (ctx.irq_status & QCC74x_HOST_IRQ_RX_RDY) ? RCP_BUS_OK : RCP_BUS_ERR_EMPTY;
}

static bool sdio_master_wait_card_present(void)
{
	(void)rcp_os_sem_take(&ctx.sdiohd.sdio.dat1_sem, 0);
	do {
	    qcc74x_wakeup_gpio_notify(0);
	    if (sdio_hal_wait_dat1_irq(&ctx.sdiohd.sdio, 10)) {
	    	qcc74x_wakeup_gpio_notify(1);
	    	break;
	    }
	    qcc74x_wakeup_gpio_notify(1);
	    rcp_os_delay_ms(1);
	} while(1);

	/* T0: card just appeared on the bus. Recover timing starts here so
	 * the device-sleep window is excluded; everything after this is
	 * counted: reinit + card identification + RCP handshake. */
	RCP_STAT_RECOVER_STAMP(t_card_detected_us);

	return true;
}

static bool sdio_master_recover(void)
{
	ctx.open = false;
	ctx.irq_pending = false;
	ctx.irq_status = 0;
	sdio_hal_disable_irq(&ctx.sdiohd.sdio);

	sdio_hal_recover_dpsm(&ctx.sdiohd.sdio);

	rcp_bus_clear_flags(RCP_BUS_SDIO, RCP_BUS_FLAG_RX_DATA);

	if (!sdio_master_wait_card_present()) {
		return false;
	}

	/* Re-arm ctx.open BEFORE reinit so qcc74x_irq_cb() does not drop
	 * the very first DEVICE_START interrupt that fires
	 * the moment sdio_hw_init() unmasks the IO function IRQ inside
	 * rcp_bus_sdio_master_reinit(). */
	ctx.open = true;

	/* Device enters Card-Identification state after CMD5(arg=0)
	 * probe. Re-init must start immediately; any delay here causes the
	 * device to clear its reported OCR window and the subsequent
	 * CMD5(arg=OCR) will fail. */
	if (!rcp_bus_sdio_master_reinit()) {
		ctx.open = false;
		return false;
	}

	return true;
}
