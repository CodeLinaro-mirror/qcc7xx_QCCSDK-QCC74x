#ifndef __RCP_PERF_H__
#define __RCP_PERF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* High-resolution monotonic counters provided by the platform port.
 * Implementations live in rcp_protocol/port/perf/<platform>/. The
 * counters MUST be monotonic (modulo 32-bit wrap) and MUST advance
 * regardless of RTOS scheduling state; do not back them with kernel
 * tick counters. */

uint32_t rcp_perf_now_us(void);
uint32_t rcp_perf_now_ms(void);

/* Convert raw "cycle" units returned by the underlying counter into
 * microseconds. Exposed to keep the existing rcp_stat dump code that
 * stores cycle deltas working without each call site knowing the
 * conversion factor. Returns 0 if the platform cannot supply a
 * conversion. */
uint64_t rcp_perf_cycles_to_us(uint64_t cycles);
uint64_t rcp_perf_cycles_to_ms(uint64_t cycles);

/* Raw cycle counter value. Mostly useful for stat profiling that wants
 * to store deltas with full hardware resolution. */
uint32_t rcp_perf_cycles(void);

/* Frequency of the cycle counter in Hz (0 if unknown). */
uint32_t rcp_perf_cycle_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* __RCP_PERF_H__ */
