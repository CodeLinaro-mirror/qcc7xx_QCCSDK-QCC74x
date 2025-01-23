/*
 * spi.c
 *
 *  Created on: Nov 6, 2024
 */

#include <stdio.h>
#include <string.h>
#include "spi.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "main.h"
#include "dwt.h"

#if 0
//#define trace	printf
#define trace(...)
//#define SPI_TXN_PROFILE
#define SPI_WAIT_TXN_TIMEOUT_MS			500
#define SPI_WAIT_MSG_XFER_TIMEOUT_MS	500

#define spi_hang(m) do {					\
	printf("spi xfer hanging, %s\r\n", m);	\
	osDelay(5000);							\
} while (1)

struct spi_header {
	uint16_t len;
	uint16_t len_inv;
	uint8_t version : 2;
	uint8_t reset : 1;
	uint8_t flags : 5;
	uint8_t type;
	uint16_t rsvd;
};

#define spi_header_init(h, _type, _len)	do {	\
	(h).type = _type;		\
	(h).version = 0;		\
	(h).reset = 0;			\
	(h).flags = 0;			\
	(h).len = _len;			\
	(h).len_inv = ~_len;	\
	(h).rsvd = 0;			\
} while (0)

struct spi_buffer {
	void *data;
	/* Length of the data. */
	unsigned int len;
	/* Capacity of the buffer, >= len. */
	unsigned int cap;
};

/* SPI transfer state. */
enum {
	SPI_XFER_STATE_IDLE,
	SPI_XFER_STATE_MSG_HEADER,
	SPI_XFER_STATE_MSG_BODY,
};
#define SPI_STAT_INC(stat, mb, val)	(stat)->mb += (val)

#define SPI_TXQ_LEN	16
#define SPI_RXQ_LEN	16

struct spi_xfer_engine {
	osThreadId_t task;
	int stop;
	SemaphoreHandle_t xfer_cplt_sem;
	/* Counting semaphore for spi tx/rx. */
	SemaphoreHandle_t txrx_sem;
	/* Binary semaphore for spi transaction ready. */
	SemaphoreHandle_t txn_sem;
	/* Transfer state. */
	int state;
	QueueHandle_t txq;
	QueueHandle_t rxq;
	uint8_t tmp_txbuf[8 * 1024];
	/* Current tx buffer. */
	struct spi_buffer *txbuf;
	struct spi_stat stat;
};

enum {
	SPI_XFER_OK = 0,
	SPI_XFER_RESET = -1,
};

extern SPI_HandleTypeDef hspi1;
static struct spi_xfer_engine xfer_engine;

#ifdef SPI_TXN_PROFILE
struct spi_profile {
	uint32_t header_prep;
	uint32_t header_txn_wait;
	uint32_t header_txn;
	uint32_t msg_prep;
	uint32_t msg_txn_wait;
	uint32_t msg_txn;
	uint32_t post_txn;
	uint32_t end;
};

static struct spi_profile profile;

static void spi_profile_report(struct spi_profile *p)
{
	uint32_t header_prep, header_txn_wait, header_txn;
	uint32_t msg_prep, msg_txn_wait, msg_txn;
	uint32_t post_txn;
	uint32_t all;

	header_prep = p->header_txn_wait - p->header_prep;
	header_txn_wait = p->header_txn - p->header_txn_wait;
	header_txn = p->msg_prep - p->header_txn;
	msg_prep = p->msg_txn_wait - p->msg_prep;
	msg_txn_wait = p->msg_txn - p->msg_txn_wait;
	msg_txn = p->post_txn - p->msg_txn;
	post_txn = p->end - p->post_txn;
	all = p->end - p->header_prep;

	printf("header preparation %lu, %luus\r\n", header_prep, dwt_cycle_to_us(header_prep));
	printf("header transaction wait %lu, %luus\r\n", header_txn_wait, dwt_cycle_to_us(header_txn_wait));
	printf("header transaction %lu, %luus\r\n", header_txn, dwt_cycle_to_us(header_txn));
	printf("message preparation %lu, %luus\r\n", msg_prep, dwt_cycle_to_us(msg_prep));
	printf("message transaction wait %lu, %luus\r\n", msg_txn_wait, dwt_cycle_to_us(msg_txn_wait));
	printf("message transaction %lu, %luus\r\n", msg_txn, dwt_cycle_to_us(msg_txn));
	printf("post transaction %lu, %luus\r\n", post_txn, dwt_cycle_to_us(post_txn));
	printf("total %lu, %luus\r\n", all, dwt_cycle_to_us(all));
}

