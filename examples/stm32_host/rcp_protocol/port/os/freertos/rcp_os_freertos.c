#include <stdbool.h>
#include <rcp_os.h>
#include <FreeRTOS.h>
#include <event_groups.h>
#include <semphr.h>
#include <task.h>
#include <queue.h>
#include <stdlib.h>

static inline TickType_t timeout_to_ticks(uint32_t timeout_ms)
{
	return (timeout_ms == RCP_OS_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
}

static void rcp_os_timer_callback(TimerHandle_t handle)
{
	rcp_timer_t *timer = (rcp_timer_t *)pvTimerGetTimerID(handle);

	if (timer != NULL && timer->_cb != NULL) {
		timer->_cb(timer->_arg);
	}
}

static void rcp_os_thread_trampoline(void *arg)
{
	rcp_thread_t *thread = (rcp_thread_t *)arg;

	if (thread != NULL && thread->_entry != NULL) {
		thread->_entry(thread->_arg);
	}

	vTaskDelete(NULL);
}

bool rcp_os_sem_init(rcp_sem_t *sem, uint32_t cnt, uint32_t limit)
{
	sem->_sem = xSemaphoreCreateCounting(limit, cnt);
	return (sem->_sem != NULL);
}

void rcp_os_sem_deinit(rcp_sem_t *sem)
{
	if (sem == NULL || sem->_sem == NULL) {
		return;
	}

	vSemaphoreDelete(sem->_sem);
	sem->_sem = NULL;
}

bool rcp_os_sem_reset(rcp_sem_t *sem)
{
	if (sem == NULL || sem->_sem == NULL || xPortIsInsideInterrupt()) {
		return false;
	}

	xQueueReset((QueueHandle_t)sem->_sem);
	return true;
}

bool rcp_os_sem_give(rcp_sem_t *sem)
{
	if (xPortIsInsideInterrupt()) {
		BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

		if (pdTRUE != xSemaphoreGiveFromISR(sem->_sem, &pxHigherPriorityTaskWoken)) {
			return false;
		}
		if (pxHigherPriorityTaskWoken) {
			portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
		}
		return true;
	}

	if (pdTRUE != xSemaphoreGive(sem->_sem)) {
		return false;
	}
	return true;
}

bool rcp_os_sem_take(rcp_sem_t *sem, uint32_t timeout)
{
	TickType_t ticks = timeout_to_ticks(timeout);

	if (pdTRUE != xSemaphoreTake(sem->_sem, ticks)) {
		return false;
	}
	return true;
}

rcp_sem_t *rcp_os_sem_create(uint32_t limit, uint32_t cnt)
{
	rcp_sem_t *sem = rcp_os_malloc(sizeof(*sem));

	if (sem == NULL) {
		return NULL;
	}

	if (!rcp_os_sem_init(sem, cnt, limit)) {
		rcp_os_free(sem);
		return NULL;
	}

	return sem;
}

void rcp_os_sem_delete(rcp_sem_t *sem)
{
	if (sem == NULL) {
		return;
	}

	rcp_os_sem_deinit(sem);
	rcp_os_free(sem);
}

bool rcp_os_mutex_init(rcp_mutex_t *mutex)
{
	if (mutex == NULL) {
		return false;
	}

	mutex->_mutex = xSemaphoreCreateMutex();
	return (mutex->_mutex != NULL);
}

void rcp_os_mutex_deinit(rcp_mutex_t *mutex)
{
	if (mutex == NULL || mutex->_mutex == NULL) {
		return;
	}

	vSemaphoreDelete(mutex->_mutex);
	mutex->_mutex = NULL;
}

bool rcp_os_mutex_lock(rcp_mutex_t *mutex, uint32_t timeout)
{
	if (mutex == NULL || xPortIsInsideInterrupt()) {
		return false;
	}

	return (xSemaphoreTake(mutex->_mutex, timeout_to_ticks(timeout)) == pdTRUE);
}

bool rcp_os_mutex_unlock(rcp_mutex_t *mutex)
{
	if (mutex == NULL || xPortIsInsideInterrupt()) {
		return false;
	}

	return (xSemaphoreGive(mutex->_mutex) == pdTRUE);
}

uint32_t rcp_os_time_ms(void)
{
	if (xPortIsInsideInterrupt()) {
		return (xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
	}

	return (xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void rcp_os_delay_ms(uint32_t time_ms)
{
	TickType_t ticks = pdMS_TO_TICKS(time_ms);

	if (time_ms != 0U && ticks == 0U) {
		ticks = 1U;
	}

	vTaskDelay(ticks);
}

bool rcp_os_queue_init(rcp_queue_t *queue, uint32_t item_size, uint32_t max_items)
{
	queue->_queue = xQueueCreate(max_items, item_size);
	return (queue->_queue != NULL);
}

void rcp_os_queue_deinit(rcp_queue_t *queue)
{
	if (queue->_queue) {
		vQueueDelete(queue->_queue);
		queue->_queue = NULL;
	}
}

int rcp_os_queue_send(rcp_queue_t *queue, const void *item, uint32_t timeout_ms)
{
	BaseType_t ret;

	if (xPortIsInsideInterrupt()) {
		BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
		ret = xQueueSendFromISR(queue->_queue, item, &pxHigherPriorityTaskWoken);
		if (pxHigherPriorityTaskWoken) {
			portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
		}
	} else {
		TickType_t ticks = timeout_to_ticks(timeout_ms);
		ret = xQueueSend(queue->_queue, item, ticks);
	}

	return (ret == pdTRUE) ? 0 : -1;
}

int rcp_os_queue_send_front(rcp_queue_t *queue, const void *item, uint32_t timeout_ms)
{
	BaseType_t ret;

	if (xPortIsInsideInterrupt()) {
		BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
		ret = xQueueSendToFrontFromISR(queue->_queue, item, &pxHigherPriorityTaskWoken);
		if (pxHigherPriorityTaskWoken) {
			portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
		}
	} else {
		TickType_t ticks = timeout_to_ticks(timeout_ms);
		ret = xQueueSendToFront(queue->_queue, item, ticks);
	}

	return (ret == pdTRUE) ? 0 : -1;
}

int rcp_os_queue_receive(rcp_queue_t *queue, void *item, uint32_t timeout_ms)
{
	BaseType_t ret;

	if (xPortIsInsideInterrupt()) {
		BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
		ret = xQueueReceiveFromISR(queue->_queue, item, &pxHigherPriorityTaskWoken);
		if (pxHigherPriorityTaskWoken) {
			portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
		}
	} else {
		TickType_t ticks = timeout_to_ticks(timeout_ms);
		ret = xQueueReceive(queue->_queue, item, ticks);
	}

	return (ret == pdTRUE) ? 0 : -1;
}

int rcp_os_queue_peek(rcp_queue_t *queue, void *item, uint32_t timeout_ms)
{
	BaseType_t ret;

	if (xPortIsInsideInterrupt()) {
		ret = xQueuePeekFromISR(queue->_queue, item);
	} else {
		TickType_t ticks = timeout_to_ticks(timeout_ms);
		ret = xQueuePeek(queue->_queue, item, ticks);
	}

	return (ret == pdTRUE) ? 0 : -1;
}

int rcp_os_queue_messages_waiting(rcp_queue_t *queue)
{
	if (xPortIsInsideInterrupt()) {
		return uxQueueMessagesWaitingFromISR(queue->_queue);
	}
	return uxQueueMessagesWaiting(queue->_queue);
}

int rcp_os_queue_is_full(rcp_queue_t *queue)
{
	if (xPortIsInsideInterrupt()) {
		return (xQueueIsQueueFullFromISR(queue->_queue) == pdTRUE) ? 1 : 0;
	}
	return (uxQueueSpacesAvailable(queue->_queue) == 0) ? 1 : 0;
}

int rcp_os_queue_spaces_available(rcp_queue_t *queue)
{
	if (queue == NULL || queue->_queue == NULL) {
		return 0;
	}

	return (int)uxQueueSpacesAvailable(queue->_queue);
}

unsigned long rcp_os_sem_count(const rcp_sem_t *sem)
{
	if (sem == NULL || sem->_sem == NULL) {
		return 0;
	}

	return (unsigned long)uxQueueMessagesWaiting((QueueHandle_t)sem->_sem);
}

bool rcp_os_event_init(rcp_event_t *event)
{
	event->_event = xEventGroupCreate();
	return (event->_event != NULL);
}

void rcp_os_event_deinit(rcp_event_t *event)
{
	if (event->_event != NULL) {
		vEventGroupDelete(event->_event);
		event->_event = NULL;
	}
}

uint32_t rcp_os_event_set_bits(rcp_event_t *event, uint32_t flags)
{
	if (xPortIsInsideInterrupt()) {
		BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

		xEventGroupSetBitsFromISR(event->_event, flags, &pxHigherPriorityTaskWoken);

		if (pxHigherPriorityTaskWoken) {
			portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
		}

		return xEventGroupGetBitsFromISR(event->_event);
	} else {
	    return xEventGroupSetBits(event->_event, flags);
    }
}

uint32_t rcp_os_event_clear_bits(rcp_event_t *event, uint32_t flags)
{
	if (xPortIsInsideInterrupt()) {
		EventBits_t bits = xEventGroupGetBitsFromISR(event->_event);

		if (xEventGroupClearBitsFromISR(event->_event, flags) == pdFAIL) {
			return bits;
		}
		return bits & ~flags;
	}

	return xEventGroupClearBits(event->_event, flags) & ~flags;
}

uint32_t rcp_os_event_get_bits(rcp_event_t *event)
{
	if (xPortIsInsideInterrupt()) {
		return xEventGroupGetBitsFromISR(event->_event);
	}

	return xEventGroupGetBits(event->_event);
}

bool rcp_os_event_wait_bits(rcp_event_t *event, uint32_t flags, bool wait_all,
			      uint32_t timeout_ms, uint32_t *matched_flags)
{
	EventBits_t bits;

	if (event == NULL || flags == 0) {
		if (matched_flags) {
			*matched_flags = 0;
		}
		return false;
	}

	bits = xEventGroupWaitBits(event->_event, flags, pdTRUE, wait_all ? pdTRUE : pdFALSE,
				     timeout_to_ticks(timeout_ms));

	if (matched_flags) {
		*matched_flags = (uint32_t)(bits & flags);
	}

	return wait_all ? ((bits & flags) == flags) : ((bits & flags) != 0);
}

bool rcp_os_thread_create(rcp_thread_t *thread, rcp_thread_entry_t entry, void *arg,
			   const char *name, uint32_t stack_size, uint32_t priority)
{
	BaseType_t ret;
	uint32_t stack_words;
	TaskHandle_t handle = NULL;

	if (thread == NULL || entry == NULL) {
		return false;
	}

	thread->_entry = entry;
	thread->_arg = arg;
	thread->_thread = NULL;
	stack_words = (stack_size + sizeof(StackType_t) - 1U) / sizeof(StackType_t);

	ret = xTaskCreate(rcp_os_thread_trampoline, name, stack_words, thread,
			  (UBaseType_t)priority, &handle);
	thread->_thread = handle;

	return (ret == pdPASS);
}

void rcp_os_thread_delete(rcp_thread_t *thread)
{
	if (thread == NULL || thread->_thread == NULL) {
		return;
	}

	vTaskDelete(thread->_thread);
	thread->_thread = NULL;
	thread->_entry = NULL;
	thread->_arg = NULL;
}

bool rcp_os_timer_init(rcp_timer_t *timer, rcp_timer_callback_t cb, void *arg)
{
	if (timer == NULL || cb == NULL) {
		return false;
	}

	timer->_cb = cb;
	timer->_arg = arg;
	timer->_timer = xTimerCreate("rcp_tmr", pdMS_TO_TICKS(1), pdFALSE, timer,
					 rcp_os_timer_callback);

	return (timer->_timer != NULL);
}

bool rcp_os_timer_start(rcp_timer_t *timer, uint32_t timeout_ms)
{
	TickType_t ticks;

	if (timer == NULL || timer->_timer == NULL) {
		return false;
	}

	ticks = timeout_to_ticks(timeout_ms);
	if (ticks == 0U) {
		ticks = 1U;
	}

	return (xTimerChangePeriod(timer->_timer, ticks, 0) == pdPASS);
}

bool rcp_os_timer_stop(rcp_timer_t *timer)
{
	if (timer == NULL || timer->_timer == NULL) {
		return false;
	}

	return (xTimerStop(timer->_timer, 0) == pdPASS);
}

void rcp_os_timer_deinit(rcp_timer_t *timer)
{
	if (timer == NULL || timer->_timer == NULL) {
		return;
	}

	(void)xTimerStop(timer->_timer, 0);
	(void)xTimerDelete(timer->_timer, 0);
	timer->_timer = NULL;
	timer->_cb = NULL;
	timer->_arg = NULL;
}

bool rcp_os_in_isr(void)
{
	return xPortIsInsideInterrupt();
}

void *rcp_os_malloc(size_t size)
{
	return malloc(size);
}

void rcp_os_free(void *ptr)
{
	free(ptr);
}

void rcp_os_disable_irq(void)
{
	if (!xPortIsInsideInterrupt()) {
		taskENTER_CRITICAL();
	}
}

void rcp_os_enable_irq(void)
{
	if (!xPortIsInsideInterrupt()) {
		taskEXIT_CRITICAL();
	}
}

uintptr_t rcp_os_irq_save(void)
{
	if (xPortIsInsideInterrupt()) {
		return (uintptr_t)taskENTER_CRITICAL_FROM_ISR();
	}

	taskENTER_CRITICAL();
	return 0;
}

void rcp_os_irq_restore(uintptr_t flags)
{
	if (xPortIsInsideInterrupt()) {
		taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)flags);
		return;
	}

	taskEXIT_CRITICAL();
}
