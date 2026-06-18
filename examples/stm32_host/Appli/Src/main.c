/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lwip/tcpip.h"
#include "stdio.h"
#if defined(__ICCARM__)
#include <LowLevelIOInterface.h>
#endif /* __ICCARM__ */
#include "sdio_hal.h"
#include "shell_commands.h"
#include <rcp_lwip_netif.h>
#include <wifi/rcp_wifi_mgmt.h>
#include "stm32h7rsxx_hal_sdio.h"
#include "rcp_host_config.h"
#include "rcp_bus.h"
#include "rcp_bus_sdio_master.h"
#include "rcp_interface.h"
#include "app_ble.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AXI_SRAM_CACHEABLE_BASE_256K    0x24000000U
#define AXI_SRAM_CACHEABLE_BASE_128K    0x24040000U
#define AXI_SRAM_NONCACHEABLE_BASE_8K0  0x24052000U
#define AXI_SRAM_NONCACHEABLE_BASE_16K  0x24054000U
#define AXI_SRAM_NONCACHEABLE_BASE_32K  0x24058000U
#define AXI_SRAM_NONCACHEABLE_BASE_64K  0x24060000U
#define AXI_SRAM_NONCACHEABLE_BASE_8K1  0x24070000U
#define AXI_SRAM_NONCACHEABLE_BASE_4K0  0x24070000U
#define AXI_SRAM_NONCACHEABLE_BASE_4K1  0x24071000U
#define GFXMMU_VIRTUAL_BUFFER_BASE      0x25000000U
#define SDIO_NOCACHE_TEST_MASK          0x1FU

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

SDIO_HandleTypeDef hsdio1;

UART_HandleTypeDef huart3;

/* Definitions for Start */
osThreadId_t StartHandle;
const osThreadAttr_t Start_attributes = {
  .name = "Start",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MPU_Config(void);
static void MPU_ConfigNonCacheRegion(MPU_Region_InitTypeDef *mpu, uint8_t number, uint32_t base, uint32_t size);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
void rcp_init_thread(void *argument);

/* USER CODE BEGIN PFP */
#if defined(__ICCARM__)
/* New definition from EWARM V9, compatible with EWARM8 */
int iar_fputc(int ch);
#define PUTCHAR_PROTOTYPE int iar_fputc(int ch)
#elif defined ( __CC_ARM ) || defined(__ARMCC_VERSION)
/* ARM Compiler 5/6*/
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#elif defined(__GNUC__)
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#endif /* __ICCARM__ */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t cycles_per_us = 0;

void delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cycles_per_us = SystemCoreClock / 1000000U;
}

void delay_us(uint32_t us)
{
    volatile uint32_t start = DWT->CYCCNT;
    volatile uint32_t cycles = us * cycles_per_us;
    while ((DWT->CYCCNT - start) < cycles);
}

