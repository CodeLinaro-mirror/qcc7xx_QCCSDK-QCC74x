#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include <rcp_config.h>
#include <rcp_os.h>
#include <rcp_stat.h>
#include "stm32h7rsxx_hal.h"
#include "sdio_hal.h"
#include "log.h"

#define SDIO_RW_TIMEOUT_MS 1000
#define SDIO_DMA_WORD_MASK (sizeof(uint32_t) - 1U)
#define SDIO_DMA_CACHE_LINE_SIZE 32U
#define SDIO_DMA_CACHE_LINE_MASK (SDIO_DMA_CACHE_LINE_SIZE - 1U)
#if 1
#define SDIO_DMA_ASSERT(cond, reason, data, len)                                                \
	do {                                                                                     \
		if (__builtin_expect(!(cond), 0)) {                                              \
			const void *const sdio_dma_addr__ = (const void *)(data);                \
			const unsigned int sdio_dma_len__ = (unsigned int)(len);                 \
			LOG_ERR("SDIO DMA assert: %s addr=%p len=%u", reason,                    \
				sdio_dma_addr__, sdio_dma_len__);                                \
			Error_Handler();                                                          \
			__builtin_unreachable();                                                 \
		}                                                                                \
	} while (0)
#else
#define SDIO_DMA_ASSERT(cond, reason, data, len)                                                \
        (void)0;
#endif

#define SDIO_OCR_VDD_32_33 (1UL << 20U)
#define SDIO_OCR_SDIO_S18R (1UL << 24U)
#define SDIO_RESET_CCCR    0x06
#define SDIO_RESET_BIT     (1 << 3)
static SDIO_HandleTypeDef *hsdio = NULL;
extern uint8_t __data_start;
extern uint8_t __NONCACHEABLEBUFFER_BEGIN;
extern uint8_t __NONCACHEABLEBUFFER_END;

static inline bool sdio_dma_buf_is_noncacheable(const void *data, uint16_t len)
{
	uintptr_t start = (uintptr_t)data;
	uintptr_t end = start + (uintptr_t)len;
	uintptr_t nc_start = (uintptr_t)&__NONCACHEABLEBUFFER_BEGIN;
	uintptr_t nc_end = (uintptr_t)&__NONCACHEABLEBUFFER_END;

	return (start >= nc_start) && (end <= nc_end) && (end >= start);
}

static inline bool sdio_dma_buf_is_cacheable(const void *data, uint16_t len)
{
	uintptr_t start = (uintptr_t)data;
	uintptr_t end = start + (uintptr_t)len;
	uintptr_t cache_start = (uintptr_t)&__data_start;
	uintptr_t cache_end = (uintptr_t)&__NONCACHEABLEBUFFER_BEGIN;

	return (start >= cache_start) && (end <= cache_end) && (end >= start);
}

static inline bool sdio_dma_buf_is_accessible(const void *data, uint16_t len)
{
	return sdio_dma_buf_is_cacheable(data, len) ||
	       sdio_dma_buf_is_noncacheable(data, len);
}

static inline bool sdio_dma_addr_is_word_aligned(const void *data)
{
	return (((uintptr_t)data & SDIO_DMA_WORD_MASK) == 0U);
}

static inline bool sdio_dma_addr_is_cache_line_aligned(const void *data)
{
	return (((uintptr_t)data & SDIO_DMA_CACHE_LINE_MASK) == 0U);
}

static inline bool sdio_dma_buf_alignment_is_valid(const void *data, uint16_t len)
{
	if (sdio_dma_buf_is_cacheable(data, len)) {
		return sdio_dma_addr_is_cache_line_aligned(data);
	}

	return sdio_dma_addr_is_word_aligned(data);
}

static inline void sdio_dma_clean_for_write(uint8_t *data, uint16_t len)
{
	if (sdio_dma_buf_is_cacheable(data, len)) {
		SCB_CleanDCache_by_Addr((uint32_t *)data, len);
	}
	__DSB();
}

static inline void sdio_dma_invalidate_for_read(uint8_t *data, uint16_t len)
{
	if (sdio_dma_buf_is_cacheable(data, len)) {
		SCB_InvalidateDCache_by_Addr((uint32_t *)data, len);
	}
	__DSB();
}

/**
  * @brief This function handles SDMMC1 global interrupt.
  */
void SDMMC1_IRQHandler(void)
{
  if (hsdio != NULL) {
    HAL_SDIO_IRQHandler(hsdio);
  }
}

static inline struct sdio_hal *sdio_hal_from_handle(SDIO_HandleTypeDef *hsdio)
{
	return (struct sdio_hal *)((uint8_t *)hsdio - offsetof(struct sdio_hal, sdio));
}

