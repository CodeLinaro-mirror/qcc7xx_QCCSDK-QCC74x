#include <string.h>
#include <stdio.h>
#include <rcp_bus.h>
#include <rcp_os.h>
#include "buffer_mgmt_ctrl.h"

#include "qcc74x_sdio.h"
#include "rcp_bus_sdio_master.h"

#include "log.h"

#define RCP_BUS_RETRIES 3

/* Arbitrary value used by peer for sanity checking. */
#define RCP_BUS_SDIO_DATA_TLV_TYPE 0x55
/* Max raw SDIO message size that can be sent to / read from QCC */
#define RCP_BUS_SDIO_BUF_SIZE QCC74x_SDIO_MAX_MSG_SIZE
/* Maxium payload over bus (raw message size - TLV header size) */
#define RCP_BUS_SDIO_MAX_MSG_SIZE (RCP_BUS_SDIO_BUF_SIZE - 4)
/* Host RX buffer must reserve protocol header room before FRAME_BUFF_PAYLOAD. */
#define RCP_BUS_RX_PACKET_MAX_SIZE (FRAME_BUFF_TOTALSIZE - FRAME_BUFF_MSGSTRUCT_OFFSET)

struct {
	bool open;
	volatile bool irq_pending;
	uint8_t irq_status;
	rcp_mutex_t rxtx_mutex;
	struct qcc74x_sdio sdiohd;
} ctx;

static uint8_t tx_buf[RCP_BUS_SDIO_BUF_SIZE] __attribute__((aligned(32)));
static uint8_t rx_buf[RCP_BUS_SDIO_BUF_SIZE] __attribute__((aligned(32)));

static enum rcp_bus_status sdio_master_open(void);
static enum rcp_bus_status sdio_master_write(rcp_msg_packt_t *packt);
static int sdio_master_read(rcp_msg_packt_t *packt);
static enum rcp_bus_status sdio_master_close(void);
static enum rcp_bus_status sdio_master_get_caps(struct rcp_bus_caps *caps);
static uint16_t sdio_master_get_rx_len(void);
static enum rcp_bus_status sdio_master_sync_irq_locked(void);
static void sdio_master_update_rx_flag_locked(uint16_t remaining_len);
static void qcc74x_irq_cb(void);

static struct rcp_bus_api sdio_master_api = {
		.open = sdio_master_open,
		.close = sdio_master_close,
		.write = sdio_master_write,
		.read = sdio_master_read,
		.get_caps = sdio_master_get_caps,
		.get_rx_len = sdio_master_get_rx_len,
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

	if (!rcp_os_mutex_init(&ctx.rxtx_mutex)) {
		return false;
	}
	qcc74x_sdio_register_irq_cb(&ctx.sdiohd, qcc74x_irq_cb);

	printf("RCP SDIO bus registered.\n");
	return true;
}

static enum rcp_bus_status sdio_master_open(void)
{
	ctx.open = true;
	ctx.irq_pending = false;
	ctx.irq_status = 0;
	return RCP_BUS_OK;
}