#endif

#define SPI_BUF_ALIGN_MASK	(0x3)

static struct spi_buffer *spi_buffer_alloc(unsigned int size)
{
	unsigned int cap, extra;
	struct spi_buffer *buf;
	int desc_size = sizeof(struct spi_buffer);

	cap = (size + SPI_BUF_ALIGN_MASK) & ~SPI_BUF_ALIGN_MASK;
	buf = pvPortMalloc(cap + desc_size);
	if (!buf)
		return NULL;

	buf->len = size;
	buf->cap = cap;
	buf->data = (char *)buf + desc_size;
	/* Fill in the debug pattern. */
	extra = cap - size;
	while (extra--) {
		uint8_t *p = buf->data;

		p[cap - extra] = 0xa5;
	}
	return buf;
}

static void spi_buffer_free(struct spi_buffer *buf)
{
	if (buf)
		vPortFree(buf);
}

static void raw_delay(unsigned x)
{
	volatile unsigned c = x;

	while (c--);
}

/* Get tx buffer from in non-blocking mode. */
static struct spi_buffer *spi_get_txbuf(struct spi_xfer_engine *engine)
{
	struct spi_buffer *buf;
	BaseType_t ret;

	if (engine->txbuf)
		return engine->txbuf;

	ret = xQueueReceive(engine->txq, &buf, 0);
	if (ret != pdTRUE)
		buf = NULL;

	engine->txbuf = buf;
	if (buf) {
		SPI_STAT_INC(&engine->stat, tx_pkts, 1);
		SPI_STAT_INC(&engine->stat, tx_bytes, buf->len);
	}
	return buf;
}

static int inline spi_wait_for_txn_ready(struct spi_xfer_engine *engine, int timeout_ms)
{
	BaseType_t ret, ticks;

	if (timeout_ms < 0)
		ticks = portMAX_DELAY;
	else
		ticks = pdMS_TO_TICKS(timeout_ms);

	ret = xSemaphoreTake(engine->txn_sem, ticks);
	if (ret == pdTRUE)
		return 1;

	return 0;
}

/* return 1 if the header is valid. */
static int inline spi_header_validate(struct spi_header *hdr)
{
	return (hdr->len ^ hdr->len_inv) == 0xFFFF;
}

static int spi_reset(void)
{
	printf("spi reset\r\n");
	xfer_engine.state = SPI_XFER_STATE_IDLE;
	/* Drain the transaction semaphore. */
	while (xSemaphoreTake(xfer_engine.txn_sem, 0) == pdTRUE);

	HAL_GPIO_WritePin(SPI_SLAVE_RESET_GPIO_Port, SPI_SLAVE_RESET_Pin, GPIO_PIN_RESET);
	raw_delay(2000);
	HAL_GPIO_WritePin(SPI_SLAVE_RESET_GPIO_Port, SPI_SLAVE_RESET_Pin, GPIO_PIN_SET);
	raw_delay(2000);
	HAL_GPIO_WritePin(SPI_SLAVE_RESET_GPIO_Port, SPI_SLAVE_RESET_Pin, GPIO_PIN_RESET);
	SPI_STAT_INC(&xfer_engine.stat, resets, 1);
	return 0;
}

/*
 * Return SPI_XFER_OK on success, a negative number otherwise.
 */