static void sdio_txrx_cplt_callback(SDIO_HandleTypeDef *sdio)
{
	struct sdio_hal *handle = sdio_hal_from_handle(sdio);

	RCP_TX_PROF_MARK(handle->dma_cplt_cycles);
	(void)rcp_os_sem_give(&handle->cplt_sem);
}

static void sdio_error_callback(SDIO_HandleTypeDef *sdio)
{
	struct sdio_hal *handle = sdio_hal_from_handle(sdio);

	__HAL_SDIO_CLEAR_FLAG(sdio, SDMMC_FLAG_DCRCFAIL | SDMMC_FLAG_DTIMEOUT |
	                             SDMMC_FLAG_TXUNDERR | SDMMC_FLAG_RXOVERR);
	__HAL_SDIO_DISABLE_IT(sdio, SDMMC_IT_DCRCFAIL | SDMMC_IT_DTIMEOUT |
	                             SDMMC_IT_TXUNDERR | SDMMC_IT_RXOVERR  |
	                             SDMMC_IT_DATAEND);

	handle->error = true;
	RCP_TX_PROF_MARK(handle->dma_cplt_cycles);
	(void)rcp_os_sem_give(&handle->cplt_sem);
}

static bool sdio_register_dma_callbacks(struct sdio_hal *handle)
{
	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_TX_CPLT_CB_ID,
						 sdio_txrx_cplt_callback)) {
		return false;
	}
	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_RX_CPLT_CB_ID,
						 sdio_txrx_cplt_callback)) {
		return false;
	}
	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_ERROR_CB_ID,
						 sdio_error_callback)) {
		return false;
	}

	return true;
}

static void hal_sdio_func_callback(SDIO_HandleTypeDef *hsdio, uint32_t func)
{
	struct sdio_hal *handle = sdio_hal_from_handle(hsdio);

	if (handle->dat1_wait_active) {
		handle->dat1_wait_active = false;
		__HAL_SDIO_CLEAR_FLAG(hsdio, SDMMC_FLAG_SDIOIT);
		__HAL_SDIO_DISABLE_IT(hsdio, SDMMC_IT_SDIOIT);
		(void)rcp_os_sem_give(&handle->dat1_sem);
		return;
	}

	if (handle->functions[func].cb) {
		handle->functions[func].cb(func, handle->functions[func].cb_arg);
	}
	/* HAL does not clear SDIO interrupt by itself */
	__HAL_SDIO_CLEAR_FLAG(hsdio, SDMMC_FLAG_SDIOIT);
}

static int get_sdmmc_bus_width(enum sdio_bus_width bw)
{
	switch(bw) {
	case SDIO_BUS_WIDTH_1B:
		return SDMMC_BUS_WIDE_1B;
	case SDIO_BUS_WIDTH_4B:
		return SDMMC_BUS_WIDE_4B;
	default:
		return -1;
	}
}



/**
  * @brief SDMMC1 Initialization Function
  * @retval true on success, false on HAL_SDIO_Init failure (caller must retry)
  */
static bool MX_SDMMC1_SDIO_Init(SDIO_HandleTypeDef *hsdio1)
{

  /* Disable clock protection */
  HAL_RCCEx_DisableClockProtection(RCC_CLOCKPROTECT_XSPI);

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */
  /* Register custom card initialization callback */
  HAL_SDIO_RegisterIdentifyCardCallback(hsdio1, sdio_hal_init_card);
  /* USER CODE END SDMMC1_Init 1 */
  hsdio1->Instance = SDMMC1;
  hsdio1->Init.ClockEdge = SDMMC_CLOCK_EDGE_FALLING;
  hsdio1->Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsdio1->Init.BusWide = SDMMC_BUS_WIDE_4B;
  hsdio1->Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsdio1->Init.ClockDiv = 50;
  if (HAL_SDIO_Init(hsdio1) != HAL_OK)
  {
    return false;
  }
  hsdio = hsdio1;
  RCP_STAT_RECOVER_STAMP_ONCE(t_hal_init_done_us);
  /* USER CODE BEGIN SDMMC1_Init 2 */
  /* Re-enable clock protection */
  HAL_RCCEx_EnableClockProtection(RCC_CLOCKPROTECT_XSPI);

  /* USER CODE END SDMMC1_Init 2 */

  return true;
}

void sdio_hal_irq_clear(struct sdio_hal *handle)
{
	__HAL_SDIO_CLEAR_FLAG(&handle->sdio, SDMMC_FLAG_SDIOIT);
}

/* Dump raw SDMMC controller registers + relevant HAL bookkeeping. Useful
 * for diagnosing "DMA started but no IRQ comes back" situations where the
 * data-path state machine is stuck. */
