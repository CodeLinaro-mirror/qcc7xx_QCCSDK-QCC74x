#include <zephyr/kernel.h>
#include "rcp_config.h"
#include "rcp_os.h"

static k_timeout_t timeout_to_k_timeout(uint32_t timeout_ms)
{
	if (timeout_ms == 0) {
		return K_NO_WAIT;
	}
	if (timeout_ms == RCP_OS_WAIT_FOREVER) {
		return K_FOREVER;
	}
	return K_MSEC(timeout_ms);
}

static void rcp_os_timer_expiry(struct k_work *work)
{
	rcp_timer_t *timer = CONTAINER_OF(work, rcp_timer_t, _timer.work);

	if (timer != NULL && timer->_cb != NULL) {
		timer->_cb(timer->_arg);
	}
}

#if RCP_LOG == 1
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(qualcomm_rcp, RCP_LOG_LEVEL);

#endif /* RCP_LOG */

bool rcp_os_sem_init(rcp_sem_t *sem, uint32_t cnt, uint32_t limit)
{
	if (0 != k_sem_init(&(sem->_sem), cnt, limit)) {
		return false;
	}
	return true;
}

void rcp_os_sem_deinit(rcp_sem_t *sem)
{
	ARG_UNUSED(sem);
}


bool rcp_os_sem_reset(rcp_sem_t *sem)
{
	if (sem == NULL) {
		return false;
	}

	k_sem_reset(&(sem->_sem));
	return true;
}

bool rcp_os_sem_give(rcp_sem_t *sem)
{
	k_sem_give(&(sem->_sem));
	return true;
}

bool rcp_os_sem_take(rcp_sem_t *sem, uint32_t timeout)
{
	if (0 != k_sem_take(&(sem->_sem), timeout_to_k_timeout(timeout))) {
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
	k_mutex_init(&(mutex->_mutex));
	return true;
}

void rcp_os_mutex_deinit(rcp_mutex_t *mutex)
{
	ARG_UNUSED(mutex);
}

bool rcp_os_mutex_lock(rcp_mutex_t *mutex, uint32_t timeout)
{
	return (k_mutex_lock(&(mutex->_mutex), timeout_to_k_timeout(timeout)) == 0);
}

bool rcp_os_mutex_unlock(rcp_mutex_t *mutex)
{
	k_mutex_unlock(&(mutex->_mutex));
	return true;
}

uint32_t rcp_os_time_ms(void)
{
	return k_uptime_get_32();
}

void rcp_os_delay_ms(uint32_t time_ms)
{
	k_sleep(K_MSEC(time_ms));
}

bool rcp_os_event_init(rcp_event_t *event)
{
	k_event_init(&(event->_event));
	return true;
}

void rcp_os_event_deinit(rcp_event_t *event)
{
	ARG_UNUSED(event);
}

uint32_t rcp_os_event_set_bits(rcp_event_t *event, uint32_t flags)
{
	return k_event_post(&(event->_event), flags);
}

uint32_t rcp_os_event_clear_bits(rcp_event_t *event, uint32_t flags)
{
	return k_event_clear(&(event->_event), flags);
}

uint32_t rcp_os_event_get_bits(rcp_event_t *event)
{
	return k_event_wait(&(event->_event), UINT32_MAX, false, K_NO_WAIT);
}

bool rcp_os_event_wait_bits(rcp_event_t *event, uint32_t flags, bool wait_all,
			      uint32_t timeout_ms, uint32_t *matched_flags)
{
	uint32_t bits;

	if (event == NULL || flags == 0) {
		if (matched_flags) {
			*matched_flags = 0;
		}
		return false;
	}

	bits = wait_all ?
		k_event_wait_all(&(event->_event), flags, false, timeout_to_k_timeout(timeout_ms)) :
		k_event_wait(&(event->_event), flags, false, timeout_to_k_timeout(timeout_ms));

	if (matched_flags) {
		*matched_flags = bits & flags;
	}

	return wait_all ? ((bits & flags) == flags) : ((bits & flags) != 0);
}

bool rcp_os_thread_create(rcp_thread_t *thread, rcp_thread_entry_t entry, void *arg,
			   const char *name, uint32_t stack_size, uint32_t priority)
{
	ARG_UNUSED(thread);
	ARG_UNUSED(entry);
	ARG_UNUSED(arg);
	ARG_UNUSED(name);
	ARG_UNUSED(stack_size);
	ARG_UNUSED(priority);
	return false;
}

void rcp_os_thread_delete(rcp_thread_t *thread)
{
	if (thread == NULL || thread->_thread == NULL) {
		return;
	}

	k_thread_abort((k_tid_t)thread->_thread);
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
	k_work_init_delayable(&(timer->_timer), rcp_os_timer_expiry);
	return true;
}

bool rcp_os_timer_start(rcp_timer_t *timer, uint32_t timeout_ms)
{
	if (timer == NULL || timer->_cb == NULL) {
		return false;
	}

	return (k_work_reschedule(&(timer->_timer), timeout_to_k_timeout(timeout_ms)) >= 0);
}

bool rcp_os_timer_stop(rcp_timer_t *timer)
{
	if (timer == NULL) {
		return false;
	}

	return k_work_cancel_delayable(&(timer->_timer)) >= 0;
}

void rcp_os_timer_deinit(rcp_timer_t *timer)
{
	if (timer == NULL) {
		return;
	}

	(void)k_work_cancel_delayable(&(timer->_timer));
	timer->_cb = NULL;
	timer->_arg = NULL;
}

bool rcp_os_in_isr(void)
{
	return k_is_in_isr();
}

void *rcp_os_malloc(size_t size)
{
	return k_malloc(size);
}

void rcp_os_free(void *ptr)
{
	k_free(ptr);
}

void rcp_os_disable_irq(void)
{
}

void rcp_os_enable_irq(void)
{
}

uintptr_t rcp_os_irq_save(void)
{
	return irq_lock();
}

void rcp_os_irq_restore(uintptr_t flags)
{
	irq_unlock((unsigned int)flags);
}