static enum rcp_bus_status sdio_master_write(rcp_msg_packt_t *packt)
{
	if (packt == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	uint16_t msg_len = packt->len + sizeof(rcp_msg_packt_t);

	memset(tx_buf, 0, sizeof(tx_buf));
	memcpy(tx_buf, packt, msg_len);

	LOG_HEXDUMP_DBG(tx_buf, msg_len, "SDIO TX");

	if (msg_len > QCC74x_SDIO_BLOCK_SIZE) {
		/* Align to block size */
		msg_len = ((msg_len + QCC74x_SDIO_BLOCK_SIZE - 1) / QCC74x_SDIO_BLOCK_SIZE);
		msg_len *= QCC74x_SDIO_BLOCK_SIZE;
		LOG_DBG("Data aligned to %d", msg_len);
	}

	if (msg_len > RCP_BUS_SDIO_BUF_SIZE) {
		LOG_ERR("message (aligned to block size) too large %d > %d",
			msg_len, RCP_BUS_SDIO_BUF_SIZE);
		return RCP_BUS_ERR_MEMORY;
	}

	if (!rcp_os_mutex_lock(&ctx.rxtx_mutex, 1000)) {
		return RCP_BUS_ERR_IO;
	}

    int ret = qcc74x_sdio_write(&ctx.sdiohd, tx_buf, msg_len, RCP_BUS_RETRIES);
	if (ret < 0) {
		LOG_ERR("qcc74x_sdio_write fail: %d", ret);
		(void)rcp_os_mutex_unlock(&ctx.rxtx_mutex);
        return (ret == -EBUSY) ? RCP_BUS_ERR_EMPTY : RCP_BUS_ERR_IO;
	}
	(void)rcp_os_mutex_unlock(&ctx.rxtx_mutex);
	return RCP_BUS_OK;
}

static int sdio_master_read(rcp_msg_packt_t *packt)
{
	rcp_msg_packt_t *tlv;
	uint32_t packet_len = 0;
	int raw_len;

	if (!rcp_os_mutex_lock(&ctx.rxtx_mutex, 1000)) {
		return RCP_BUS_ERR_IO;
	}

	sdio_master_sync_irq_locked();

	raw_len = qcc74x_sdio_read(&ctx.sdiohd, rx_buf, RCP_BUS_SDIO_BUF_SIZE, 0);
	if (raw_len <= 0) {
		sdio_master_update_rx_flag_locked(0);
		(void)rcp_os_mutex_unlock(&ctx.rxtx_mutex);
        return (raw_len == -EBUSY) ? RCP_BUS_ERR_EMPTY : RCP_BUS_ERR_IO;
	}

	//sdio_master_update_rx_flag_locked(qcc74x_sdio_get_data_len(&ctx.sdiohd));
	(void)rcp_os_mutex_unlock(&ctx.rxtx_mutex);

	tlv = (rcp_msg_packt_t *)rx_buf;
	if (raw_len < (int)sizeof(*tlv)) {
		LOG_ERR("RX packet too small: %d", raw_len);
		return RCP_BUS_ERR_IO;
	}

	packet_len = (uint32_t)tlv->len + sizeof(*tlv);
	if (packet_len > (uint32_t)raw_len || packet_len > RCP_BUS_RX_PACKET_MAX_SIZE) {
		LOG_ERR("RX packet invalid length: pkt=%lu raw=%d", (unsigned long)packet_len, raw_len);
		return RCP_BUS_ERR_IO;
	}

	LOG_HEXDUMP_DBG(rx_buf, packet_len, "SDIO RX");

	memcpy(packt, rx_buf, packet_len);

	return packet_len;
}

static enum rcp_bus_status sdio_master_close(void)
{
	ctx.open = false;
	ctx.irq_pending = false;
	ctx.irq_status = 0;
	//rcp_bus_clear_flags(RCP_BUS_SDIO, RCP_BUS_FLAG_RX_DATA);
	return RCP_BUS_OK;
}

static enum rcp_bus_status sdio_master_get_caps(struct rcp_bus_caps *caps)
{
	caps->mtu = RCP_BUS_SDIO_MAX_MSG_SIZE;
	return RCP_BUS_OK;
}

static uint16_t sdio_master_get_rx_len(void)
{
	uint16_t rx_len = 0;

	return rx_len;
}

static enum rcp_bus_status sdio_master_sync_irq_locked(void)
{
	if (!ctx.irq_pending) {
		return RCP_BUS_OK;
	}

	ctx.irq_status = qcc74x_sdio_get_irq_status(&ctx.sdiohd);
	ctx.irq_pending = false;
	return RCP_BUS_OK;
}

static void sdio_master_update_rx_flag_locked(uint16_t remaining_len)
{
	rcp_os_disable_irq();
	if (remaining_len == 0U && !ctx.irq_pending) {
		//rcp_bus_clear_flags(RCP_BUS_SDIO, RCP_BUS_FLAG_RX_DATA);
	}
	rcp_os_enable_irq();
	ctx.irq_status = 0;
}
