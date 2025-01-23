/*
 * dwt.h
 *
 *  Created on: Nov 12, 2024
 */

#ifndef INC_DWT_H_
#define INC_DWT_H_

#include <stdint.h>
#include "system_stm32u5xx.h"
#include "stm32u575xx.h"
#include "core_cm33.h"

static inline void dwt_enable(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
}

static inline void dwt_cycle_counter_enable(void)
{
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t dwt_cycle_counter_read(void)
{
	return DWT->CYCCNT;
}

static inline void dwt_cycle_counter_reset(void)
{
	DWT->CYCCNT = 0;
}

static inline uint32_t dwt_cycle_to_us(uint32_t cycles)
{
	uint32_t f = SystemCoreClock / (1000 * 1000);
	return cycles / f;
}

#endif /* INC_DWT_H_ */
