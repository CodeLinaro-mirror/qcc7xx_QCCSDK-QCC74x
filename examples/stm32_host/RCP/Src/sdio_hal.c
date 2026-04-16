#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include <rcp_config.h>
#include <rcp_os.h>
#include "stm32h7rsxx_hal.h"
#include "sdio_hal.h"
#include "log.h"

#define SDIO_RW_TIMEOUT_MS 1000

#define SDIO_OCR_VDD_32_33 (1UL << 20U)
#define SDIO_OCR_SDIO_S18R (1UL << 24U)
#define SDIO_RESET_CCCR    0x06
#define SDIO_RESET_BIT     (1 << 3)
static SDIO_HandleTypeDef *hsdio = NULL;

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

	(void)rcp_os_sem_give(&handle->cplt_sem);
}

static void sdio_error_callback(SDIO_HandleTypeDef *sdio)
{
	struct sdio_hal *handle = sdio_hal_from_handle(sdio);

	handle->error = true;
	printf("sdio err\r\n");
	(void)rcp_os_sem_give(&handle->cplt_sem);
}

static void hal_sdio_func_callback(SDIO_HandleTypeDef *hsdio, uint32_t func)
{
	struct sdio_hal *handle = sdio_hal_from_handle(hsdio);

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
  * @param None
  * @retval None
  */
static void MX_SDMMC1_SDIO_Init(SDIO_HandleTypeDef *hsdio1)
{

  /* USER CODE BEGIN SDMMC1_Init 0 */
  //HAL_GPIO_WritePin(CHIP_EN_GPIO_Port, CHIP_EN_Pin, GPIO_PIN_SET);

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
    Error_Handler();
  }
  hsdio = hsdio1;
  /* USER CODE BEGIN SDMMC1_Init 2 */

  /* USER CODE END SDMMC1_Init 2 */

}

void sdio_hal_irq_clear(struct sdio_hal *handle)
{
	__HAL_SDIO_CLEAR_FLAG(&handle->sdio, SDMMC_FLAG_SDIOIT);
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
		LOG_ERR("Direct RW failed");
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
	if (HAL_OK != HAL_SDIO_EnableIOFunctionInterrupt(&handle->sdio, func)) {
		return false;
	}
	if (HAL_OK != HAL_SDIO_RegisterIOFunctionCallback(&handle->sdio, func, hal_sdio_func_callback)) {
		return false;
	}
	handle->functions[func].cb = cb;
	handle->functions[func].cb_arg = cb_arg;
	return true;
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
		/* Send CMD5 with arg= S18R, WV*/
		if (SDMMC_CmdSendOperationcondition(hsdio->Instance,
						    (SDIO_OCR_VDD_32_33 | SDIO_OCR_SDIO_S18R),
						    &Resp4) != HAL_SDIO_ERROR_NONE) {
			LOG_ERR("SDIO OCR set (CMD5) failed");
			return HAL_ERROR;
		}
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

	/** Cmd3 is sent while response is SDMMC_ERROR_ILLEGAL_CMD, due to the partial init test
	 * done before (sending cmd0 after the sequence cmd0/cmd3 is sent is considered illegal).
	 */
	do {
		errorstate = SDMMC_CmdSetRelAdd(hsdio->Instance, &sdio_rca);
		timeout++;
		HAL_Delay(1);
	} while ((errorstate == SDMMC_ERROR_ILLEGAL_CMD) && (timeout != SDIO_RW_TIMEOUT_MS));

	if ((timeout == SDIO_RW_TIMEOUT_MS) || (errorstate != HAL_SDIO_ERROR_NONE)) {
		LOG_ERR("SDIO set address (CMD3) failed");
		return HAL_ERROR;
	}

	/* Select the Card ( Sending CMD7)*/
	errorstate = SDMMC_CmdSelDesel(hsdio->Instance, (uint32_t)(((uint32_t)sdio_rca) << 16U));
	if (errorstate != HAL_SDIO_ERROR_NONE) {
		LOG_ERR("SDIO select card (CMD7) successful");
		return HAL_ERROR;
	}

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
	if (len > SDIO_BS_DEFAULT && (len % handle->functions[func].bs)) {
			LOG_ERR("SDIO data size (%d) has to be a multiple of block size (%d) in block mode",
					len, handle->functions[func].bs);
			return false;
	}

	handle->error = false;

	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_TX_CPLT_CB_ID, sdio_txrx_cplt_callback)) {
		return false;
	}
	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_ERROR_CB_ID, sdio_error_callback)) {
		return false;
	}
	/* Reset a binary semaphore by taking it without delay. */
	(void)rcp_os_sem_take(&handle->cplt_sem, 0);

	bool status = true;

	SCB_CleanDCache_by_Addr(data, len);

	int ret = HAL_SDIO_WriteExtended_DMA(&handle->sdio, &arg, data, len);

	if (ret != HAL_OK) {
		LOG_ERR("SDIO DMA write request failed: %d", ret);
		status = false;
		goto end;
	}

	if (!rcp_os_sem_take(&handle->cplt_sem, SDIO_RW_TIMEOUT_MS) || handle->error) {
		LOG_ERR("Error during SDIO DMA write");
		status = false;
		goto end;
	}

end:
	(void)HAL_SDIO_UnRegisterCallback(&handle->sdio, HAL_SDIO_TX_CPLT_CB_ID);
	(void)HAL_SDIO_UnRegisterCallback(&handle->sdio, HAL_SDIO_ERROR_CB_ID);

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
	if (len > SDIO_BS_DEFAULT && (len % handle->functions[func].bs)) {
			LOG_ERR("SDIO data size (%d) has to be a multiple of block size (%d) in block mode",
					len, handle->functions[func].bs);
			return false;
	}
	handle->error = false;

	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_RX_CPLT_CB_ID, sdio_txrx_cplt_callback)) {
		return false;
	}
	if (HAL_OK != HAL_SDIO_RegisterCallback(&handle->sdio, HAL_SDIO_ERROR_CB_ID, sdio_error_callback)) {
		return false;
	}

	/* Reset a binary semaphore by taking it without delay. */
	(void)rcp_os_sem_take(&handle->cplt_sem, 0);
	bool status = true;
	int ret = HAL_SDIO_ReadExtended_DMA(&handle->sdio, &arg, data, len);

	if (ret != HAL_OK) {
		LOG_ERR("SDIO DMA read request failed %d", ret);
		status = false;
		goto end;
	}
	if (!rcp_os_sem_take(&handle->cplt_sem, SDIO_RW_TIMEOUT_MS) || handle->error) {
		LOG_ERR("Error during SDIO DMA read");
		status = false;
		goto end;
	}
	/* Success */
	SCB_InvalidateDCache_by_Addr(data, len);

end:
	(void)HAL_SDIO_UnRegisterCallback(&handle->sdio, HAL_SDIO_RX_CPLT_CB_ID);
	(void)HAL_SDIO_UnRegisterCallback(&handle->sdio, HAL_SDIO_ERROR_CB_ID);

	return status;
}

bool sdio_hal_init(struct sdio_hal *handle)
{
	MX_SDMMC1_SDIO_Init(&handle->sdio);

	if (!rcp_os_sem_init(&handle->cplt_sem, 0, 1)) {
		return false;
	}

	return true;
}
