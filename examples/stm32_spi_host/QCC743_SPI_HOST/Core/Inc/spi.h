/*
 * spi.h
 *
 *  Created on: Nov 6, 2024
 */

#ifndef INC_SPI_H_
#define INC_SPI_H_

#define SPI_MSG_F_TRUNCATED	0x1

struct spi_msg {
	void *data;
	unsigned int data_len;
	void *ctrl;
	unsigned int ctrl_len;
	unsigned int flags;
};

struct spi_stat {
	unsigned long tx_pkts;
	unsigned long tx_bytes;
	unsigned long rx_pkts;
	unsigned long rx_bytes;
	unsigned long rx_drop;
	unsigned long io_err;
	unsigned long hdr_err;
	unsigned long wait_txn_timeouts;
	unsigned long wait_msg_xfer_timeouts;
	unsigned long wait_hdr_ack_timeouts;
	unsigned long mem_err;
	unsigned long rx_stall;
};

#define SPI_MSG_INIT(m, d, dl, c, cl)	do {	\
	struct spi_msg *_m = &(m);					\
	_m->data = d;								\
	_m->data_len = dl;							\
	_m->ctrl = c;								\
	_m->ctrl_len = cl;							\
	_m->flags = 0;								\
} while (0)

struct spi_buffer {
	void *data;
	/* Length of the data. */
	unsigned int len;
	/* Capacity of the buffer, >= len. */
	unsigned int cap;
#define SPI_BUF_F_PUSHED	0x1
	unsigned int flags;
};

int spi_transaction_init(void);

struct spi_buffer *spi_buffer_alloc(unsigned int size, unsigned int reserve);

struct spi_buffer *spi_tx_buffer_alloc(unsigned int size);

void spi_buffer_free(struct spi_buffer *buf);

static inline void *spi_buffer_data(struct spi_buffer *buf)
{
	return buf->data;
}

static inline unsigned int spi_buffer_len(struct spi_buffer *buf)
{
	return buf->len;
}

int spi_read(struct spi_msg *msg, int timeout_ms);

/* Caller is supposed to free the buffer by calling spi_buffer_free. */
int spi_read_buffer(struct spi_buffer **buffer, int timeout_ms);

int spi_write(struct spi_msg *msg, int timeout_ms);

int spi_write_buffer(struct spi_buffer *buffer, int timeout_ms);

void spi_show_throuput_enable(int en);

int spi_on_transaction_ready(void);

int spi_on_txn_data_ready(void);

int spi_on_header_ack(void);

void spi_dump(void);

int spi_get_stats(struct spi_stat *stat);

#endif /* INC_SPI_H_ */
