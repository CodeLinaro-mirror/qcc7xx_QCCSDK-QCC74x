#ifndef __RCP_STAT_H__
#define __RCP_STAT_H__

#include <stdint.h>
#include <string.h>
#include "rcp_bus.h"
#include "rcp_config.h"

#if defined(RCP_TX_PROFILE_ENABLE) && (RCP_TX_PROFILE_ENABLE != 0)
#include "rcp_perf.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Global enable / disable
 *
 * Compile-time switch. When set to 0, recover/status counters below collapse
 * to no-ops and rcp_stat_dump_all() skips RCP/SDIO status output.
 *****************************************************************************/
#ifndef RCP_STAT_ENABLE
#define RCP_STAT_ENABLE 1
#endif

#if RCP_STAT_ENABLE

/*****************************************************************************
 * Recover timing
 *
 * Timeline (only the bits AFTER the device wakes up are measured;
 * device-sleep window is intentionally excluded):
 *
 *   T0 t_card_detected_us   : sdio_detect first returned true (card
 *                             back on the bus)
 *   T1 t_reinit_done_us     : SDIO reinit + card identification done
 *                             (rcp_bus_runtime_recover returned ok)
 *   T2 t_handshake_kicked_us: drain_handshake_pending entry
 *   T2b t_ctrl_drained_us   : ctrl tx scheduler emptied (host side
 *                             pushed every HOST_RESET / HOST_READY out)
 *   T2c t_first_dev_ready_us: first DEVICE_READY received
 *   T2d t_first_dev_start_us: first DEVICE_START received
 *   per-stream RUN moments
 *   T3 t_all_run_us         : every active stream reached DEVICE_RUN
 *
 *   reinit cost  = T1 - T0
 *   handshake    = T3 - T2
 *   total recover= T3 - T0
 *****************************************************************************/

typedef struct {
	uint32_t t_card_detected_us;
	uint32_t t_hal_init_done_us;        /* MX_SDMMC1_SDIO_Init returned */
	uint32_t t_card_identified_us;      /* sdio_hal_init_card returned */
	uint32_t t_reinit_done_us;
	uint32_t t_handshake_kicked_us;
	uint32_t t_ctrl_drained_us;
	uint32_t t_first_dev_ready_us;
	uint32_t t_first_dev_start_us;
	uint32_t t_stream_run_us[RCP_MSG_TYPE_MAX];
	uint32_t t_all_run_us;
	uint8_t  active_stream_count;
	uint8_t  run_stream_count;
} rcp_stat_recover_timing_t;

extern rcp_stat_recover_timing_t g_rcp_stat_recover;

/* Returns the current monotonic timestamp in microseconds. Implemented
 * in rcp_stat.c using DWT->CYCCNT and SystemCoreClock. */
uint32_t rcp_stat_now_us(void);

#define RCP_STAT_RECOVER_RESET()                                              \
	do {                                                                   \
		memset(&g_rcp_stat_recover, 0, sizeof(g_rcp_stat_recover));    \
	} while (0)

#define RCP_STAT_RECOVER_STAMP(field)                                         \
	do {                                                                   \
		(g_rcp_stat_recover.field) = rcp_stat_now_us();                \
	} while (0)

#define RCP_STAT_RECOVER_STAMP_ONCE(field)                                    \
	do {                                                                   \
		if (g_rcp_stat_recover.field == 0U) {                          \
			g_rcp_stat_recover.field = rcp_stat_now_us();          \
		}                                                              \
	} while (0)

#define RCP_STAT_RECOVER_INC(field)                                           \
	do {                                                                   \
		(g_rcp_stat_recover.field)++;                                  \
	} while (0)

#define RCP_STAT_RECOVER_STAMP_STREAM_RUN(stream)                             \
	do {                                                                   \
		if ((stream) < RCP_MSG_TYPE_MAX                                \
		    && g_rcp_stat_recover.t_card_detected_us != 0U             \
		    && g_rcp_stat_recover.t_stream_run_us[(stream)] == 0U) {   \
			g_rcp_stat_recover.t_stream_run_us[(stream)] =         \
				rcp_stat_now_us();                             \
			g_rcp_stat_recover.run_stream_count++;                 \
			if (g_rcp_stat_recover.run_stream_count >=             \
			    g_rcp_stat_recover.active_stream_count) {          \
				g_rcp_stat_recover.t_all_run_us =              \
					rcp_stat_now_us();                     \
			}                                                      \
		}                                                              \
	} while (0)

