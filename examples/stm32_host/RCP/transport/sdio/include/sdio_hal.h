#ifndef __SDIO_LL_H__
#define __SDIO_LL_H__

#include <stdbool.h>
#include <stdint.h>
#include "main.h"
#include <rcp_os.h>
#include <rcp_stat.h>

enum sdio_bus_width {
	SDIO_BUS_WIDTH_1B,
	SDIO_BUS_WIDTH_4B,
};

enum sdio_rw {
	SDIO_READ,
	SDIO_WRITE,
};

enum sdio_bs {
	SDIO_BS_512B = 512,
	SDIO_BS_MIN = SDIO_BS_512B,
	SDIO_BS_DEFAULT = SDIO_BS_512B,
	SDIO_BS_1024B = 1024,
	SDIO_BS_2048B = 2048,
	SDIO_BS_MAX = SDIO_BS_2048B,
};

enum sdio_speed_mode {
	SDIO_SPEED_DEFAULT,
	SDIO_SPEED_SDR25,
	SDIO_SPEED_SDR50,
	SDIO_SPEED_DDR50,
};

typedef void (*sdio_int_callback)(uint8_t func, void *data);

/* Configuration for enabled functions. */
struct sdio_func {
	bool enabled;
	enum sdio_bs bs;
	sdio_int_callback cb;
	void *cb_arg;
};

struct sdio_hal {
	/* transfer error flag */
	volatile bool error;
	RCP_TX_PROF_U32_FIELD(dma_cplt_cycles);
	/* Semaphore to indicate DMA transfer completion */
	rcp_sem_t cplt_sem;
	/* Semaphore for DAT1 (SDIOIT) wait during card-present detection */
	rcp_sem_t dat1_sem;
	volatile bool dat1_wait_active;
	struct sdio_func functions[7];
	uint16_t rca;
	SDIO_HandleTypeDef sdio;
};

void sdio_hal_chip_en_pulse(void);

/* Initialize HAL-based SDIO implementation */
bool sdio_hal_init(struct sdio_hal *handle);

/* Reinitialize SDMMC1 IP (RCC reset + MX_SDMMC1_SDIO_Init). */
bool sdio_hal_reinit(struct sdio_hal *handle);

/* Wait for device to assert DAT1 (SDIOIT).  The SDMMC1 IP must already be
 * running from a prior sdio_hal_init / sdio_hal_reinit.  Blocks until the
 * interrupt fires or timeout_ms elapses.  Returns true on success. */
bool sdio_hal_wait_dat1_irq(struct sdio_hal *handle, uint32_t timeout_ms);

/* Custom callback to be registered with HAL_SDIO_RegisterIdentifyCardCallback */
HAL_StatusTypeDef sdio_hal_init_card(SDIO_HandleTypeDef *hsdio);

/* Disable SDIO interrupts internally */
void sdio_hal_disable_irq(struct sdio_hal *handle);
/* Enable SDIO interrupts internally */
void sdio_hal_enable_irq(struct sdio_hal *handle);
void sdio_hal_irq_clear(struct sdio_hal *handle);
/* Recover the SDMMC controller after a DMA write/read failure so the
 * next transfer can proceed. Clears stale STA flags, aborts in-flight
 * DMA, resets HAL bookkeeping. */
void sdio_hal_recover_after_err(struct sdio_hal *handle);

/* RCC-reset SDMMC1 to drop a wedged DPSMACT and re-arm just enough
 * host-side state (POWER, CLKCR, SDIOEN, SDIOIT mask) for the next
 * DAT1 wakeup to latch. No card identification — caller drives that
 * separately once the device is back. */
void sdio_hal_recover_dpsm(struct sdio_hal *handle);

/* Dump SDMMC registers and HAL bookkeeping for debugging. */
void sdio_hal_dump_regs(struct sdio_hal *handle);
bool sdio_hal_rescan(struct sdio_hal *handle);

/* Set SDIO bus width. */
bool sdio_set_bus_width(struct sdio_hal *handle, enum sdio_bus_width width);
/* Set SDIO card speed mode. */
bool sdio_set_speed_mode(struct sdio_hal *handle, enum sdio_speed_mode mode);
/* Set SDIO host clock frequency. */
bool sdio_set_frequency(struct sdio_hal *handle, uint32_t hz);
/* Check pending interrupts from functions. */
uint8_t sdio_check_pending_interrupts(void);
/* Enable IO function */
bool sdio_enable_func(struct sdio_hal *handle, uint8_t func);
/* Set block size for function */
bool sdio_set_block_size(struct sdio_hal *handle, uint8_t func, enum sdio_bs bs);
/* Set block size using CMD52 (more reliable, doesn't need I/O Ready) */
bool sdio_set_block_size_cmd52(struct sdio_hal *handle, uint8_t func, uint16_t block_size);
/* Enable interrupts for function */
bool sdio_enable_int(struct sdio_hal *handle, uint8_t func, sdio_int_callback cb, void *cb_arg);

/* Write / read byte to / from an SDIO function register. */
bool sdio_cmd52(struct sdio_hal *handle, uint8_t func, uint32_t reg, enum sdio_rw rw, uint8_t *data);

/* Read specified amount of data from an SDIO function register.
 *
 * This function shall determine the appropriate transfer mode (byte / block)
 * automatically based on the amount of data.
 * */
bool sdio_cmd53_read(struct sdio_hal *handle, uint8_t func, uint32_t reg, uint8_t *data, uint16_t len);

/* Write specified amount of data to an SDIO function register.
 *
 * This function shall determine the appropriate transfer mode (byte / block)
 * automatically based on the amount of data.
 * */
bool sdio_cmd53_write(struct sdio_hal *handle, uint8_t func, uint32_t reg, uint8_t *data, uint16_t len);

#endif
