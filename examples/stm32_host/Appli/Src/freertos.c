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

/* Runtime statistics support (optional, for debugging) */
__weak void configureTimerForRunTimeStats(void)
{
}

__weak unsigned long getRunTimeCounterValue(void)
{
  return 0;
}

/* USER CODE END Application */