void rcp_stat_recover_dump(void);

#else /* RCP_STAT_ENABLE == 0 */

#define RCP_STAT_RECOVER_RESET()                  do { } while (0)
#define RCP_STAT_RECOVER_STAMP(field)             do { } while (0)
#define RCP_STAT_RECOVER_STAMP_ONCE(field)        do { } while (0)
#define RCP_STAT_RECOVER_INC(field)               do { } while (0)
#define RCP_STAT_RECOVER_STAMP_STREAM_RUN(s)      do { } while (0)
static inline uint32_t rcp_stat_now_us(void) { return 0U; }
static inline void     rcp_stat_recover_dump(void) {}

#endif /* RCP_STAT_ENABLE */

/*****************************************************************************
 * One-shot aggregate dump.
 *
 * Calls in order:
 *   rcp_dump_status()             link-state / scheduler / per-stream stats
 *   rcp_stat_recover_dump()       recover timing breakdown (this module)
 *   rcp_bus_sdio_master_dump()    SDIO bus + SDMMC registers
 *   RCP_TX_PROF_DUMP()            TX profiling counters
 *
 * Use this from shell or panic paths to grab everything at once.
 *****************************************************************************/
void rcp_stat_dump_all(void);

#if RCP_HOST_STATS_ENABLE
#define RCP_STAT_DUMP_ALL_BODY()                                             \
	do {                                                                  \
		rcp_dump_status();                                            \
		rcp_stat_recover_dump();                                      \
		rcp_bus_sdio_master_dump();                                   \
		RCP_TX_PROF_DUMP();                                           \
	} while (0)
#else
#define RCP_STAT_DUMP_ALL_BODY()                                             \
	do {                                                                  \
	} while (0)
#endif

/*****************************************************************************
 * TX profile
 *
 * Kept in this module so all runtime counters have one owner. When
 * RCP_TX_PROFILE_ENABLE is 0, these macros compile out of hot paths.
 *****************************************************************************/

enum rcp_tx_prof_stage {
	RCP_TX_PROF_PROD_ALLOC = 0,
	RCP_TX_PROF_PROD_LINK_SEND,
	RCP_TX_PROF_NETIF_ALLOC,
	RCP_TX_PROF_NETIF_COPY,
	RCP_TX_PROF_NETIF_LINK_SEND,
	RCP_TX_PROF_CREDIT_WAIT,
	RCP_TX_PROF_QUEUE_LATENCY,
	RCP_TX_PROF_PROC_TOTAL,
	RCP_TX_PROF_SDIO_DMA_WAIT,
	RCP_TX_PROF_SDIO_WAIT_TO_IRQ,
	RCP_TX_PROF_SDIO_IRQ_TO_RESUME,
	RCP_TX_PROF_MAX
};

#if defined(RCP_TX_PROFILE_ENABLE) && (RCP_TX_PROFILE_ENABLE != 0)

typedef struct {
	uint64_t total_cycles;
	uint32_t count;
	uint32_t max_cycles;
} rcp_tx_prof_counter_t;

typedef struct {
	rcp_tx_prof_counter_t stage[RCP_TX_PROF_MAX];
	uint32_t wakeups;
	uint32_t packets_per_wakeup_sum;
	uint32_t max_packets_per_wakeup;
} rcp_tx_prof_stats_t;

extern rcp_tx_prof_stats_t g_rcp_tx_prof;

void rcp_tx_prof_dump_stats(void);

#define RCP_TX_PROF_RESET() \
	memset(&(g_rcp_tx_prof), 0, sizeof(g_rcp_tx_prof))

#define RCP_TX_PROF_DUMP() \
	rcp_tx_prof_dump_stats()

#define RCP_TX_PROF_TS() \
	(rcp_perf_cycles())

#define RCP_TX_PROF_BEGIN(var) \
	uint32_t var = RCP_TX_PROF_TS()

#define RCP_TX_PROF_MARK(field) \
	do { (field) = RCP_TX_PROF_TS(); } while (0)

#define RCP_TX_PROF_CLEAR(field) \
	do { (field) = 0U; } while (0)

#define RCP_TX_PROF_U32_FIELD(name) \
	volatile uint32_t name

#define RCP_TX_PROF_DELTA_MAX_CYCLES \
	(UINT32_MAX / 2U)