void sdio_hal_dump_regs(struct sdio_hal *handle)
{
	SDMMC_TypeDef *inst;
	uint32_t sta;

	if (handle == NULL || handle->sdio.Instance == NULL) {
		printf("SDMMC: <null>\n");
		return;
	}

	inst = handle->sdio.Instance;
	sta  = inst->STA;

	printf("SDMMC regs:\n");
	printf("  STA      = 0x%08lx  CPSMACT=%lu DPSMACT=%lu DCRCFAIL=%lu DTIMEOUT=%lu\n",
	       (unsigned long)sta,
	       (unsigned long)((sta >> 13) & 1U),
	       (unsigned long)((sta >> 12) & 1U),
	       (unsigned long)((sta >>  1) & 1U),
	       (unsigned long)((sta >>  3) & 1U));
	printf("           TXUNDERR=%lu RXOVERR=%lu DATAEND=%lu DABORT=%lu DBCKEND=%lu\n",
	       (unsigned long)((sta >>  4) & 1U),
	       (unsigned long)((sta >>  5) & 1U),
	       (unsigned long)((sta >>  8) & 1U),
	       (unsigned long)((sta >> 11) & 1U),
	       (unsigned long)((sta >> 10) & 1U));
	printf("  POWER    = 0x%08lx\n", (unsigned long)inst->POWER);
	printf("  CLKCR    = 0x%08lx\n", (unsigned long)inst->CLKCR);
	printf("  ARG      = 0x%08lx\n", (unsigned long)inst->ARG);
	printf("  CMD      = 0x%08lx\n", (unsigned long)inst->CMD);
	printf("  DCTRL    = 0x%08lx  DTEN=%lu FIFORST=%lu SDIOEN=%lu\n",
	       (unsigned long)inst->DCTRL,
	       (unsigned long)(inst->DCTRL & 0x1U),
	       (unsigned long)((inst->DCTRL >> 13) & 1U),
	       (unsigned long)((inst->DCTRL >> 11) & 1U));
	printf("  DLEN     = %lu\n",     (unsigned long)inst->DLEN);
	printf("  DCOUNT   = %lu\n",     (unsigned long)inst->DCOUNT);
	printf("  IDMACTRL = 0x%08lx\n", (unsigned long)inst->IDMACTRL);
	printf("  IDMABASER= 0x%08lx\n", (unsigned long)inst->IDMABASER);
	printf("  MASK     = 0x%08lx\n", (unsigned long)inst->MASK);
	printf("  RESPCMD  = 0x%08lx\n", (unsigned long)inst->RESPCMD);
	printf("HAL state: State=%u ErrorCode=0x%08lx Context=0x%lx error=%u\n",
	       (unsigned)handle->sdio.State,
	       (unsigned long)handle->sdio.ErrorCode,
	       (unsigned long)handle->sdio.Context,
	       (unsigned)handle->error);
}

/* Bring the SDMMC controller back to a usable state after a DMA
 * write/read failure (DCRCFAIL / DTIMEOUT / TXUNDERR / RXOVERR / DABORT
 * or sem timeout while DMA was still running). Without this the next
 * HAL_SDIO_*Extended_DMA call may immediately fail because of stale
 * STA bits, an in-flight transfer, or a leftover CMDTRANS / IDMA
 * configuration.
 *
 * Steps:
 *   1. Disable data-path interrupts so a late completion IRQ cannot
 *      race with the recovery.
 *   2. Disable the IDMA engine and clear DLEN / DCTRL so any pending
 *      transfer is dropped.
 *   3. Clear all data-path STA flags.
 *   4. Reset HAL state so the next API call configures everything
 *      from scratch.
 *   5. Re-enable the SDIO IO function interrupt that we always need. */
