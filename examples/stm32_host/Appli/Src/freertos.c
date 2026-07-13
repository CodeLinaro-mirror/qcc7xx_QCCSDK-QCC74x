/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "rcp_user_config.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/*
 * BLE Stack required FreeRTOS hooks
 */

void vApplicationMallocFailedHook(void)
{
  /* Called when malloc fails - required by BLE Stack */
  taskDISABLE_INTERRUPTS();
  for( ;; );
}

void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
  /* Called when stack overflows - required by BLE Stack */
  (void)xTask;
  (void)pcTaskName;
  taskDISABLE_INTERRUPTS();
  for( ;; );
}

void vApplicationIdleHook(void)
{
  /* Idle hook - optional */
}

static uint32_t runtime_stats_cycle_last;
static uint64_t runtime_stats_cycle_high;

/* Runtime statistics support (optional, for debugging) */
__weak void configureTimerForRunTimeStats(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  runtime_stats_cycle_last = 0U;
  runtime_stats_cycle_high = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

__weak uint64_t getRunTimeCounterValue(void)
{
  UBaseType_t uxSavedInterruptStatus;
  uint32_t cycle;
  uint64_t counter;

  uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
  cycle = DWT->CYCCNT;
  if (cycle < runtime_stats_cycle_last) {
    runtime_stats_cycle_high += (1ULL << 32);
  }
  runtime_stats_cycle_last = cycle;
  counter = runtime_stats_cycle_high | cycle;
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

  return counter;
}

/* USER CODE END Application */
