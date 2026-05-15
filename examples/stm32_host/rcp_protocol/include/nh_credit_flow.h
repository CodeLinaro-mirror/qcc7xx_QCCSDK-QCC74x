#ifndef __NH_CREDIT_FLOW_H__
#define __NH_CREDIT_FLOW_H__

#include <stdbool.h>
#include <stdint.h>

#include "rcp_os.h"

typedef enum {
    NH_CREDIT_DIR_TX = 0,
    NH_CREDIT_DIR_RX = 1,
    NH_CREDIT_DIR_MAX
} nh_credit_dir_t;

#define NH_CREDIT_EN_TX  (1U << NH_CREDIT_DIR_TX)
#define NH_CREDIT_EN_RX  (1U << NH_CREDIT_DIR_RX)
#define NH_CREDIT_EN_ALL (NH_CREDIT_EN_TX | NH_CREDIT_EN_RX)

typedef struct {
    uint8_t credit_limit_cnt;
    uint8_t credit_consumed_cnt;
    uint8_t init_credit;
    uint8_t threshold;
    rcp_sem_t *wait_sem;
} nh_credit_t;

typedef struct {
    nh_credit_t credit[NH_CREDIT_DIR_MAX];
    uint8_t enable;
    const char *tag;
} nh_flow_cntrl_t;

static inline void nh_credit_set_enable(nh_flow_cntrl_t *fc, nh_credit_dir_t dir, bool enabled)
{
    uint8_t mask;

    if (!fc || dir >= NH_CREDIT_DIR_MAX) {
        return;
    }

    mask = (uint8_t)(1U << dir);
    if (enabled) {
        fc->enable |= mask;
    } else {
        fc->enable &= (uint8_t)~mask;
    }
}

static inline bool nh_credit_get_enable(const nh_flow_cntrl_t *fc, nh_credit_dir_t dir)
{
    if (!fc || dir >= NH_CREDIT_DIR_MAX) {
        return false;
    }

    return (fc->enable & (uint8_t)(1U << dir)) != 0U;
}

static inline int nh_credit_enable(nh_flow_cntrl_t *fc, uint8_t enable)
{
    if (fc == NULL) {
        return -1;
    }

    fc->enable = enable;
    return 0;
}

int nh_tx_credit_get(nh_flow_cntrl_t *fc, uint32_t timeout_ms);
void nh_tx_credit_put(nh_flow_cntrl_t *fc);
bool nh_tx_credit_update(nh_flow_cntrl_t *fc, uint8_t new_limit);

void nh_rx_credit_reclaim(nh_flow_cntrl_t *fc);
bool nh_rx_credit_need_update(nh_flow_cntrl_t *fc);
int nh_rx_credit_update(nh_flow_cntrl_t *fc);

int nh_credit_init(nh_flow_cntrl_t *fc, const char *name,
        uint8_t max_txcredit, uint8_t tx_threshold,
        uint8_t max_rxcredit, uint8_t rx_threshold);

void nh_credit_reset(nh_flow_cntrl_t *fc);
void nh_credit_deinit(nh_flow_cntrl_t *fc);

#endif