static int spi_do_xfer(struct spi_xfer_engine *engine)
{
	int ret;
	GPIO_PinState rx_pending;
	struct spi_header mh, sh;
	HAL_StatusTypeDef status;
	struct spi_buffer *txbuf, *rxbuf = NULL;

	trace("spi_xfer_one entry\r\n");
	engine->state = SPI_XFER_STATE_MSG_HEADER;
	while (1) {
#ifdef SPI_TXN_PROFILE
		dwt_cycle_counter_reset();
		profile.header_prep = dwt_cycle_counter_read();
#endif
		/* Get tx buffer */
		txbuf = spi_get_txbuf(engine);
		rx_pending = HAL_GPIO_ReadPin(SPI_SLAVE_DATA_RDY_GPIO_Port, SPI_SLAVE_DATA_RDY_Pin);
		trace("txbuf %p, rx pin %d\r\n", txbuf, rx_pending);
		if (!txbuf && !(rx_pending == GPIO_PIN_SET)) {
			trace("nothing to transfer\r\n");
			return SPI_XFER_OK;
		}

		trace("start txn\r\n");
		/* Header transfer. */
		if (txbuf)
			spi_header_init(mh, 0, txbuf->len);
		else
			spi_header_init(mh, 0, 0);

		trace("wait header transaction\r\n");
#ifdef SPI_TXN_PROFILE
		profile.header_txn_wait = dwt_cycle_counter_read();
#endif
		ret = spi_wait_for_txn_ready(engine, SPI_WAIT_TXN_TIMEOUT_MS);
		if (!ret) {
			printf("failed to wait header txn ready\r\n");
			SPI_STAT_INC(&engine->stat, wait_hdr_txn_timeouts, 1);
			return SPI_XFER_RESET;
		}

		trace("start header transaction\r\n");
#ifdef SPI_TXN_PROFILE
		profile.header_txn = dwt_cycle_counter_read();
#endif
		status = HAL_SPI_TransmitReceive(&hspi1, (const uint8_t *)&mh, (uint8_t *)&sh, sizeof(mh), 10);
		if (status != HAL_OK) {
			printf("failed to transmit spi header, %d\r\n", status);
			SPI_STAT_INC(&engine->stat, io_err, 1);
			spi_hang("spi msg header IO error");
		}

		trace("slave spi header, len 0x%x, len_inv %x, version %u, type %x, "
				"reset %u, flags 0x%x, rsvd 0x%x\r\n",
				sh.len, sh.len_inv, sh.version, sh.type, sh.reset, sh.flags, sh.rsvd);
#ifdef SPI_TXN_PROFILE
		profile.msg_prep = dwt_cycle_counter_read();
#endif
		ret = spi_header_validate(&sh);
		if (!ret) {
			printf("Invalid spi header from peer, len (%d, 0x%x), len_inv %x, version %u, type %x, "
					"reset %u, flags 0x%x, rsvd 0x%x\r\n",
					sh.len, sh.len, sh.len_inv, sh.version, sh.type, sh.reset, sh.flags, sh.rsvd);
			SPI_STAT_INC(&engine->stat, hdr_err, 1);
			return SPI_XFER_RESET;
		}

		if (sh.reset) {
			printf("Peer requested to reset\r\n");
			return SPI_XFER_RESET;
		}

		/* Message transfer. */
		trace("master %u, slave %u\r\n", mh.len, sh.len);
		int xfer_size = mh.len > sh.len ? mh.len : sh.len;
		if (!xfer_size) {
			trace("transfer size is 0!\r\n");
			SPI_STAT_INC(&engine->stat, idle_hdr, 1);
			continue;
		}

		/* XXX transfer size fix-up. */
		xfer_size = (xfer_size + SPI_BUF_ALIGN_MASK) & ~SPI_BUF_ALIGN_MASK;
		trace("message transfer size %u\r\n", xfer_size);
		/* We do need to set up a spi transfer. */
		if (sh.len) {
			rxbuf = spi_buffer_alloc(xfer_size);
			if (!rxbuf) {
				spi_hang("no mem for rxbuf");
			}
		}

		trace("wait message transaction\r\n");
		engine->state = SPI_XFER_STATE_MSG_BODY;
#ifdef SPI_TXN_PROFILE
		profile.msg_txn_wait = dwt_cycle_counter_read();
#endif
		ret = spi_wait_for_txn_ready(engine, SPI_WAIT_TXN_TIMEOUT_MS);
		if (!ret) {
			printf("failed to wait spi transaction ready for message\r\n");
			SPI_STAT_INC(&engine->stat, wait_body_txn_timeouts, 1);
			if (rxbuf) {
				spi_buffer_free(rxbuf);
				rxbuf = NULL;
			}
			return SPI_XFER_RESET;
		}

		trace("start message transaction\r\n");
#ifdef SPI_TXN_PROFILE
		profile.msg_txn = dwt_cycle_counter_read();
#endif
		if (mh.len && sh.len) {
			uint8_t *txptr;

			/* XXX assuming that tx buffer is shorter than 8192! */
			if (mh.len < sh.len) {
				memcpy(engine->tmp_txbuf, txbuf->data, txbuf->len);
				txptr = engine->tmp_txbuf;
			} else {
				txptr = txbuf->data;
			}

			status = HAL_SPI_TransmitReceive_DMA(&hspi1, txptr, rxbuf->data, xfer_size);
			if (status != HAL_OK) {
				printf("failed to start hal spi txrx, %d\r\n", status);
				SPI_STAT_INC(&engine->stat, io_err, 1);
				spi_hang("spi msg body txrx IO error");
			}
		} else if (mh.len) {
			status = HAL_SPI_Transmit_DMA(&hspi1, txbuf->data, xfer_size);
			if (status != HAL_OK) {
				printf("failed to start spi tx, %d\r\n", status);
				SPI_STAT_INC(&engine->stat, io_err, 1);
				spi_hang("spi msg body tx IO error");
			}
		} else {
			status = HAL_SPI_Receive_DMA(&hspi1, rxbuf->data, xfer_size);
			if (status != HAL_OK) {
				printf("failed to start spi rx, %d\r\n", status);
				SPI_STAT_INC(&engine->stat, io_err, 1);
				spi_hang("spi msg body rx IO error");
			}
		}

		trace("waiting for spi transfer completion\r\n");
		/* Wait until the spi transaction is over. */
		ret = xSemaphoreTake(engine->xfer_cplt_sem, SPI_WAIT_MSG_XFER_TIMEOUT_MS);
		if (ret != pdTRUE) {
			spi_hang("spi msg body IO timeouted");
		}

		trace("spi transfer completed\r\n");
#ifdef SPI_TXN_PROFILE
		profile.post_txn = dwt_cycle_counter_read();
#endif
		/* txbuf is free after the message transaction. */
		if (txbuf) {
			engine->txbuf = NULL;
			spi_buffer_free(txbuf);
		}

		if (rxbuf) {
			SPI_STAT_INC(&engine->stat, rx_pkts, 1);
			SPI_STAT_INC(&engine->stat, rx_bytes, sh.len);
			/* rx buffer length fix-up */
			rxbuf->len = sh.len;
			ret = xQueueSend(engine->rxq, &rxbuf, 0);
			if (ret != pdTRUE) {
				trace("failed to send to rxq, the msg is dicarded\r\n");
				spi_buffer_free(rxbuf);
				SPI_STAT_INC(&engine->stat, rx_drop, 1);
			}
			rxbuf = NULL;
		}

#ifdef SPI_TXN_PROFILE
		profile.end = dwt_cycle_counter_read();
		spi_profile_report(&profile);
#endif
	}
	return SPI_XFER_OK;
}

