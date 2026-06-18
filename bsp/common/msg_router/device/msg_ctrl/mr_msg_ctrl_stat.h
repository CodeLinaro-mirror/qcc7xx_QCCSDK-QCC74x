#ifndef __MR_MSG_CTRL_STAT_H__
#define __MR_MSG_CTRL_STAT_H__

#include <stdint.h>
#include <string.h>

#include "mr_msg_ctrl.h"

typedef struct {
    mr_msg_t msg_pkt;
    uint8_t reserved[1];
    uint8_t flag;
    uint8_t credit_update_flag;
    uint8_t credit_limit_cnt;
} mr_msg_stat_pkt_t;

#ifndef CONFIG_MR_MSG_STREAM_STATS
#define CONFIG_MR_MSG_STREAM_STATS (1)
#endif

#ifndef MR_DNLD_PROFILE_ENABLE
#define MR_DNLD_PROFILE_ENABLE 1
#endif

#define MR_MSG_FLAG_IS_DATA(flag)      (((flag) == 1U) || ((flag) == 2U))
#define MR_MSG_FLAG_IS_HANDSHAKE(flag) (((flag) >= 3U) && ((flag) <= 8U))
#define MR_MSG_PROTO_META_SIZE         ((uint32_t)(sizeof(mr_msg_stat_pkt_t) - sizeof(mr_msg_t)))
#define MR_MSG_PAYLOAD_SIZE(msg_pkt) \
    ((uint32_t)(((msg_pkt)->len >= MR_MSG_PROTO_META_SIZE) ? ((msg_pkt)->len - MR_MSG_PROTO_META_SIZE) : 0U))

typedef enum {
    MR_DNLD_PROF_SDIO_TO_MSG = 0,
    MR_DNLD_PROF_MSG_CB,
    MR_DNLD_PROF_MSG_TO_NETDEV,
    MR_DNLD_PROF_NETDEV_OUTPUT,
    MR_DNLD_PROF_NETHUB_SEND,
    MR_DNLD_PROF_FHOST_TX,
    MR_DNLD_PROF_MAX,
} mr_dnld_prof_stage_t;

#if CONFIG_MR_MSG_STREAM_STATS
#define MR_MSG_STREAM_STAT_UPDATE(_msg_ctrl, _frame_elem, _dir)                   \
    do {                                                                          \
        mr_msg_ctrl_priv_t *_stat_msg_ctrl = (_msg_ctrl);                         \
        mr_frame_elem_t *_stat_frame_elem = (_frame_elem);                        \
        mr_msg_t *_stat_msg_pkt;                                                  \
        mr_msg_stream_stat_t *_stat;                                              \
        uint8_t _stat_flag = 0;                                                   \
        uint32_t _stat_payload_size;                                              \
                                                                                  \
        if (_stat_msg_ctrl == NULL || _stat_frame_elem == NULL ||                 \
            _stat_frame_elem->buff_addr == NULL) {                                \
            break;                                                                \
        }                                                                         \
                                                                                  \
        _stat_msg_pkt = (mr_msg_t *)_stat_frame_elem->buff_addr;                  \
        if (_stat_msg_pkt->tag >= MR_MSG_TAG_MAX) {                               \
            break;                                                                \
        }                                                                         \
                                                                                  \
        if (_stat_frame_elem->data_size >= sizeof(mr_msg_stat_pkt_t)) {           \
            _stat_flag = ((mr_msg_stat_pkt_t *)_stat_frame_elem->buff_addr)->flag; \
        }                                                                         \
                                                                                  \
        if (MR_MSG_FLAG_IS_HANDSHAKE(_stat_flag)) {                               \
            break;                                                                \
        }                                                                         \
                                                                                  \
        _stat = &_stat_msg_ctrl->stream_stats[_stat_msg_pkt->tag];                \
        _stat_payload_size = MR_MSG_PAYLOAD_SIZE(_stat_msg_pkt);                  \
        if (MR_MSG_FLAG_IS_DATA(_stat_flag)) {                                    \
            _stat->_dir##_data_packets++;                                         \
            _stat->_dir##_data_bytes += _stat_payload_size;                       \
        } else {                                                                  \
            _stat->_dir##_ctrl_packets++;                                         \
            _stat->_dir##_ctrl_bytes += _stat_payload_size;                       \
        }                                                                         \
    } while (0)
