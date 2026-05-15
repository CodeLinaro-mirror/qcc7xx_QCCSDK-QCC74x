#ifndef __QCC74X_SDIO_H__
#define __QCC74X_SDIO_H__

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include "sdio_hal.h"

/* Max message size should be aligned with MCU build configuration. */
#define QCC74x_SDIO_MAX_MSG_SIZE 2048
/* Use default 512B value. */
#define QCC74x_SDIO_BLOCK_SIZE SDIO_BS_DEFAULT

/* Write msg includes data size header. */
#define QCC74x_SDIO_MAX_TX_MSG_SIZE (QCC74x_SDIO_BLOCK_SIZE - 2)

/* Card events (polling) */
#define QCC74x_CARD_EVT_DNLD_RDY 	(1 << 0)
#define QCC74x_CARD_EVT_UPLD_RDY	(1 << 1)

/* Card->MCU IRQ source events */
#define QCC74x_HOST_IRQ_RX_RDY		(1 << 0) /* MCU->Host data available */
#define QCC74x_HOST_IRQ_TX_RDY		(1 << 1) /* MCU ready to accept data */

#define QCC74x_SDIO_FUNC     1
#define QCC74x_SDIO_PORT_NUM 16
#define QCC74x_SDIO_PORT_MASK (QCC74x_SDIO_PORT_NUM - 1)

#define QCC74x_REG_SCRATCH_0_0  	0x60 /* Scratch register. Master ready signal. */
#define QCC74x_REG_SCRATCH_0_1  	0x61 /* Scratch 0x61 - MAC addr ? */
#define QCC74x_REG_SCRATCH_0_2      0x62
#define QCC74x_REG_SCRATCH_0_3      0x63
#define QCC74x_REG_IOPORT       	0x78 /* Value of RW port base - 3 bytes. */
#define QCC74x_REG_READ_BITMAP  	0x04 /* Current RD bitmap - 2 bytes.*/
#define QCC74x_REG_WRITE_BITMAP 	0x06 /* Current WR bitmap - 2 bytes. */
#define QCC74x_REG_CARD_EVT		    0x30 /* Card event mask register */
#define QCC74x_REG_HOST_IRQ_RSR		0x01
#define QCC74x_REG_HOST_IRQ_MASK 	0x02 /* MCU->Host interrupt mask register */
#define QCC74x_REG_HOST_IRQ_STATUS	0x03 /* MCU->Host interrupt status register */

/* Amount of data to read. 2 bytes for each port. */
#define QCC74x_REG_DATA_LEN_PORTx(port) (0x08 + (port * 2))

#define QCC74x_MASTER_READY      0x01
#define QCC74x_MASTER_READY_MASK 0xFF
#define QCC74x_SIZE_CONSULT_MULTIPLE 64
#define QCC74x_FLAG_RD_LEN_COMPRESS_SUP (1 << 5)

/* QCC74x MCU->Host IRQ callback.
 *
 * After the receiving callback, the code must call qcc74x_sdio_get_irq_status()
 * to read the interrupt status. Further interrupts may be blocked until status
 * is received.
 */
typedef void (*qcc74x_sdio_irq_cb)(void);

struct qcc74x_sdio {
	struct sdio_hal sdio;
	uint32_t rw_port_base;   /* Base port for reading and writing (ioport). */
	uint16_t curr_read_bitmap;
	uint16_t curr_write_bitmap;
	uint8_t curr_write_out;
	uint8_t curr_read_out;
	uint32_t read_max_size;
	uint32_t write_max_size;
	uint16_t curr_blocksize;
	bool rd_len_compress;
	bool busy;
	qcc74x_sdio_irq_cb irq_cb;
};

/* Initialize SDIO communication with QCC74x. Performs SDIO bus initialization. */
bool qcc74x_sdio_init(struct qcc74x_sdio *hd);
bool qcc74x_sdio_reinit(struct qcc74x_sdio *hd);

/* Register MCU -> Host IRQ handler. */
bool qcc74x_sdio_register_irq_cb(struct qcc74x_sdio *hd, qcc74x_sdio_irq_cb cb);

bool qcc74x_sdio_detect(struct qcc74x_sdio *hd);

int qcc74x_sdio_write(struct qcc74x_sdio *hd, uint8_t *data, uint16_t len, int rety_cnt);
int qcc74x_sdio_read(struct qcc74x_sdio *hd, uint8_t *data, uint16_t len, int rety_cnt);
/* Get card-to-host events. */
uint8_t qcc74x_sdio_get_card_evt(struct qcc74x_sdio *hd);
uint8_t qcc74x_sdio_set_card_evt(struct qcc74x_sdio *hd, uint8_t evts);
/* Get MCU->Host IRQ status */
uint8_t qcc74x_sdio_get_irq_status(struct qcc74x_sdio *hd);
/* Amount of data ready to read */
uint16_t qcc74x_sdio_get_data_len(struct qcc74x_sdio *hd);
/* Check if there is any data left to read */
bool qcc74x_has_data(struct qcc74x_sdio *hd);

#endif