void sdio_hal_recover_after_err(struct sdio_hal *handle)
{
	SDIO_HandleTypeDef *hsdio;
	SDMMC_TypeDef *inst;

	if (handle == NULL) {
		return;
	}

	hsdio = &handle->sdio;
	inst  = hsdio->Instance;

	/* 1. mask the data IRQs HAL enables in Read/Write DMA so a late
	 * completion / error IRQ cannot race with the recovery. */
	__HAL_SDIO_DISABLE_IT(hsdio, SDMMC_IT_DCRCFAIL | SDMMC_IT_DTIMEOUT |
	                              SDMMC_IT_TXUNDERR | SDMMC_IT_RXOVERR  |
	                              SDMMC_IT_DATAEND);


	/* 2. Abort any pending DMA / data path. We only soft-clear here.
	 * If the controller is wedged hard (CPSMACT stuck), the upper
	 * layer will detect the bus is dead and run sdio_hal_reinit(),
	 * which RCC-resets the whole SDMMC1 IP. Touching POWER here
	 * would drive SDIO bus power off and disturb the device side. */
	inst->CMD      = 0U;
	inst->IDMACTRL = SDMMC_DISABLE_IDMA;
	inst->DLEN     = 0;
	/* Preserve SDIOEN: it is the SDIO-mode master switch. Clearing it
	 * masks IT_SDIOIT entirely, so a follow-up DAT1-only wait (e.g.
	 * sdio_master_recover -> wait_card_present) would never see the
	 * device wakeup IRQ. cmd53 paths re-arm DCTRL on the next transfer,
	 * so leaving SDIOEN set has no side effect there either. */
	inst->DCTRL    &= SDMMC_DCTRL_SDIOEN;
	__SDMMC_CMDTRANS_DISABLE(inst);

	/* 3. clear stale STA bits so the next transfer starts clean */
	__HAL_SDIO_CLEAR_FLAG(hsdio, SDMMC_STATIC_DATA_FLAGS);

	/* 4. reset HAL bookkeeping */
	hsdio->State     = HAL_SDIO_STATE_READY;
	hsdio->ErrorCode = HAL_SDIO_ERROR_NONE;
	hsdio->Context   = SDIO_CONTEXT_NONE;
	handle->error    = false;

	/* Drain any stale completion semaphore so the next take blocks
	 * for the *next* transfer's IRQ, not for the failed one. */
	(void)rcp_os_sem_take(&handle->cplt_sem, 0);

	/* 5. re-arm the SDIO IO function IRQ (data IRQ enable is per-transfer) */
	sdio_hal_enable_irq(handle);
}

/* Force-clear DPSMACT and re-arm only what's needed for SDIOIT to
 * latch a DAT1 wakeup. RCC reset is the only software-visible way to
 * drop DPSMACT (every data-path register is gated on DPSMACT=0 per
 * STM32H7 RM). After the reset everything in SDMMC1 returns to
 * default, so we restore the bare minimum host-side configuration:
 *   - CLKCR: bus clock (kept as init divider, full speed re-applied
 *            later by sdio_hw_init when the device is ready)
 *   - POWER: ON
 *   - DCTRL: SDIOEN=1 (otherwise SDIOIT never fires)
 *   - MASK : SDIOITIE
 * Card identification (CMD0/5/3/7) is intentionally skipped here. */
void sdio_hal_recover_dpsm(struct sdio_hal *handle)
{
	SDMMC_TypeDef *inst;
	uint32_t saved_clkcr;

	if (handle == NULL) {
		return;
	}

	inst = handle->sdio.Instance;
	saved_clkcr = inst->CLKCR;

	__HAL_RCC_SDMMC1_FORCE_RESET();
	__HAL_RCC_SDMMC1_RELEASE_RESET();

	/* Restore clock divider so the controller can talk on the bus
	 * (bus clock must run for SDIOIT detection to work). */
	inst->CLKCR = saved_clkcr;

	(void)SDMMC_PowerState_ON(inst);
	__SDMMC_OPERATION_ENABLE(inst);
	//__HAL_SDIO_ENABLE_IT(&handle->sdio, SDMMC_IT_SDIOIT);

	/* Reset HAL bookkeeping that the dropped IP no longer reflects. */
	handle->sdio.State     = HAL_SDIO_STATE_READY;
	handle->sdio.ErrorCode = HAL_SDIO_ERROR_NONE;
	handle->sdio.Context   = SDIO_CONTEXT_NONE;
	handle->error          = false;
	(void)rcp_os_sem_take(&handle->cplt_sem, 0);
}

void sdio_hal_disable_irq(struct sdio_hal *handle)
{
	__HAL_SDIO_DISABLE_IT(&handle->sdio, SDMMC_IT_SDIOIT);
	__HAL_SDIO_DISABLE_IT(&handle->sdio, SDMMC_IT_DTIMEOUT);
}

void sdio_hal_enable_irq(struct sdio_hal *handle)
{
	__HAL_SDIO_ENABLE_IT(&handle->sdio, SDMMC_IT_SDIOIT);
}

bool sdio_set_frequency(struct sdio_hal *handle, uint32_t hz)
{
	if (handle == NULL) {
		return false;
	}

	return (HAL_OK == HAL_SDIO_ConfigFrequency(&handle->sdio, hz));
}

