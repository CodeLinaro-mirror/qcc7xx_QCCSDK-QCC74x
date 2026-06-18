#include "main.h"
#include <stdint.h>

uint32_t sys_now(void) { return HAL_GetTick(); }
