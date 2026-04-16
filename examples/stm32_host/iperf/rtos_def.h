
#ifndef __RTOS_DEF_H__
#define __RTOS_DEF_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

/**
 * Type by which tasks are referenced.
 */
typedef void *        rtos_task_t;



typedef TaskHandle_t  rtos_task_handle;

/// RTOS priority
typedef uint32_t      rtos_prio;

/// RTOS task function
typedef void *        rtos_task_fct;

/// RTOS queue
typedef void *        rtos_queue;

/// RTOS semaphore
typedef void *        rtos_semaphore;

/// RTOS mutex
typedef void *        rtos_mutex;

#ifdef CFG_QCC74x_WIFI_PS_ENABLE
typedef TimerHandle_t rtos_timer_t;

typedef void (*rtos_timer_callback_t)(void *);
#endif

#define g_tskIDLE_PRIORITY 0
#define RTOS_TASK_PRIORITY(prio)  (g_tskIDLE_PRIORITY + (prio))

#define RTOS_portTASK_FUNCTION(vFunction, pvParameters) void vFunction(void* pvParameters)
#define RTOS_TASK_FCT(name)        RTOS_portTASK_FUNCTION(name, env)

#define RTOS_TASK_NULL             NULL

#endif
