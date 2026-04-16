#include <stdio.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <shell.h>

void app_atmodule_init(void)
{
    at_module_init();
}