#define MR_MSG_STREAM_STAT_TX_UPDATE(_msg_ctrl, _frame_elem) \
    MR_MSG_STREAM_STAT_UPDATE(_msg_ctrl, _frame_elem, tx)
#define MR_MSG_STREAM_STAT_RX_UPDATE(_msg_ctrl, _frame_elem) \
    MR_MSG_STREAM_STAT_UPDATE(_msg_ctrl, _frame_elem, rx)

#define MR_MSG_CTRL_RESET_STREAM_STAT(_msg_ctrl, _tag)                         \
    do {                                                                       \
        mr_msg_ctrl_priv_t *_stat_msg_ctrl = (_msg_ctrl);                      \
        uint8_t _stat_tag = (_tag);                                            \
                                                                               \
        if (_stat_msg_ctrl == NULL || _stat_tag >= MR_MSG_TAG_MAX) {           \
            break;                                                             \
        }                                                                      \
                                                                               \
        memset((void *)&_stat_msg_ctrl->stream_stats[_stat_tag], 0,            \
               sizeof(_stat_msg_ctrl->stream_stats[_stat_tag]));               \
    } while (0)

#else
#define MR_MSG_STREAM_STAT_TX_UPDATE(_msg_ctrl, _frame_elem) \
    do {                                                     \
    } while (0)
#define MR_MSG_STREAM_STAT_RX_UPDATE(_msg_ctrl, _frame_elem) \
    do {                                                     \
    } while (0)
#define MR_MSG_CTRL_RESET_STREAM_STAT(_msg_ctrl, _tag) \
    do {                                               \
    } while (0)
#endif

#if MR_DNLD_PROFILE_ENABLE
typedef struct {
    uint64_t total_cycles;
    uint32_t count;
    uint32_t max_cycles;
} mr_dnld_prof_counter_t;

extern mr_dnld_prof_counter_t g_mr_dnld_prof[MR_DNLD_PROF_MAX];

static inline uint32_t mr_dnld_prof_cycles(void)
{
#if __riscv_xlen == 32
    uint32_t cycle;
    __asm volatile ("rdcycle %0" : "=r"(cycle));
    return cycle;
#else
    uint64_t cycle;
    __asm volatile ("rdcycle %0" : "=r"(cycle));
    return (uint32_t)cycle;
#endif
}

static inline void mr_dnld_prof_add(mr_dnld_prof_stage_t stage, uint32_t cycles)
{
    mr_dnld_prof_counter_t *counter = &g_mr_dnld_prof[stage];

    counter->total_cycles += cycles;
    counter->count++;
    if (cycles > counter->max_cycles) {
        counter->max_cycles = cycles;
    }
}

#define MR_DNLD_PROF_STAMP(_frame_elem, _idx) \
    (((uint32_t *)(void *)((_frame_elem)->ex_data))[_idx])
#define MR_DNLD_PROF_SET(_frame_elem, _idx) \
    do { MR_DNLD_PROF_STAMP((_frame_elem), (_idx)) = mr_dnld_prof_cycles(); } while (0)
#define MR_DNLD_PROF_FROM(_stage, _frame_elem, _idx)                              \
    do {                                                                          \
        uint32_t _prof_start = MR_DNLD_PROF_STAMP((_frame_elem), (_idx));         \
        if (_prof_start != 0U) {                                                  \
            mr_dnld_prof_add((_stage), mr_dnld_prof_cycles() - _prof_start);      \
        }                                                                         \
    } while (0)
#define MR_DNLD_PROF_BEGIN(_var) uint32_t _var = mr_dnld_prof_cycles()
#define MR_DNLD_PROF_END(_stage, _var) \
    mr_dnld_prof_add((_stage), mr_dnld_prof_cycles() - (_var))

void mr_dnld_profile_reset(void);
void mr_dnld_profile_dump(void);
#else
#define MR_DNLD_PROF_SET(_frame_elem, _idx) \
    do { (void)(_frame_elem); (void)(_idx); } while (0)
#define MR_DNLD_PROF_FROM(_stage, _frame_elem, _idx) \
    do { (void)(_stage); (void)(_frame_elem); (void)(_idx); } while (0)
#define MR_DNLD_PROF_BEGIN(_var) \
    do { } while (0)
#define MR_DNLD_PROF_END(_stage, _var) \
    do { (void)(_stage); } while (0)
static inline void mr_dnld_profile_reset(void) {}
static inline void mr_dnld_profile_dump(void) {}
#endif

#endif /* __MR_MSG_CTRL_STAT_H__ */