uint32_t delay_get_cycles(void)
{
    return DWT->CYCCNT;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Update SystemCoreClock variable according to RCC registers values. */
  SystemCoreClockUpdate();
  delay_init();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Start */
  StartHandle = osThreadNew(rcp_init_thread, NULL, &Start_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief SDMMC1 Initialization Function
  * @param None
  * @retval None
  */
void MX_SDMMC1_SDIO_Init(void)
{

  /* USER CODE BEGIN SDMMC1_Init 0 */

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */

  /* USER CODE END SDMMC1_Init 1 */
  hsdio1.Instance = SDMMC1;
  hsdio1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsdio1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsdio1.Init.BusWide = SDMMC_BUS_WIDE_4B;
  hsdio1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsdio1.Init.ClockDiv = 50;
  if (HAL_SDIO_Init(&hsdio1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SDMMC1_Init 2 */

  /* USER CODE END SDMMC1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 921600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  __HAL_RCC_SBS_CLK_ENABLE();
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CHIP_EN_GPIO_Port, CHIP_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LED_YELLOW_Pin|LED_GREEN_Pin, GPIO_PIN_RESET);

#if CONFIG_SDIO_OOB
  GPIO_InitStruct.Pin = QCC74X_OOB_WAKEUP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(QCC74X_OOB_WAKEUP_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(QCC74X_OOB_WAKEUP_GPIO_Port, QCC74X_OOB_WAKEUP_Pin, 0);
#endif
  /*Configure GPIO pin : CHIP_EN_Pin */
  GPIO_InitStruct.Pin = CHIP_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CHIP_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_RED_Pin */
  GPIO_InitStruct.Pin = LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LED_RED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_YELLOW_Pin LED_GREEN_Pin */
  GPIO_InitStruct.Pin = LED_YELLOW_Pin|LED_GREEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MPU_ConfigNonCacheRegion(MPU_Region_InitTypeDef *mpu, uint8_t number, uint32_t base, uint32_t size)
{
  mpu->Enable = MPU_REGION_ENABLE;
  mpu->Number = number;
  mpu->BaseAddress = base;
  mpu->Size = size;
  mpu->SubRegionDisable = 0x0;
  mpu->TypeExtField = MPU_TEX_LEVEL1;
  mpu->AccessPermission = MPU_REGION_FULL_ACCESS;
  mpu->DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  mpu->IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  mpu->IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  mpu->IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(mpu);
}

#if defined(__ICCARM__)
size_t __write(int file, unsigned char const *ptr, size_t len)
{
  size_t idx;
  unsigned char const *pdata = ptr;

  for (idx = 0; idx < len; idx++)
  {
    iar_fputc((int)*pdata);
    pdata++;
  }
  return len;
}
#endif /* __ICCARM__ */

/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of putchar here */
  /* e.g. write a character to the USART3 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_rcp_init_thread */
/**
  * @brief  Function implementing the Start thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_rcp_init_thread */
void rcp_init_thread(void *argument)
{
  /* USER CODE BEGIN 5 */
	/* Initialize SDIO bus */
	if (!rcp_bus_sdio_master_init()) {
		printf("Failed to initialize RCP bus\n");
	}

	/* Initialize RCP protocol */
	(void)init_rcp(RCP_BUS_SDIO);

	osDelay(50);

	if (rcp_wifi_init() != RCP_WIFI_OK) {
		printf("Failed to initialize RCP WiFi\n");
	}

	/* rcp_lwip_netif_start() initializes lwIP core and registers the RCP-backed netif. */
	if (rcp_lwip_init() != ERR_OK) {
		printf("Failed to initialize RCP lwIP netif\n");
	}


    BleAppInit();
    osDelay(200);
    BleAppAciInit();

    RCP_Init(RCP_ObjGet());

	/* Initialize and start shell */
	shell_commands_init();
	shell_task_start();

    for(;;)
    {
      /* Delete the Init Thread */
      osThreadTerminate(StartHandle);
    }
  /* USER CODE END 5 */
}

 /* MPU Configuration */

static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /* Disables all MPU regions */
  for(uint8_t i=0; i<__MPU_REGIONCOUNT; i++)
  {
    HAL_MPU_DisableRegion(i);
  }

  /* Region 0: default deny, with 0x20000000-0x3fffffff kept for explicit regions. */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Region 1: XSPI mapped image, 0x70000000-0x77ffffff. */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x70000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_128MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /*
   * Region 2: GFXMMU virtual buffers, 0x25000000-0x25ffffff.
   * STM32H7R/S Rev B can hang on speculative access here when GFXMMU is disabled.
   */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = GFXMMU_VIRTUAL_BUFFER_BASE;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Region 4: AXI SRAM1/2 cacheable, 0x24000000-0x2403ffff. */
  MPU_InitStruct.Number = MPU_REGION_NUMBER4;
  MPU_InitStruct.BaseAddress = AXI_SRAM_CACHEABLE_BASE_256K;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Region 5: AXI SRAM3 cacheable, 0x24040000-0x2405ffff. */
  MPU_InitStruct.Number = MPU_REGION_NUMBER5;
  MPU_InitStruct.BaseAddress = AXI_SRAM_CACHEABLE_BASE_128K;
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

#if (SDIO_NOCACHE_TEST_MASK & 0x01U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER6, AXI_SRAM_NONCACHEABLE_BASE_8K0, MPU_REGION_SIZE_8KB);
#endif
#if (SDIO_NOCACHE_TEST_MASK & 0x02U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER7, AXI_SRAM_NONCACHEABLE_BASE_16K, MPU_REGION_SIZE_16KB);
#endif
#if (SDIO_NOCACHE_TEST_MASK & 0x04U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER8, AXI_SRAM_NONCACHEABLE_BASE_32K, MPU_REGION_SIZE_32KB);
#endif
#if (SDIO_NOCACHE_TEST_MASK & 0x08U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER9, AXI_SRAM_NONCACHEABLE_BASE_64K, MPU_REGION_SIZE_64KB);
#endif
#if (SDIO_NOCACHE_TEST_MASK & 0x10U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER11, AXI_SRAM_NONCACHEABLE_BASE_8K1, MPU_REGION_SIZE_8KB);
#endif
#if (SDIO_NOCACHE_TEST_MASK & 0x20U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER11, AXI_SRAM_NONCACHEABLE_BASE_4K0, MPU_REGION_SIZE_4KB);
#endif
#if (SDIO_NOCACHE_TEST_MASK & 0x40U)
  MPU_ConfigNonCacheRegion(&MPU_InitStruct, MPU_REGION_NUMBER12, AXI_SRAM_NONCACHEABLE_BASE_4K1, MPU_REGION_SIZE_4KB);
#endif

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  printf("Critical Error \r\n");
  while (1)
  {
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
    HAL_Delay(500);
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