bool sdio_cmd52(struct sdio_hal *handle, uint8_t func, uint32_t reg, enum sdio_rw rw, uint8_t *data)
{
	uint8_t status;
	HAL_SDIO_DirectCmd_TypeDef arg = {
			.Reg_Addr = reg,
			.IOFunctionNbr = func,
			.ReadAfterWrite = HAL_SDIO_WRITE_ONLY,
	};
	if (rw == SDIO_WRITE) {
		status = HAL_SDIO_WriteDirect(&handle->sdio, &arg, *data);
	} else {
		status = HAL_SDIO_ReadDirect(&handle->sdio, &arg, data);
	}

	if (status != HAL_OK) {
		LOG_DBG("Direct RW failed");
		return false;
	}
	return true;
}

bool sdio_enable_func(struct sdio_hal *handle, uint8_t func)
{
	if (HAL_OK != HAL_SDIO_EnableIOFunction(&handle->sdio, func)) {
		return false;
	}

	/* Initialize with default values */
	handle->functions[func].enabled = true;
	handle->functions[func].bs = SDIO_BS_DEFAULT;
	return true;
}

bool sdio_set_bus_width(struct sdio_hal *handle, enum sdio_bus_width width)
{
	if (HAL_OK != HAL_SDIO_SetDataBusWidth(&handle->sdio, width)) {
		return false;
	}
	return true;
}

bool sdio_set_speed_mode(struct sdio_hal *handle, enum sdio_speed_mode mode)
{
	uint32_t hal_rate;

	if (handle == NULL) {
		return false;
	}

	switch (mode) {
	case SDIO_SPEED_SDR25:
		hal_rate = HAL_SDIOS_DATA_RATE_SDR25;
		break;
	case SDIO_SPEED_SDR50:
		hal_rate = HAL_SDIOS_DATA_RATE_SDR50;
		break;
	case SDIO_SPEED_DDR50:
		hal_rate = HAL_SDIOS_DATA_RATE_DDR50;
		break;
	case SDIO_SPEED_DEFAULT:
	default:
		hal_rate = HAL_SDIOS_DATA_RATE_SDR12;
		break;
	}

	return (HAL_OK == HAL_SDIO_SetSpeedMode(&handle->sdio, hal_rate));
}

bool sdio_set_block_size(struct sdio_hal *handle, uint8_t func, enum sdio_bs bs)
{
	/* First try to set the block size with CMD52. This is more reliable. */
	if (sdio_set_block_size_cmd52(handle, func, (uint16_t)bs)) {
		handle->functions[func].bs = bs;
		return true;
	}

	if (HAL_OK != HAL_SDIO_SetBlockSize(&handle->sdio, func, (uint16_t)bs)) {
		LOG_ERR("Both CMD52 and CMD53 set block size failed");
		return false;
	}
	handle->functions[func].bs = bs;
	return true;
}

bool sdio_set_block_size_cmd52(struct sdio_hal *handle, uint8_t func, uint16_t block_size)
{
	uint32_t reg_addr;
	uint8_t block_size_low;
	uint8_t block_size_high;

	/* Compute the block size register address:
	 * Function 1: 0x100 + 0x10 = 0x110
	 * Function 2: 0x200 + 0x10 = 0x210
	 */
	reg_addr = (func * 0x100UL) + 0x10UL;

	/* Split the 16-bit block size into two bytes. */
	block_size_low = (uint8_t)(block_size & 0xFF);
	block_size_high = (uint8_t)((block_size >> 8) & 0xFF);

	/* Write the low byte with CMD52 (address: reg_addr). */
	HAL_SDIO_DirectCmd_TypeDef arg = {
		.Reg_Addr = reg_addr,
		.IOFunctionNbr = 0,
		.ReadAfterWrite = HAL_SDIO_READ_AFTER_WRITE,
	};

	if (HAL_OK != HAL_SDIO_WriteDirect(&handle->sdio, &arg, block_size_low)) {
		LOG_ERR("Failed to write block size low byte to 0x%03X", reg_addr);
		return false;
	}

	/* Write the high byte with CMD52 (address: reg_addr + 1). */
	arg.Reg_Addr = reg_addr + 1;
	if (HAL_OK != HAL_SDIO_WriteDirect(&handle->sdio, &arg, block_size_high)) {
		LOG_ERR("Failed to write block size high byte to 0x%03X", reg_addr + 1);
		return false;
	}
	handle->sdio.block_size = block_size;

	return true;
}

bool sdio_enable_int(struct sdio_hal *handle, uint8_t func, sdio_int_callback cb, void *cb_arg)
{
	if (HAL_OK != HAL_SDIO_RegisterIOFunctionCallback(&handle->sdio, func, hal_sdio_func_callback)) {
		return false;
	}
	if (HAL_OK != HAL_SDIO_EnableIOFunctionInterrupt(&handle->sdio, func)) {
		return false;
	}
	handle->functions[func].cb = cb;
	handle->functions[func].cb_arg = cb_arg;
	return true;
}

