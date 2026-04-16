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
/* Register MCU -> Host IRQ handler. */
bool qcc74x_sdio_register_irq_cb(struct qcc74x_sdio *hd, qcc74x_sdio_irq_cb cb);

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
