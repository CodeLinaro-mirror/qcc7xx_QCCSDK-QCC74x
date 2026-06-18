#ifndef __MR_CREDIT_FLOW_H__
#define __MR_CREDIT_FLOW_H__

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MR_CREDIT_DIR_TX = 0,
    MR_CREDIT_DIR_RX = 1,
    MR_CREDIT_DIR_MAX
} mr_credit_dir_t;

#define MR_CREDIT_EN_TX  (1U << MR_CREDIT_DIR_TX)
#define MR_CREDIT_EN_RX  (1U << MR_CREDIT_DIR_RX)
#define MR_CREDIT_EN_ALL (MR_CREDIT_EN_TX | MR_CREDIT_EN_RX)

typedef struct {
    uint8_t credit_limit_cnt;
    uint8_t credit_consumed_cnt;
    uint8_t init_credit;
    uint8_t threshold;
    void *wait_sem;
} mr_credit_t;

typedef struct {
    mr_credit_t credit[MR_CREDIT_DIR_MAX];
    uint8_t enable;
    const char *tag;
} mr_flow_ctrl_t;

static inline void mr_credit_set_enable(mr_flow_ctrl_t *fc, mr_credit_dir_t dir, bool enabled)
{
    uint8_t mask;

    if (!fc || dir >= MR_CREDIT_DIR_MAX) {
        return;
    }

    mask = (uint8_t)(1U << dir);
    if (enabled) {
        fc->enable |= mask;
    } else {
        fc->enable &= (uint8_t)~mask;
    }
}

static inline bool mr_credit_get_enable(const mr_flow_ctrl_t *fc, mr_credit_dir_t dir)
{
    if (!fc || dir >= MR_CREDIT_DIR_MAX) {
        return false;
    }

    return (fc->enable & (uint8_t)(1U << dir)) != 0U;
}

static inline int mr_credit_enable(mr_flow_ctrl_t *fc, uint8_t enable)
{
    if (fc == NULL) {
        return -1;
    }

    fc->enable = enable;
    return 0;
}

int mr_tx_credit_get(mr_flow_ctrl_t *fc, uint32_t timeout_ms);
void mr_tx_credit_put(mr_flow_ctrl_t *fc);
bool mr_tx_credit_update(mr_flow_ctrl_t *fc, uint8_t new_limit);

void mr_rx_credit_reclaim(mr_flow_ctrl_t *fc);
bool mr_rx_credit_need_update(mr_flow_ctrl_t *fc);
int mr_rx_credit_prepare(mr_flow_ctrl_t *fc);
int mr_rx_credit_update(mr_flow_ctrl_t *fc);

int mr_credit_init(mr_flow_ctrl_t *fc, const char *name,
                   uint8_t max_txcredit, uint8_t tx_threshold,
                   uint8_t max_rxcredit, uint8_t rx_threshold);

void mr_credit_reset(mr_flow_ctrl_t *fc);
void mr_credit_deinit(mr_flow_ctrl_t *fc);

#endif
