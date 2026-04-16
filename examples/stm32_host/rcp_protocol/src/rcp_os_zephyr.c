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

bool rcp_os_mutex_init(rcp_mutex_t *mutex)
{
	k_mutex_init(&(mutex->_mutex));
	return true;
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
