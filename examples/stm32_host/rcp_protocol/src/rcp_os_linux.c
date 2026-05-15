#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rcp_os.h"

static void timeout_to_abs_timespec(uint32_t timeout_ms, struct timespec *ts)
{
	clock_gettime(CLOCK_REALTIME, ts);
	ts->tv_sec += timeout_ms / 1000U;
	ts->tv_nsec += (long)(timeout_ms % 1000U) * 1000000L;
	if (ts->tv_nsec >= 1000000000L) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000L;
	}
}

static void *rcp_os_timer_thread(void *arg)
{
	rcp_timer_t *timer = (rcp_timer_t *)arg;

	pthread_mutex_lock(&(timer->_lock));
	while (!timer->_stop) {
		struct timespec ts;
		int ret;

		while (!timer->_armed && !timer->_stop) {
			pthread_cond_wait(&(timer->_cond), &(timer->_lock));
		}
		if (timer->_stop) {
			break;
		}

		timeout_to_abs_timespec(timer->_timeout_ms, &ts);
		ret = pthread_cond_timedwait(&(timer->_cond), &(timer->_lock), &ts);
		if (timer->_stop) {
			break;
		}

		if (ret == ETIMEDOUT && timer->_armed) {
			rcp_timer_callback_t cb = timer->_cb;
			void *cb_arg = timer->_arg;

			timer->_armed = false;
			pthread_mutex_unlock(&(timer->_lock));
			if (cb != NULL) {
				cb(cb_arg);
			}
			pthread_mutex_lock(&(timer->_lock));
		}
	}
	pthread_mutex_unlock(&(timer->_lock));

	return NULL;
}

static void *rcp_os_thread_trampoline(void *arg)
{
	rcp_thread_t *thread = (rcp_thread_t *)arg;

	if (thread != NULL && thread->_entry != NULL) {
		thread->_entry(thread->_arg);
	}

	return NULL;
}

bool rcp_os_sem_init(rcp_sem_t *sem, uint32_t cnt, uint32_t limit)
{
	(void)limit;
	sem_init(&(sem->_sem), 0, cnt);
	return true;
}

void rcp_os_sem_deinit(rcp_sem_t *sem)
{
	if (sem == NULL) {
		return;
	}

	(void)sem_destroy(&(sem->_sem));
}

bool rcp_os_sem_reset(rcp_sem_t *sem)
{
	if (sem == NULL) {
		return false;
	}

	while (sem_trywait(&(sem->_sem)) == 0) {
	}

	return (errno == EAGAIN || errno == 0);
}

bool rcp_os_sem_give(rcp_sem_t *sem)
{
	sem_post(&(sem->_sem));
	return true;
}

bool rcp_os_sem_take(rcp_sem_t *sem, uint32_t timeout)
{
	(void)timeout;
	return (sem_wait(&(sem->_sem)) == 0);
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
	return (pthread_mutex_init(&(mutex->_mutex), NULL) == 0);
}

void rcp_os_mutex_deinit(rcp_mutex_t *mutex)
{
	if (mutex == NULL) {
		return;
	}

	(void)pthread_mutex_destroy(&(mutex->_mutex));
}

bool rcp_os_mutex_lock(rcp_mutex_t *mutex, uint32_t timeout)
{
	(void)timeout;
	return (pthread_mutex_lock(&(mutex->_mutex)) == 0);
}

bool rcp_os_mutex_unlock(rcp_mutex_t *mutex)
{
	return (pthread_mutex_unlock(&(mutex->_mutex)) == 0);
}

uint32_t rcp_os_time_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

void rcp_os_delay_ms(uint32_t time_ms)
{
	struct timespec ts = {
		.tv_sec = time_ms / 1000,
		.tv_nsec = (time_ms % 1000) * 1000000ULL,
	};

	nanosleep(&ts, NULL);
}

bool rcp_os_event_init(rcp_event_t *event)
{
	if (pthread_mutex_init(&(event->_lock), NULL) != 0) {
		return false;
	}
	if (pthread_cond_init(&(event->_cond), NULL) != 0) {
		pthread_mutex_destroy(&(event->_lock));
		return false;
	}
	event->_bits = 0;
	return true;
}

void rcp_os_event_deinit(rcp_event_t *event)
{
	pthread_cond_destroy(&(event->_cond));
	pthread_mutex_destroy(&(event->_lock));
}

uint32_t rcp_os_event_set_bits(rcp_event_t *event, uint32_t flags)
{
	uint32_t bits;

	pthread_mutex_lock(&(event->_lock));
	event->_bits |= flags;
	bits = event->_bits;
	pthread_cond_broadcast(&(event->_cond));
	pthread_mutex_unlock(&(event->_lock));

	return bits;
}