bool sdio_hal_rescan(struct sdio_hal *handle)
{
	uint32_t Resp4;
	//SDMMC_CmdGoIdleState(hsdio->Instance);
	return (SDMMC_CmdSendOperationcondition(hsdio->Instance, 0U, &Resp4) == HAL_SDIO_ERROR_NONE);
}

/* The function is simply a copy of the default handler but performs SDIO reset at the
 * beginning.
 */
HAL_StatusTypeDef sdio_hal_init_card(SDIO_HandleTypeDef *hsdio)
{
	uint32_t errorstate;
	uint32_t timeout = 0U;
	uint16_t sdio_rca = 1U;
	uint32_t Resp4;
	uint32_t nbr_of_func;

	/* Identify card operating voltage */
	errorstate = SDMMC_CmdGoIdleState(hsdio->Instance);
	if (errorstate != HAL_SDIO_ERROR_NONE) {
		LOG_ERR("SDIO idle request (CMD0) failed");
		return HAL_ERROR;
	}

	/* Check the power State */
	if (SDMMC_GetPowerState(hsdio->Instance) == 0U) {
		return HAL_ERROR;
	}

	/* Perform card reset. HAL_SDIO_CardReset() seems to not set correct bit. */
	HAL_SDIO_DirectCmd_TypeDef arg = {
		.Reg_Addr = SDIO_RESET_CCCR,
		.IOFunctionNbr = 0,
		.ReadAfterWrite = HAL_SDIO_WRITE_ONLY,
	};
	/* Set state to use WriteDirect function during initialization. */
	hsdio->State = HAL_SDIO_STATE_READY;
	(void)HAL_SDIO_WriteDirect(hsdio, &arg, SDIO_RESET_BIT);
	/* Bring back original state */
	hsdio->State = HAL_SDIO_STATE_RESET;
	/* Send CMD5 */
	errorstate = SDMMC_CmdSendOperationcondition(hsdio->Instance, 0U, &Resp4);
	if (errorstate != HAL_SDIO_ERROR_NONE) {
		LOG_ERR("SDIO OCR query (CMD5) failed");
		return HAL_ERROR;
	}

	nbr_of_func = ((Resp4 & 0x70000000U) >> 28U);
	/* Check if Nbr of function > 0 and OCR valid */
	if (nbr_of_func > 0U) {
		/* Send CMD5 with arg= S18R, WV.
		 * Workaround: after a DeInit->Init cycle the SDIO device
		 * may return OCR=0 to CMD5(arg=0) and the subsequent
		 * CMD5(arg=OCR) may fail. In that case the device has already
		 * acknowledged the host (bit[31]=1 on arg=0), so skip the OCR
		 * set failure and proceed to CMD3 directly. */
		uint32_t cmd5_ocr_ret = SDMMC_CmdSendOperationcondition(hsdio->Instance,
									(SDIO_OCR_VDD_32_33 | SDIO_OCR_SDIO_S18R),
									&Resp4);
		if (cmd5_ocr_ret != HAL_SDIO_ERROR_NONE) {
			//LOG_INF("SDIO OCR set (CMD5) failed (err=%lu), skip and try CMD3", (unsigned long)cmd5_ocr_ret);
		} else {
			//LOG_INF("CMD5(arg=OCR) Resp4 = 0x%08X, IORDY = %u, S18A = %u", (unsigned int)Resp4, (unsigned int)((Resp4 >> 31) & 1), (unsigned int)((Resp4 >> 24) & 1));
			/* Check if IORDY = 1 and S18A = 1 */
			if ((((Resp4 & 0x80000000U) >> 31U) != 0U) &&
			    (((Resp4 & 0x1000000U) >> 24U) != 0U)) {
				/* Send CMD11 to switch 1.8V mode */
				errorstate = SDMMC_CmdVoltageSwitch(hsdio->Instance);
				if (errorstate != HAL_SDIO_ERROR_NONE) {
					LOG_ERR("SDIO voltage switch (CMD11) failed");
					return HAL_ERROR;
				}
			} else {
				/* S18A is not supported */
			}
		}
	}

	/** Cmd3 is sent while response is SDMMC_ERROR_ILLEGAL_CMD, due to the partial init test
	 * done before (sending cmd0 after the sequence cmd0/cmd3 is sent is considered illegal).
	 */
	do {
		errorstate = SDMMC_CmdSetRelAdd(hsdio->Instance, &sdio_rca);
        if (errorstate == SDMMC_ERROR_NONE) {
            break;
        }
		timeout++;
		HAL_Delay(1);
	} while ((errorstate == SDMMC_ERROR_ILLEGAL_CMD) && (timeout != SDIO_RW_TIMEOUT_MS));

	if ((timeout == SDIO_RW_TIMEOUT_MS) || (errorstate != HAL_SDIO_ERROR_NONE)) {
		LOG_ERR("SDIO set address (CMD3) failed");
		return HAL_ERROR;
	}

	sdio_hal_from_handle(hsdio)->rca = sdio_rca;
/* Select the Card ( Sending CMD7)*/
	errorstate = SDMMC_CmdSelDesel(hsdio->Instance, (uint32_t)(((uint32_t)sdio_rca) << 16U));
	if (errorstate != HAL_SDIO_ERROR_NONE) {
		LOG_ERR("SDIO select card (CMD7) successful");
		return HAL_ERROR;
	}

	RCP_STAT_RECOVER_STAMP_ONCE(t_card_identified_us);
	return HAL_OK;
}

