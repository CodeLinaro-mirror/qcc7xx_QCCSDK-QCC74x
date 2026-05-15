#include <stdio.h>
#include <string.h>

#include "rcp_os.h"
#include "nh_credit_flow.h"

//#define NH_CREDIT_FLOW_LOG_ENABLE 1

#if NH_CREDIT_FLOW_LOG_ENABLE
#define NH_CREDIT_LOG(fc, fmt, ...) \
    printf("[credit][%s] " fmt, nh_credit_log_tag(fc), ##__VA_ARGS__)
#else
#define NH_CREDIT_LOG(fc, fmt, ...) do { } while (0)
#endif

static inline const char *nh_credit_log_tag(const nh_flow_cntrl_t *fc)
{
    return (fc != NULL && fc->tag != NULL) ? fc->tag : "?";
}

static inline uint32_t nh_credit_ms_to_ticks(uint32_t timeout_ms)
{
    return timeout_ms;
}


int nh_credit_init(nh_flow_cntrl_t *fc, const char *name,
        uint8_t max_txcredit, uint8_t tx_threshold,
        uint8_t max_rxcredit, uint8_t rx_threshold)
{
    rcp_sem_t *tx_wait_sem;
    uint8_t tx_wait_sem_max;
    uint8_t tx_wait_sem_fill;

    if (fc == NULL) {
        return -1;
    }

    tx_wait_sem = fc->credit[NH_CREDIT_DIR_TX].wait_sem;
    tx_wait_sem_max = fc->credit[NH_CREDIT_DIR_TX].init_credit;

    if (tx_wait_sem != NULL) {
        if (max_txcredit == 0U || tx_wait_sem_max < max_txcredit) {
            rcp_os_sem_delete(tx_wait_sem);
            tx_wait_sem = NULL;
        } else {
            rcp_os_sem_reset(tx_wait_sem);
            for (tx_wait_sem_fill = 0; tx_wait_sem_fill < max_txcredit; tx_wait_sem_fill++) {
                (void)rcp_os_sem_give(tx_wait_sem);
            }
        }
    }

    memset(fc, 0, sizeof(*fc));

    fc->credit[NH_CREDIT_DIR_TX].init_credit = max_txcredit;
    fc->credit[NH_CREDIT_DIR_TX].threshold = tx_threshold;
    fc->credit[NH_CREDIT_DIR_TX].credit_limit_cnt = max_txcredit;

    fc->credit[NH_CREDIT_DIR_RX].init_credit = max_rxcredit;
    fc->credit[NH_CREDIT_DIR_RX].threshold = rx_threshold;
    fc->credit[NH_CREDIT_DIR_RX].credit_consumed_cnt = max_rxcredit;

    if (max_txcredit != 0U) {
        if (tx_wait_sem == NULL) {
            tx_wait_sem = rcp_os_sem_create(max_txcredit, max_txcredit);
        }
        fc->credit[NH_CREDIT_DIR_TX].wait_sem = tx_wait_sem;
        if (fc->credit[NH_CREDIT_DIR_TX].wait_sem == NULL) {
            return -1;
        }
    }
    fc->tag = name;

    //fc->enable = NH_CREDIT_EN_TX;

    NH_CREDIT_LOG(fc,
                  "init: tx_init=%u tx_th=%u rx_init=%u rx_th=%u\r\n",
                  max_txcredit, tx_threshold, max_rxcredit, rx_threshold);

    return 0;
}

void nh_credit_reset(nh_flow_cntrl_t *fc)
{
    uintptr_t irq_flags;

    if (fc == NULL) {
        return;
    }

    if (fc->credit[NH_CREDIT_DIR_TX].wait_sem != NULL) {
        rcp_os_sem_reset(fc->credit[NH_CREDIT_DIR_TX].wait_sem);
    }

    irq_flags = rcp_os_irq_save();
    fc->credit[NH_CREDIT_DIR_TX].credit_limit_cnt = 0;
    fc->credit[NH_CREDIT_DIR_TX].credit_consumed_cnt = 0;
    fc->credit[NH_CREDIT_DIR_RX].credit_limit_cnt = 0;
    fc->credit[NH_CREDIT_DIR_RX].credit_consumed_cnt = fc->credit[NH_CREDIT_DIR_RX].init_credit;
    rcp_os_irq_restore(irq_flags);

    NH_CREDIT_LOG(fc,
                  "reset: tx_limit=%u tx_consumed=%u rx_advertised=%u rx_available=%u\r\n",
                  fc->credit[NH_CREDIT_DIR_TX].credit_limit_cnt,
                  fc->credit[NH_CREDIT_DIR_TX].credit_consumed_cnt,
                  fc->credit[NH_CREDIT_DIR_RX].credit_limit_cnt,
                  fc->credit[NH_CREDIT_DIR_RX].credit_consumed_cnt);
}

int nh_tx_credit_get(nh_flow_cntrl_t *fc, uint32_t timeout_ms)
{
    uintptr_t irq_flags;
    nh_credit_t *credit;

    if (fc == NULL) {
        return -1;
    }

    if ((fc->enable & NH_CREDIT_EN_TX) == 0U) {
        return 0;
    }

    credit = &fc->credit[NH_CREDIT_DIR_TX];
    if (credit->wait_sem == NULL) {
        return 0;
    }

    if (!rcp_os_sem_take(credit->wait_sem, nh_credit_ms_to_ticks(timeout_ms))) {
        return -1;
    }

    irq_flags = rcp_os_irq_save();
    credit->credit_consumed_cnt++;
    rcp_os_irq_restore(irq_flags);

    NH_CREDIT_LOG(fc,
                  "tx get: limit=%u consumed=%u\r\n",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);

    return 0;
}

void nh_tx_credit_put(nh_flow_cntrl_t *fc)
{
    uintptr_t irq_flags;
    nh_credit_t *credit;
    bool give_back = false;

    if (fc == NULL) {
        return;
    }

    if ((fc->enable & NH_CREDIT_EN_TX) == 0U) {
        return;
    }

    credit = &fc->credit[NH_CREDIT_DIR_TX];
    if (credit->wait_sem == NULL) {
        return;
    }

    irq_flags = rcp_os_irq_save();
    if (credit->credit_consumed_cnt != 0U) {
        credit->credit_consumed_cnt--;
        give_back = true;
    }
    rcp_os_irq_restore(irq_flags);

    if (give_back) {
        (void)rcp_os_sem_give(credit->wait_sem);
        NH_CREDIT_LOG(fc,
                      "tx put: limit=%u consumed=%u\r\n",
                      credit->credit_limit_cnt,
                      credit->credit_consumed_cnt);
    }
}

bool nh_tx_credit_update(nh_flow_cntrl_t *fc, uint8_t new_limit)
{
    uintptr_t irq_flags;
    nh_credit_t *credit;
    uint8_t delta;

    if (fc == NULL) {
        return false;
    }

    if ((fc->enable & NH_CREDIT_EN_TX) == 0U) {
        return false;
    }

    credit = &fc->credit[NH_CREDIT_DIR_TX];
    if (credit->wait_sem == NULL) {
        return false;
    }

    irq_flags = rcp_os_irq_save();
    delta = (uint8_t)(new_limit - credit->credit_limit_cnt);
    if ((int8_t)delta > 0) {
        credit->credit_limit_cnt = new_limit;
    }
    rcp_os_irq_restore(irq_flags);

    if ((int8_t)delta <= 0) {
        return false;
    }

    NH_CREDIT_LOG(fc,
                  "tx update: limit=%u consumed=%u delta=%u\r\n",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt,
                  delta);

    while (delta-- != 0U) {
        if (!rcp_os_sem_give(credit->wait_sem)) {
            break;
        }
    }

    return true;
}

void nh_rx_credit_reclaim(nh_flow_cntrl_t *fc)
{
    uintptr_t irq_flags;
    nh_credit_t *credit;

    if (fc == NULL || (fc->enable & NH_CREDIT_EN_RX) == 0U) {
        return;
    }

    credit = &fc->credit[NH_CREDIT_DIR_RX];

    irq_flags = rcp_os_irq_save();
    credit->credit_consumed_cnt++;
    rcp_os_irq_restore(irq_flags);

    NH_CREDIT_LOG(fc,
                  "rx reclaim: advertised=%u available=%u\r\n",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);
}

bool nh_rx_credit_need_update(nh_flow_cntrl_t *fc)
{
    nh_credit_t *credit;
    uint8_t delta;

    if (fc == NULL || (fc->enable & NH_CREDIT_EN_RX) == 0U) {
        return false;
    }

    credit = &fc->credit[NH_CREDIT_DIR_RX];
    delta = (uint8_t)(credit->credit_consumed_cnt - credit->credit_limit_cnt);

    if (credit->threshold == 0U) {
        return delta != 0U;
    }

    return delta >= credit->threshold;
}

int nh_rx_credit_update(nh_flow_cntrl_t *fc)
{
    uintptr_t irq_flags;
    int need_update = -1;
    nh_credit_t *credit;

    if (fc == NULL) {
        return -1;
    }

    if ((fc->enable & NH_CREDIT_EN_RX) == 0U) {
        return -1;
    }

    credit = &fc->credit[NH_CREDIT_DIR_RX];

    irq_flags = rcp_os_irq_save();
    if (credit->credit_limit_cnt != credit->credit_consumed_cnt) {
        credit->credit_limit_cnt = credit->credit_consumed_cnt;
        need_update = credit->credit_consumed_cnt;
    }
    rcp_os_irq_restore(irq_flags);

    NH_CREDIT_LOG(fc,
                  "rx %supdate: limit=%u consumed=%u\r\n",
                  (need_update >= 0) ? "" : "NOT ",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);

    return need_update;
}

void nh_credit_deinit(nh_flow_cntrl_t *fc)
{
    if (fc == NULL) {
        return;
    }

    if (fc->credit[NH_CREDIT_DIR_TX].wait_sem != NULL) {
        rcp_os_sem_delete(fc->credit[NH_CREDIT_DIR_TX].wait_sem);
    }

    memset(fc, 0, sizeof(*fc));
}