uint32_t rcp_os_event_clear_bits(rcp_event_t *event, uint32_t flags)
{
	uint32_t bits;

	pthread_mutex_lock(&(event->_lock));
	event->_bits &= ~flags;
	bits = event->_bits;
	pthread_mutex_unlock(&(event->_lock));

	return bits;
}

uint32_t rcp_os_event_get_bits(rcp_event_t *event)
{
	uint32_t bits;

	pthread_mutex_lock(&(event->_lock));
	bits = event->_bits;
	pthread_mutex_unlock(&(event->_lock));

	return bits;
}

bool rcp_os_event_wait_bits(rcp_event_t *event, uint32_t flags, bool wait_all,
			      uint32_t timeout_ms, uint32_t *matched_flags)
{
	struct timespec ts;
	int ret = 0;

	if (event == NULL || flags == 0) {
		if (matched_flags) {
			*matched_flags = 0;
		}
		return false;
	}

	pthread_mutex_lock(&(event->_lock));

	while (true) {
		uint32_t active = event->_bits & flags;
		bool ready = wait_all ? (active == flags) : (active != 0);

		if (ready) {
			if (matched_flags) {
				*matched_flags = active;
			}
			pthread_mutex_unlock(&(event->_lock));
			return true;
		}

		if (timeout_ms == 0) {
			if (matched_flags) {
				*matched_flags = 0;
			}
			pthread_mutex_unlock(&(event->_lock));
			return false;
		}

		if (timeout_ms == RCP_OS_WAIT_FOREVER) {
			ret = pthread_cond_wait(&(event->_cond), &(event->_lock));
		} else {
			timeout_to_abs_timespec(timeout_ms, &ts);
			ret = pthread_cond_timedwait(&(event->_cond), &(event->_lock), &ts);
		}

		if (ret == ETIMEDOUT) {
			if (matched_flags) {
				*matched_flags = 0;
			}
			pthread_mutex_unlock(&(event->_lock));
			return false;
		}
	}
}

bool rcp_os_thread_create(rcp_thread_t *thread, rcp_thread_entry_t entry, void *arg,
			   const char *name, uint32_t stack_size, uint32_t priority)
{
	(void)name;
	(void)stack_size;
	(void)priority;

	if (thread == NULL || entry == NULL) {
		return false;
	}

	thread->_entry = entry;
	thread->_arg = arg;

	return (pthread_create(&thread->_thread, NULL, rcp_os_thread_trampoline, thread) == 0);
}

void rcp_os_thread_delete(rcp_thread_t *thread)
{
	if (thread == NULL) {
		return;
	}

	(void)pthread_cancel(thread->_thread);
	(void)pthread_join(thread->_thread, NULL);
	memset(&(thread->_thread), 0, sizeof(thread->_thread));
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
	timer->_armed = false;
	timer->_stop = false;
	timer->_timeout_ms = 0;

	if (pthread_mutex_init(&(timer->_lock), NULL) != 0) {
		return false;
	}
	if (pthread_cond_init(&(timer->_cond), NULL) != 0) {
		pthread_mutex_destroy(&(timer->_lock));
		return false;
	}
	if (pthread_create(&(timer->_thread), NULL, rcp_os_timer_thread, timer) != 0) {
		pthread_cond_destroy(&(timer->_cond));
		pthread_mutex_destroy(&(timer->_lock));
		return false;
	}

	return true;
}

bool rcp_os_timer_start(rcp_timer_t *timer, uint32_t timeout_ms)
{
	if (timer == NULL || timer->_cb == NULL) {
		return false;
	}

	pthread_mutex_lock(&(timer->_lock));
	timer->_timeout_ms = timeout_ms;
	timer->_armed = true;
	pthread_cond_signal(&(timer->_cond));
	pthread_mutex_unlock(&(timer->_lock));

	return true;
}

bool rcp_os_timer_stop(rcp_timer_t *timer)
{
	if (timer == NULL) {
		return false;
	}

	pthread_mutex_lock(&(timer->_lock));
	timer->_armed = false;
	pthread_cond_signal(&(timer->_cond));
	pthread_mutex_unlock(&(timer->_lock));

	return true;
}

void rcp_os_timer_deinit(rcp_timer_t *timer)
{
	if (timer == NULL) {
		return;
	}

	pthread_mutex_lock(&(timer->_lock));
	timer->_stop = true;
	timer->_armed = false;
	pthread_cond_signal(&(timer->_cond));
	pthread_mutex_unlock(&(timer->_lock));

	(void)pthread_join(timer->_thread, NULL);
	pthread_cond_destroy(&(timer->_cond));
	pthread_mutex_destroy(&(timer->_lock));
	timer->_cb = NULL;
	timer->_arg = NULL;
}
