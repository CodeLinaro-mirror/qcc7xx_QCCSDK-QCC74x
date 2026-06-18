/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7rsxx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32h7rsxx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "rcp_bus_sdio_master.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CRASH_DUMP 0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern SDIO_HandleTypeDef hsdio1;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32H7RSxx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7rsxx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */

  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */

  /* USER CODE END USART3_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void Fault_Handler(void)
{
  // Disable all interrupts to prevent recursion
  __disable_irq();

#if CRASH_DUMP
  // Read critical register information
  uint32_t ipsr = __get_IPSR();
  uint32_t control = __get_CONTROL();
  uint32_t primask = __get_PRIMASK();
  uint32_t basepri = __get_BASEPRI();
  uint32_t faultmask = __get_FAULTMASK();
  uint32_t msp = __get_MSP();
  uint32_t psp = __get_PSP();

  printf("\n\n");
  printf("======================================================================\n");
  printf("           !!! UNHANDLED INTERRUPT / CRASH !!!                       \n");
  printf("======================================================================\n");

  // 1. Interrupt number and type identification
  printf("\n[1. INTERRUPT INFORMATION]\n");
  printf("IPSR (Interrupt Program Status Register): 0x%08lx\n", ipsr);

  // Extract interrupt number (bits 0-8 of IPSR)
  uint32_t irq_number = ipsr & 0x1FF;

  if (irq_number == 0) {
    printf("Mode: Thread Mode (not in interrupt)\n");
  } else if (irq_number >= 1 && irq_number <= 15) {
    // System exception
    const char *exception_names[] = {
      "Reserved",
      "Reset",
      "NMI",
      "HardFault",
      "MemManage",
      "BusFault",
      "UsageFault",
      "Reserved",
      "Reserved",
      "Reserved",
      "Reserved",
      "SVCall",
      "DebugMonitor",
      "Reserved",
      "PendSV",
      "SysTick"
    };
    printf("Exception: %s (Vector #%lu)\n", exception_names[irq_number], irq_number);

    // If fault exception, output detailed information
    if (irq_number >= 3 && irq_number <= 6) {
      printf("\n--- FAULT STATUS REGISTERS ---\n");

      // HardFault Status
      if (irq_number == 3 || irq_number == 4) {
        volatile uint32_t *hfsr = (volatile uint32_t *)0xE000ED2C;
        printf("HardFault Status Register (HFSR):     0x%08lx\n", *hfsr);
        if (*hfsr & (1 << 31)) printf("  - Escalated from configurable fault\n");
        if (*hfsr & (1 << 1))  printf("  - Vector table read error\n");
        if (*hfsr & (1 << 30)) printf("  - Debug event\n");
      }

      // MemManage Fault
      if (irq_number == 4) {
        volatile uint32_t *mmfsr = (volatile uint32_t *)0xE000ED28;
        volatile uint32_t *mmfar = (volatile uint32_t *)0xE000ED34;
        printf("MemManage Fault Status (MMFSR):      0x%02lx\n", *mmfsr & 0xFF);
        printf("MemManage Fault Address (MMFAR):     0x%08lx\n", *mmfar);
      }

      // BusFault
      if (irq_number == 5) {
        volatile uint32_t *bfsr = (volatile uint32_t *)0xE000ED29;
        volatile uint32_t *bfar = (volatile uint32_t *)0xE000ED38;
        printf("BusFault Status Register (BFSR):     0x%02lx\n", *bfsr & 0xFF);
        printf("BusFault Address Register (BFAR):    0x%08lx\n", *bfar);
      }

      // UsageFault
      if (irq_number == 6) {
        volatile uint32_t *ufsr = (volatile uint32_t *)0xE000ED2A;
        printf("UsageFault Status Register (UFSR):   0x%04lx\n", *ufsr & 0xFFFF);
        volatile uint32_t ufsr_val = *ufsr & 0xFFFF;
        if (ufsr_val & (1 << 0))  printf("  - Undefined instruction\n");
        if (ufsr_val & (1 << 1))  printf("  - Invalid state\n");
        if (ufsr_val & (1 << 2))  printf("  - Invalid PC load\n");
        if (ufsr_val & (1 << 3))  printf("  - No coprocessor\n");
        if (ufsr_val & (1 << 8))  printf("  - Unaligned access\n");
        if (ufsr_val & (1 << 9))  printf("  - Divide by zero\n");
      }

      // Configurable Fault Status
      volatile uint32_t *cfsr = (volatile uint32_t *)0xE000ED28;
      printf("Configurable Fault Status (CFSR):    0x%08lx\n", *cfsr);
    }

  } else {
    // External interrupt
    printf("External IRQ #%lu\n", irq_number - 16);

    // Try to identify common interrupts
    const char *irq_names[] = {
      "WWDG", "PVD", "RTC_TAMP_STAMP", "RTC_WKUP", "FLASH",
      "RCC", "EXTI0", "EXTI1", "EXTI2", "EXTI3",
      "EXTI4", "DMA1_Stream0", "DMA1_Stream1", "DMA1_Stream2", "DMA1_Stream3",
      "DMA1_Stream4", "DMA1_Stream5", "DMA1_Stream6", "ADC1", "DAC1",
      "FDCAN1_IT0", "FDCAN2_IT0", "FDCAN1_IT1", "FDCAN2_IT1", "EXTI9_5",
      "TIM1_BRK_TIM15", "TIM1_UP_TIM16", "TIM1_TRGCOM_TIM17", "TIM1_CC", "TIM2",
      "TIM3", "TIM4", "I2C1_EV", "I2C1_ER", "I2C2_EV",
      "I2C2_ER", "SPI1", "SPI2", "USART1", "USART2",
      "USART3", "EXTI15_10", "RTC_Alarm", "TIM8_BRK_TIM12", "TIM8_UP_TIM13",
      "TIM8_TRGCOM_TIM14", "TIM8_CC", "DMA1_Stream7", "FMC", "SDMMC1",
      "TIM5", "SPI3", "UART4", "UART5", "TIM6_DAC",
      "TIM7", "DMA2_Stream0", "DMA2_Stream1", "DMA2_Stream2", "DMA2_Stream3",
      "DMA2_Stream4", "ETH", "ETH_WKUP", "FDCAN_CAL", "DMA2_Stream5",
      "DMA2_Stream6", "DMA2_Stream7", "USART6", "I2C3_EV", "I2C3_ER",
      "OTG_HS_EP1_OUT", "OTG_HS_EP1_IN", "OTG_HS_WKUP", "OTG_HS", "DCMI_PSSI",
      "CRYP", "HASH_RNG", "FPU", "UART7", "UART8",
      "SPI4", "SPI5", "SPI6", "SAI1", "LTDC",
      "LTDC_ER", "DMA2D", "SGUI", "DMA2_Stream8", "QUADSPI",
      "LPTIM1", "CEC", "I2C4_EV", "I2C4_ER", "SPDIF_RX",
      "OTG_FS_EP1_OUT", "OTG_FS_EP1_IN", "OTG_FS_WKUP", "OTG_FS", "DMAMUX1_OVR",
      "HRTIM1_M", "HRTIM1_A", "HRTIM1_B", "HRTIM1_C", "HRTIM1_D",
      "HRTIM1_E", "HRTIM1_F", "HRTIM1_FLT", "HRTIM1_TIMA", "HRTIM1_TIMB",
      "HRTIM1_TIMC", "HRTIM1_TIMD", "HRTIM1_TIME", "HRTIM1_TIMF", "DFSDM1_FLT0",
      "DFSDM1_FLT1", "DFSDM1_FLT2", "DFSDM1_FLT3", "SAI3", "SWPMI1",
      "TIM15", "TIM16", "TIM17", "MDIOS_WKUP", "MDIOS",
      "JPEG", "MDMA", "DTS", "PWR_S3WU", "SDMMC2",
      "HSEM_IT0", "HSEM_IT1", "ADC3", "DMAMUX2_OVR", "BDMA_CH0",
      "BDMA_CH1", "BDMA_CH2", "BDMA_CH3", "BDMA_CH4", "BDMA_CH5",
      "BDMA_CH6", "BDMA_CH7", "COMP1", "LPTIM2", "LPTIM3",
      "LPTIM4", "LPTIM5", "LPUART1", "TSC", "AES3",
      "FMC_Bank1_R", "SPI7", "SPI6", "SAI4", "WWDG_RST",
      "Crs",
      // Continue defining more interrupt names...
    };

    uint32_t ext_irq = irq_number - 16;
    if (ext_irq < sizeof(irq_names)/sizeof(irq_names[0])) {
      printf("IRQ Name: %s\n", irq_names[ext_irq]);
    }
  }

  // 2. Core register status
  printf("\n[2. CORE REGISTERS]\n");
  printf("CONTROL:     0x%08lx ", control);
  printf("[Thread: %s, PSP: %s]\n",
         (control & 2) ? "Handler" : "Thread",
         (control & 2) ? "Active" : "Not Active");

  printf("PRIMASK:     0x%08lx [%s]\n", primask, primask ? "IRQ Disabled" : "IRQ Enabled");
  printf("BASEPRI:     0x%08lx\n", basepri);
  printf("FAULTMASK:   0x%08lx [%s]\n", faultmask, faultmask ? "Faults Disabled" : "Faults Enabled");

  printf("MSP:         0x%08lx (Main Stack Pointer)\n", msp);
  printf("PSP:         0x%08lx (Process Stack Pointer)\n", psp);

  // 3. Stack frame analysis
  printf("\n[3. STACK FRAME ANALYSIS]\n");
  uint32_t *sp = (uint32_t *)msp;
  printf("Stack location: 0x%08lx\n", (uint32_t)sp);
  printf("\nStack dump (top 16 words):\n");
  for (int i = 0; i < 16; i++) {
    printf("  [SP+%2d]: 0x%08lx  %ld\n", i*4, sp[i], sp[i]);
  }

  // 4. Program counter and link register (recovered from stack)
  printf("\n[4. RETURN ADDRESS INFO]\n");
  printf("Note: Exact PC/LR require analyzing exception stack frame\n");
  printf("      Use debugger to examine: x/10x $sp\n");

  // 5. NVIC status
  printf("\n[5. NVIC STATE]\n");
  printf("Active interrupts (first 32):\n");
  for (int i = 0; i < 32; i++) {
    if (NVIC->ISER[0] & (1U << i)) {
      printf("  IRQ %2d: Enabled\n", i);
    }
  }

  // 6. SysTick information
  printf("\n[6. SYSTEM INFO]\n");
  volatile uint32_t *syst_csr = (volatile uint32_t *)0xE000E010;
  volatile uint32_t *syst_rvr = (volatile uint32_t *)0xE000E014;
  volatile uint32_t *syst_cvr = (volatile uint32_t *)0xE000E018;
  printf("SysTick CSR:  0x%08lx [%s]\n", *syst_csr,
         (*syst_csr & (1 << 16)) ? "Exception pending" : "No exception");
  printf("SysTick RVR:  0x%08lx (Reload value)\n", *syst_rvr);
  printf("SysTick CVR:  0x%08lx (Current value)\n", *syst_cvr);

  // 7. Memory and bus fault address
  printf("\n[7. MEMORY ACCESS INFO]\n");
  volatile uint32_t *mmfar = (volatile uint32_t *)0xE000ED34;
  volatile uint32_t *bfar = (volatile uint32_t *)0xE000ED38;
  volatile uint32_t *cfsr = (volatile uint32_t *)0xE000ED28;

  printf("MMFAR:        0x%08lx (MemManage Fault Address)\n", *mmfar);
  printf("BFAR:         0x%08lx (Bus Fault Address)\n", *bfar);

  if (*cfsr & (1 << 7)) {
    printf("  -> MMFAR is valid - Memory access violation at 0x%08lx\n", *mmfar);
  }
  if (*cfsr & (1 << 15)) {
    printf("  -> BFAR is valid - Bus access violation at 0x%08lx\n", *bfar);
  }

  // 8. Diagnosis suggestions
  printf("\n[8. DIAGNOSIS SUGGESTIONS]\n");

  if (irq_number == 3) {  // HardFault
    printf("Possible causes for HardFault:\n");
    printf("  - Null pointer dereference\n");
    printf("  - Stack overflow\n");
    printf("  - Accessing unmapped memory\n");
    printf("  - Dividing by zero\n");
    printf("  - Unaligned access\n");
  } else if (irq_number == 4) {  // MemManage
    printf("Possible causes for MemManage Fault:\n");
    printf("  - Accessing protected memory region (MPU)\n");
    printf("  - Accessing XSPI2 (0x70000000) without proper setup\n");
    printf("  - Code execution from non-executable region\n");
  } else if (irq_number == 5) {  // BusFault
    printf("Possible causes for Bus Fault:\n");
    printf("  - Accessing nonexistent memory\n");
    printf("  - Misaligned memory access\n");
    printf("  - Device access error\n");
  } else if (irq_number == 6) {  // UsageFault
    printf("Possible causes for Usage Fault:\n");
    printf("  - Undefined instruction\n");
    printf("  - Invalid state transition\n");
    printf("  - Divide by zero\n");
    printf("  - Unaligned access\n");
  } else {
    printf("This is an unhandled external interrupt (IRQ #%lu)\n", irq_number - 16);
    printf("Possible causes:\n");
    printf("  - Interrupt handler not registered\n");
    printf("  - Peripheral interrupt not properly configured\n");
    printf("  - Spurious interrupt\n");
    printf("  - Wrong interrupt vector table\n");
  }

  printf("\n[RECOMMENDATIONS]\n");
  printf("1. Attach a debugger to examine the full context\n");
  printf("2. Check the stack trace for the function that caused the fault\n");
  printf("3. Verify all interrupt handlers are properly registered\n");
  printf("4. Check MPU/memory configuration if accessing external memory\n");
  printf("5. Review recent code changes that might cause memory issues\n");

  printf("\n======================================================================\n");
  printf("              SYSTEM HALTED - INVESTIGATE CRASH                    \n");
  printf("======================================================================\n\n");

  /* USER CODE END Default_Handler */
#endif
  // Enter infinite loop, wait for debugger or watchdog reset
  while (1)
  {
    // Can set breakpoint here
    __NOP();
    __WFI();  // Wait for interrupt (for low power)
  }
}

/* USER CODE END 1 */