bool sdio_cmd53_write(struct sdio_hal *handle, uint8_t func, uint32_t reg, uint8_t *data, uint16_t len)
{
	HAL_SDIO_ExtendedCmd_TypeDef arg = {
			.OpCode = HAL_SDIO_OP_CODE_NO_INC,
			/* For sizes larger than 512 switch to block mode (regardless of function BS). */
			.Block_Mode = (len >= SDIO_BS_DEFAULT) ? HAL_SDIO_MODE_BLOCK : HAL_SDIO_MODE_BYTE,
			.Reg_Addr = reg,
			.IOFunctionNbr = func,
	};
	SDIO_DMA_ASSERT(handle != NULL, "null handle", data, len);
	SDIO_DMA_ASSERT(data != NULL, "null buffer", data, len);
	SDIO_DMA_ASSERT(len != 0U, "zero length", data, len);
	SDIO_DMA_ASSERT(sdio_dma_buf_is_accessible(data, len),
			"dma buffer is outside DMA-accessible AXI SRAM", data, len);
	SDIO_DMA_ASSERT(sdio_dma_buf_alignment_is_valid(data, len),
			"dma buffer alignment is invalid", data, len);
	SDIO_DMA_ASSERT((len <= SDIO_BS_DEFAULT) || ((len % handle->functions[func].bs) == 0U),
			"block-mode length mismatch", data, len);

	handle->error = false;
	RCP_TX_PROF_CLEAR(handle->dma_cplt_cycles);
	(void)rcp_os_sem_take(&handle->cplt_sem, 0);

	bool status = true;

	sdio_dma_clean_for_write(data, len);
	int ret = HAL_SDIO_WriteExtended_DMA(&handle->sdio, &arg, data, len);

	if (ret != HAL_OK) {
		//LOG_ERR("SDIO DMA write request failed: %d", ret);
		sdio_hal_recover_after_err(handle);
		return false;
	}

	RCP_TX_PROF_BEGIN(sdio_dma_wait_start_cycles);
	if (!rcp_os_sem_take(&handle->cplt_sem, SDIO_RW_TIMEOUT_MS) || handle->error) {
		RCP_TX_PROF_ACC(RCP_TX_PROF_SDIO_DMA_WAIT, sdio_dma_wait_start_cycles);
		RCP_TX_PROF_ACC_IRQ_RESUME(handle->dma_cplt_cycles, sdio_dma_wait_start_cycles);
		//LOG_ERR("Error during SDIO DMA write err:%d", handle->error);
		sdio_hal_recover_after_err(handle);
		status = false;
	} else {
		RCP_TX_PROF_ACC(RCP_TX_PROF_SDIO_DMA_WAIT, sdio_dma_wait_start_cycles);
		RCP_TX_PROF_ACC_IRQ_RESUME(handle->dma_cplt_cycles, sdio_dma_wait_start_cycles);
	}

	return status;
}