static void spi_show_stat(struct spi_stat *stat)
{
	printf("tx %lu pkts, %lu bytes, rx %lu pkts, %lu bytes, drop %lu pkts\r\n",
			stat->tx_pkts, stat->tx_bytes, stat->rx_pkts, stat->rx_bytes, stat->rx_drop);
	printf("IO error %lu, header error %lu, wait_hdr_txn_timeout %lu, "
			"wait_body_txn_timeout %lu, resets %lu, idle_header %lu\r\n",
			stat->io_err, stat->hdr_err, stat->wait_hdr_txn_timeouts,  stat->wait_body_txn_timeouts,
			stat->resets, stat->idle_hdr);
}

static void spi_xfer_engine_task(void *arg)
{
	BaseType_t ret;
	int err, xfer_pending = 0;
	struct spi_xfer_engine *engine = arg;

	while (!engine->stop) {
		if (!xfer_pending) {
			engine->state = SPI_XFER_STATE_IDLE;
			ret = xSemaphoreTake(engine->txrx_sem, 5000);
			if (ret != pdTRUE)
				continue;
		}

		err = spi_do_xfer(engine);
		if (err == SPI_XFER_RESET) {
			spi_reset();
			/* Keep trying the curren transfer after reset. */
			xfer_pending = 1;
		} else {
			/* Start a new tranfer. */
			xfer_pending = 0;
		}
	}
}