static inline uint32_t rcp_tx_prof_cycles_delta(uint32_t start_cycles,
						uint32_t end_cycles)
{
	return end_cycles - start_cycles;
}

static inline int rcp_tx_prof_cycles_delta_valid(uint32_t start_cycles,
						 uint32_t end_cycles,
						 uint32_t *delta_cycles)
{
	uint32_t delta = rcp_tx_prof_cycles_delta(start_cycles, end_cycles);

	if (delta > RCP_TX_PROF_DELTA_MAX_CYCLES) {
		return 0;
	}

	*delta_cycles = delta;
	return 1;
}

#define RCP_TX_PROF_ADD(stage_id, cycles)                                                   \
	do {                                                                                 \
		uint32_t __cycles = (cycles);                                                \
		rcp_tx_prof_counter_t *__stat = &g_rcp_tx_prof.stage[(stage_id)];            \
		__stat->total_cycles += __cycles;                                            \
		__stat->count++;                                                             \
		if (__cycles > __stat->max_cycles) {                                         \
			__stat->max_cycles = __cycles;                                       \
		}                                                                            \
	} while (0)

#define RCP_TX_PROF_ACC(stage_id, start_cycles)                                             \
	do {                                                                                 \
		RCP_TX_PROF_ADD((stage_id),                                             \
				rcp_tx_prof_cycles_delta((start_cycles),                 \
							 RCP_TX_PROF_TS()));                 \
	} while (0)

#define RCP_TX_PROF_ACC_IRQ_RESUME(irq_cycles, start_cycles)                                \
	do {                                                                                 \
		uint32_t __irq_cycles = (irq_cycles);                                        \
		if (__irq_cycles != 0U) {                                                    \
			uint32_t __resume_cycles = RCP_TX_PROF_TS();                         \
			uint32_t __delta_cycles;                                            \
			if (rcp_tx_prof_cycles_delta_valid((start_cycles), __irq_cycles,    \
							   &__delta_cycles)) {                 \
				RCP_TX_PROF_ADD(RCP_TX_PROF_SDIO_WAIT_TO_IRQ,                \
						__delta_cycles);                             \
			}                                                                    \
			if (rcp_tx_prof_cycles_delta_valid(__irq_cycles, __resume_cycles,    \
							   &__delta_cycles)) {                 \
				RCP_TX_PROF_ADD(RCP_TX_PROF_SDIO_IRQ_TO_RESUME,              \
						__delta_cycles);                             \
			}                                                                    \
		}                                                                            \
	} while (0)

#define RCP_TX_PROF_QUEUE_STAMP() \
	RCP_TX_PROF_TS()

#define RCP_TX_PROF_NOTE_WAKEUP(packet_cnt)                                                 \
	do {                                                                                 \
		uint32_t __packets = (packet_cnt);                                           \
		if (__packets != 0U) {                                                       \
			g_rcp_tx_prof.wakeups++;                                             \
			g_rcp_tx_prof.packets_per_wakeup_sum += __packets;                   \
			if (__packets > g_rcp_tx_prof.max_packets_per_wakeup) {              \
				g_rcp_tx_prof.max_packets_per_wakeup = __packets;             \
			}                                                                    \
		}                                                                            \
	} while (0)

#else

#define RCP_TX_PROF_RESET() \
	do { } while (0)

#define RCP_TX_PROF_DUMP() \
	do { } while (0)

#define RCP_TX_PROF_TS() \
	(0U)

#define RCP_TX_PROF_BEGIN(var) \
	const uint32_t var = 0U

#define RCP_TX_PROF_MARK(field) \
	do { } while (0)

#define RCP_TX_PROF_CLEAR(field) \
	do { } while (0)

#define RCP_TX_PROF_U32_FIELD(name)

#define RCP_TX_PROF_ACC(stage_id, start_cycles) \
	do { (void)(stage_id); (void)(start_cycles); } while (0)

#define RCP_TX_PROF_ADD(stage_id, cycles) \
	do { (void)(stage_id); (void)(cycles); } while (0)

#define RCP_TX_PROF_ACC_IRQ_RESUME(irq_cycles, start_cycles) \
	do { (void)(start_cycles); } while (0)

#define RCP_TX_PROF_QUEUE_STAMP() \
	(0U)

#define RCP_TX_PROF_NOTE_WAKEUP(packet_cnt) \
	do { (void)(packet_cnt); } while (0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* __RCP_STAT_H__ */