bool sdio_cmd53_read(struct sdio_hal *handle, uint8_t func, uint32_t reg, uint8_t *data, uint16_t len)
{
	HAL_SDIO_ExtendedCmd_TypeDef arg = {
		.OpCode = HAL_SDIO_OP_CODE_NO_INC,
		.Block_Mode = (len >= SDIO_BS_DEFAULT) ? HAL_SDIO_MODE_BLOCK : HAL_SDIO_MODE_BYTE,
		.Reg_Addr = reg,
		.IOFunctionNbr = func,
	};
	SDIO_DMA_ASSERT(handle != NULL, "null handle", data, len);
	SDIO_DMA_ASSERT(data != NULL, "null buffer", data, len);
	SDIO_DMA_ASSERT(len != 0U, "zero length", data, len);
	SDIO_DMA_ASSERT(sdio_dma_buf_is_accessible(data, len),
			"dma buffer is outside DMA-accessible AXI SRAM", data, len);
	SDIO_DMA_ASSERT(sdio_dma_buf_alignment_is_valid(data, len),
			"dma buffer alignment is invalid", data, len);
	SDIO_DMA_ASSERT((len <= SDIO_BS_DEFAULT) || ((len % handle->functions[func].bs) == 0U),
			"block-mode length mismatch", data, len);
	handle->error = false;
	RCP_TX_PROF_CLEAR(handle->dma_cplt_cycles);

	(void)rcp_os_sem_take(&handle->cplt_sem, 0);
	bool status = true;
	sdio_dma_invalidate_for_read(data, len);
	int ret = HAL_SDIO_ReadExtended_DMA(&handle->sdio, &arg, data, len);

	if (ret != HAL_OK) {
		LOG_ERR("SDIO DMA read request failed %d", ret);
		sdio_hal_recover_after_err(handle);
		return false;
	}
	if (!rcp_os_sem_take(&handle->cplt_sem, SDIO_RW_TIMEOUT_MS) || handle->error) {
		LOG_ERR("Error during SDIO DMA read");
		sdio_hal_recover_after_err(handle);
		status = false;
	} else {
		sdio_dma_invalidate_for_read(data, len);
	}

	return status;
}

void sdio_hal_chip_en_pulse(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Drive CHIP_EN high as push-pull to enable the device. */
    GPIO_InitStruct.Pin = CHIP_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CHIP_EN_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(CHIP_EN_GPIO_Port, CHIP_EN_Pin, GPIO_PIN_SET);

    rcp_os_delay_ms(300);

    /* Release CHIP_EN to high-Z (analog input, no pull) so external
     * pull/keeper circuitry holds the level. */
    GPIO_InitStruct.Pin = CHIP_EN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CHIP_EN_GPIO_Port, &GPIO_InitStruct);
}

bool sdio_hal_init(struct sdio_hal *handle)
{
    sdio_hal_chip_en_pulse();

	handle->rca = 0U;
	handle->dat1_wait_active = false;
	MX_SDMMC1_SDIO_Init(&handle->sdio);

	if (!rcp_os_sem_init(&handle->cplt_sem, 0, 1)) {
		return false;
	}
	if (!rcp_os_sem_init(&handle->dat1_sem, 0, 1)) {
		return false;
	}
	if (!sdio_register_dma_callbacks(handle)) {
		return false;
	}

	return true;
}

bool sdio_hal_reinit(struct sdio_hal *handle)
{
	handle->rca = 0U;
	handle->dat1_wait_active = false;
	__HAL_SDIO_DISABLE_IT(&handle->sdio, SDMMC_IT_SDIOIT);
	__HAL_SDIO_CLEAR_FLAG(&handle->sdio, SDMMC_FLAG_SDIOIT);

	/* memset of the HAL handle alone is NOT enough: it only resets
	 * the RAM-side bookkeeping (State, Context, callbacks, ...). The
	 * SDMMC1 controller registers (CMD, DCTRL, DLEN, IDMACTRL, STA,
	 * CPSM/DPSM state machines) keep their previous values, so if the
	 * previous transfer left the controller stuck (CPSMACT=1 waiting
	 * for an R-response that will never come because the device went
	 * away), the next HAL_SDIO_*Extended_DMA call will see the dirty
	 * registers and fail or hang.
	 *
	 * Force-reset the SDMMC1 IP through RCC. This drops every SDMMC1
	 * register back to its default, including CPSM/DPSM state, but
	 * does NOT touch the GPIO pins or the NVIC, so it does not
	 * disturb the device side (no SDMMC bus power-off, unlike
	 * HAL_SDIO_DeInit / POWER=0). */
	__HAL_RCC_SDMMC1_FORCE_RESET();
	__HAL_RCC_SDMMC1_RELEASE_RESET();

	memset(&handle->sdio, 0, sizeof(handle->sdio));
	if (!MX_SDMMC1_SDIO_Init(&handle->sdio)) {
		/* HAL_SDIO_Init failed (e.g. card identification timed out
		 * because the device went away again). Caller will retry. */
		return false;
	}
	return sdio_register_dma_callbacks(handle);
}

bool sdio_hal_wait_dat1_irq(struct sdio_hal *handle, uint32_t timeout_ms)
{
	handle->dat1_wait_active = true;
	__HAL_SDIO_ENABLE_IT(&handle->sdio, SDMMC_IT_SDIOIT);

	bool got = rcp_os_sem_take(&handle->dat1_sem, timeout_ms);

	if (!got) {
		__HAL_SDIO_DISABLE_IT(&handle->sdio, SDMMC_IT_SDIOIT);
		handle->dat1_wait_active = false;
		(void)rcp_os_sem_take(&handle->dat1_sem, 0);
	}

	return got;
}