int spi_transaction_init(void)
{
	int ret;

	osThreadAttr_t task_attr = {
		.name = "spi_xfer_engine",
		.priority = (osPriority_t) osPriorityRealtime7,
		.stack_size = 8 * 1024
	};

	xfer_engine.rxq = xQueueCreate(SPI_RXQ_LEN, sizeof(void *));
	if (!xfer_engine.rxq) {
		printf("failed to create rxq\r\n");
		return -1;
	}
	xfer_engine.txq = xQueueCreate(SPI_TXQ_LEN, sizeof(void *));
	if (!xfer_engine.txq) {
		printf("failed to create txq\r\n");
		return -1;
	}

	xfer_engine.stop = 0;
	xfer_engine.task = osThreadNew(spi_xfer_engine_task, &xfer_engine, &task_attr);
	if (!xfer_engine.task) {
		printf("failed to create spi xfer engine task\r\n");
		return -1;
	}

	xfer_engine.xfer_cplt_sem = xSemaphoreCreateBinary();
	if (!xfer_engine.xfer_cplt_sem) {
		printf("failed to create spi sem\r\n");
		return -1;
	}
	xfer_engine.txrx_sem = xSemaphoreCreateCounting(0xFFFFFFFF, 0);
	if (!xfer_engine.txrx_sem) {
		printf("failed to create txrx semaphore!\r\n");
		return -1;
	}
	xfer_engine.txn_sem = xSemaphoreCreateBinary();
	if (!xfer_engine.txn_sem) {
		printf("failed to create txn_sem\r\n");
		return -1;
	}

	/* Reset the slave. */
	spi_reset();

	GPIO_PinState pin_state;

	pin_state = HAL_GPIO_ReadPin(SPI_SLAVE_DATA_RDY_GPIO_Port, SPI_SLAVE_DATA_RDY_Pin);
	if (pin_state == GPIO_PIN_SET) {
		ret = xSemaphoreGive(xfer_engine.txrx_sem);
		if (ret != pdTRUE) {
			printf("failed to give tx sem in init\r\n");
			return -3;
		}
	}
	printf("spi transaction initialized\r\n");
	return 0;
}

int spi_read(struct spi_msg *msg, int timeout_ms)
{
	BaseType_t ret, ticks;
	struct spi_buffer *buf;

	if (!msg || !msg->data || !msg->data_len)
		return -1;

	if (timeout_ms < 0)
		ticks = portMAX_DELAY;
	else
		ticks = pdMS_TO_TICKS(timeout_ms);
	ret = xQueueReceive(xfer_engine.rxq, &buf, ticks);
	if (ret != pdTRUE) {
		//printf("failed to read rxq\r\n");
		return -2;
	}
	if (msg->data_len >= buf->len) {
		msg->data_len = buf->len;
		msg->flags &= ~SPI_MSG_F_TRUNCATED;
	} else {
		msg->flags |= SPI_MSG_F_TRUNCATED;
	}

	memcpy(msg->data, buf->data, msg->data_len);
	spi_buffer_free(buf);
	return msg->data_len;
}

