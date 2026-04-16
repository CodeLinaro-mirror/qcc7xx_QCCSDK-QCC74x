#ifndef __RCP_OS_H__
#define __RCP_OS_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "rcp_config.h"

#ifdef RCP_OS_ZEPHYR
#include <zephyr/kernel.h>
#elif defined(RCP_OS_FREERTOS)
#include <FreeRTOS.h>
#include <event_groups.h>
#include <semphr.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#elif defined(RCP_OS_LINUX)
#include <pthread.h>
#include <semaphore.h>
#endif

typedef struct {
#ifdef RCP_OS_ZEPHYR
	struct k_sem _sem;
#elif defined(RCP_OS_FREERTOS)
	SemaphoreHandle_t _sem;
#elif defined(RCP_OS_LINUX)
	sem_t _sem;
#endif
} rcp_sem_t;

typedef struct {
#ifdef RCP_OS_ZEPHYR
	struct k_mutex _mutex;
#elif defined(RCP_OS_FREERTOS)
	SemaphoreHandle_t _mutex;
#elif defined(RCP_OS_LINUX)
	pthread_mutex_t _mutex;
#endif
} rcp_mutex_t;

typedef struct {
#ifdef RCP_OS_ZEPHYR
	struct k_queue _queue;
#elif defined(RCP_OS_FREERTOS)
	QueueHandle_t _queue;
#elif defined(RCP_OS_LINUX)
	void *_queue;
#endif
} rcp_queue_t;

typedef struct {
#ifdef RCP_OS_ZEPHYR
	struct k_event _event;
#elif defined(RCP_OS_FREERTOS)
	EventGroupHandle_t _event;
#elif defined(RCP_OS_LINUX)
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	uint32_t _bits;
#endif
} rcp_event_t;

typedef void (*rcp_thread_entry_t)(void *arg);

typedef struct {
	rcp_thread_entry_t _entry;
	void *_arg;
#ifdef RCP_OS_ZEPHYR
	void *_thread;
#elif defined(RCP_OS_FREERTOS)
	TaskHandle_t _thread;
#elif defined(RCP_OS_LINUX)
	pthread_t _thread;
#endif
} rcp_thread_t;

typedef void (*rcp_timer_callback_t)(void *arg);

typedef struct {
	rcp_timer_callback_t _cb;
	void *_arg;
#ifdef RCP_OS_ZEPHYR
	struct k_work_delayable _timer;
#elif defined(RCP_OS_FREERTOS)
	TimerHandle_t _timer;
#elif defined(RCP_OS_LINUX)
	pthread_t _thread;
	pthread_mutex_t _lock;
	pthread_cond_t _cond;
	bool _armed;
	bool _stop;
	uint32_t _timeout_ms;
#endif
} rcp_timer_t;

#define RCP_OS_WAIT_FOREVER UINT32_MAX

/* Semaphore APIs */
bool rcp_os_sem_init(rcp_sem_t *sem, uint32_t cnt, uint32_t limit);
bool rcp_os_sem_give(rcp_sem_t *sem);
bool rcp_os_sem_take(rcp_sem_t *sem, uint32_t timeout);

/* Mutex APIs */
bool rcp_os_mutex_init(rcp_mutex_t *mutex);
bool rcp_os_mutex_lock(rcp_mutex_t *mutex, uint32_t timeout);
bool rcp_os_mutex_unlock(rcp_mutex_t *mutex);

/* Time APIs */
uint32_t rcp_os_time_ms(void);
void rcp_os_delay_ms(uint32_t time_ms);

/* Queue APIs */
bool rcp_os_queue_init(rcp_queue_t *queue, uint32_t item_size, uint32_t max_items);
void rcp_os_queue_deinit(rcp_queue_t *queue);
int rcp_os_queue_send(rcp_queue_t *queue, const void *item, uint32_t timeout_ms);
int rcp_os_queue_send_front(rcp_queue_t *queue, const void *item, uint32_t timeout_ms);
int rcp_os_queue_receive(rcp_queue_t *queue, void *item, uint32_t timeout_ms);
int rcp_os_queue_peek(rcp_queue_t *queue, void *item, uint32_t timeout_ms);
int rcp_os_queue_messages_waiting(rcp_queue_t *queue);
int rcp_os_queue_is_full(rcp_queue_t *queue);

/* Event bit APIs */
bool rcp_os_event_init(rcp_event_t *event);
void rcp_os_event_deinit(rcp_event_t *event);
uint32_t rcp_os_event_set_bits(rcp_event_t *event, uint32_t flags);
uint32_t rcp_os_event_clear_bits(rcp_event_t *event, uint32_t flags);
uint32_t rcp_os_event_get_bits(rcp_event_t *event);
bool rcp_os_event_wait_bits(rcp_event_t *event, uint32_t flags, bool wait_all,
			      uint32_t timeout_ms, uint32_t *matched_flags);

/* Thread APIs */
bool rcp_os_thread_create(rcp_thread_t *thread, rcp_thread_entry_t entry, void *arg,
			   const char *name, uint32_t stack_size, uint32_t priority);

/* One-shot timer APIs */
bool rcp_os_timer_init(rcp_timer_t *timer, rcp_timer_callback_t cb, void *arg);
bool rcp_os_timer_start(rcp_timer_t *timer, uint32_t timeout_ms);
bool rcp_os_timer_stop(rcp_timer_t *timer);
void rcp_os_timer_deinit(rcp_timer_t *timer);

/* Check if in ISR context */
bool rcp_os_in_isr(void);
void rcp_os_disable_irq(void);
void rcp_os_enable_irq(void);

/* Memory APIs */
void *rcp_os_malloc(size_t size);
void rcp_os_free(void *ptr);

#endif
