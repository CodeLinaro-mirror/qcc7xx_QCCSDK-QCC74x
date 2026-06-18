/* rcp_perf port implementation backed by ARMv7-M DWT->CYCCNT.
 *
 * Suitable for any Cortex-M3/M4/M7 SoC where DWT and SystemCoreClock are
 * available. The cycle counter must be enabled before the first rcp_perf_*
 * call (typically by the platform's clock init); on STM32 host this is
 * handled in main.c::delay_init(). */

#include <stdint.h>

#include "rcp_perf.h"

/* Pull in CMSIS so DWT and SystemCoreClock are visible.  This file is a
 * platform port and is allowed to include vendor SDK headers; protocol
 * core code is not. The header path varies between SoC families, so
 * pick whichever CMSIS-Core header the project's vendor SDK exposes. */
#if defined(STM32H7S3xx) || defined(STM32H7S7xx) || defined(STM32H7R3xx) || defined(STM32H7R7xx)
#  include "stm32h7rsxx.h"
#elif defined(STM32H743xx) || defined(STM32H7xx)
#  include "stm32h7xx.h"
#elif defined(STM32F4)
#  include "stm32f4xx.h"
#else
#  include "cmsis_compiler.h"
#endif

uint32_t rcp_perf_cycles(void)
{
	return (uint32_t)DWT->CYCCNT;
}

uint32_t rcp_perf_cycle_hz(void)
{
	return (uint32_t)SystemCoreClock;
}

uint32_t rcp_perf_now_us(void)
{
	uint32_t mhz = SystemCoreClock / 1000000U;
	if (mhz == 0U) {
		return 0U;
	}
	return (uint32_t)DWT->CYCCNT / mhz;
}

uint32_t rcp_perf_now_ms(void)
{
	uint32_t khz = SystemCoreClock / 1000U;
	if (khz == 0U) {
		return 0U;
	}
	return (uint32_t)DWT->CYCCNT / khz;
}

uint64_t rcp_perf_cycles_to_us(uint64_t cycles)
{
	if (SystemCoreClock == 0U) {
		return 0U;
	}
	return (cycles * 1000000ULL) / SystemCoreClock;
}

uint64_t rcp_perf_cycles_to_ms(uint64_t cycles)
{
	if (SystemCoreClock == 0U) {
		return 0U;
	}
	return (cycles * 1000ULL) / SystemCoreClock;
}