int spi_write(struct spi_msg *msg, int timeout_ms)
{
	BaseType_t ret;
	struct spi_buffer *buf;
	BaseType_t ticks;

	if (!msg || !msg->data || !msg->data_len)
		return -1;

	buf = spi_buffer_alloc(msg->data_len);
	if (!buf) {
		//printf("no mem for txbuf\r\n");
		return -2;
	}

	/* Copy the data from caller. */
	memcpy(buf->data, msg->data, msg->data_len);

	if (timeout_ms < 0)
		ticks = portMAX_DELAY;
	else
		ticks = pdMS_TO_TICKS(timeout_ms);

	ret = xQueueSend(xfer_engine.txq, &buf, ticks);
	if (ret != pdTRUE) {
		printf("failed to send to txq\r\n");
		return -3;
	}
	/* Indicate that we have something to send. */
	ret = xSemaphoreGive(xfer_engine.txrx_sem);
	if (ret != pdTRUE) {
		printf("failed to give tx sem\r\n");
		return -4;
	}
	return msg->data_len;
}

int spi_get_stats(struct spi_stat *stat)
{
	if (!stat)
		return -1;

	*stat = xfer_engine.stat;
	return 0;
}

int spi_on_transaction_ready(void)
{
	BaseType_t ret, task_woken;

	trace("txn ready!\r\n");
    if (xPortIsInsideInterrupt()) {
    	ret = xSemaphoreGiveFromISR(xfer_engine.txn_sem, &task_woken);
    	portYIELD_FROM_ISR(task_woken);
    } else {
    	ret = xSemaphoreGive(xfer_engine.txn_sem);
    }

    /* Since this is a binary semaphore, no need to report this information. */
	if (ret != pdTRUE) {
		trace("failed to give sem when txn ready\r\n");
		return -1;
	}
	return 0;
}

int spi_on_data_ready(void)
{
	BaseType_t ret, task_woken;

	trace("data ready!\r\n");
    if (xPortIsInsideInterrupt()) {
    	ret = xSemaphoreGiveFromISR(xfer_engine.txrx_sem, &task_woken);
    	portYIELD_FROM_ISR(task_woken);
    } else {
    	ret = xSemaphoreGive(xfer_engine.txrx_sem);
    }
	if (ret != pdTRUE) {
		printf("failed to give sem when txrx ready\r\n");
		return -1;
	}
	return 0;
}

static void spi_on_transaction_complete(void)
{
	BaseType_t ret, task_woken;

	trace("hw txn done\r\n");
    if (xPortIsInsideInterrupt()) {
    	ret = xSemaphoreGiveFromISR(xfer_engine.xfer_cplt_sem, &task_woken);
    	portYIELD_FROM_ISR(task_woken);
    } else {
    	ret = xSemaphoreGive(xfer_engine.xfer_cplt_sem);
    }
	if (ret != pdTRUE) {
		printf("failed to give sem when spi xfer is complete\r\n");
	}
}

static const char *spi_state_str[] = {
	[SPI_XFER_STATE_IDLE] = "Idle",
	[SPI_XFER_STATE_MSG_HEADER] = "Header Transaction",
	[SPI_XFER_STATE_MSG_BODY] = "Message Body Transaction",
};

void spi_dump(void)
{
	BaseType_t ret;
	GPIO_PinState pin_state;

	printf("Master transfer state %s\r\n", spi_state_str[xfer_engine.state]);

	ret = uxQueueMessagesWaiting(xfer_engine.txn_sem);
	printf("SPI transaction ready semaphore counter %lu\r\n", ret);

	pin_state = HAL_GPIO_ReadPin(SPI_SLAVE_DATA_RDY_GPIO_Port, SPI_SLAVE_DATA_RDY_Pin);
	printf("Slave data ready pin %s\r\n", pin_state == GPIO_PIN_SET ? "High" : "Low");

	pin_state = HAL_GPIO_ReadPin(SPI_TXN_RDY_GPIO_Port, SPI_TXN_RDY_Pin);
	printf("Slave transaction ready pin %s\r\n", pin_state == GPIO_PIN_SET ? "High" : "Low");

	spi_show_stat(&xfer_engine.stat);

	printf("Number of queue items, TX %lu, RX %lu\r\n",
			uxQueueMessagesWaiting(xfer_engine.txq), uxQueueMessagesWaiting(xfer_engine.rxq));
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	spi_on_transaction_complete();
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	spi_on_transaction_complete();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	spi_on_transaction_complete();
}
#endif
