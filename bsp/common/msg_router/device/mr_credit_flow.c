#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "qcc74x_core.h"

#include "mr_credit_flow.h"

/* #define MR_CREDIT_FLOW_LOG_ENABLE 1 */

#ifndef MR_CREDIT_FLOW_LOG_ENABLE
#define MR_CREDIT_FLOW_LOG_ENABLE 0
#endif

#if MR_CREDIT_FLOW_LOG_ENABLE
#define MR_CREDIT_LOG(fc, fmt, ...) \
    printf("[credit][%s] " fmt, mr_credit_log_tag(fc), ##__VA_ARGS__)
#else
#define MR_CREDIT_LOG(fc, fmt, ...) do { } while (0)
#endif

static inline const char *mr_credit_log_tag(const mr_flow_ctrl_t *fc)
{
    return (fc != NULL && fc->tag != NULL) ? fc->tag : "?";
}

static inline TickType_t mr_credit_ms_to_ticks(uint32_t timeout_ms)
{
    return (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
}

int mr_credit_init(mr_flow_ctrl_t *fc, const char *name,
                   uint8_t max_txcredit, uint8_t tx_threshold,
                   uint8_t max_rxcredit, uint8_t rx_threshold)
{
    SemaphoreHandle_t tx_wait_sem;
    uint8_t tx_wait_sem_max;
    uint8_t tx_wait_sem_fill;

    if (fc == NULL) {
        return -1;
    }

    tx_wait_sem = (SemaphoreHandle_t)fc->credit[MR_CREDIT_DIR_TX].wait_sem;
    tx_wait_sem_max = fc->credit[MR_CREDIT_DIR_TX].init_credit;

    if (tx_wait_sem != NULL) {
        if (max_txcredit == 0U || tx_wait_sem_max < max_txcredit) {
            vSemaphoreDelete(tx_wait_sem);
            tx_wait_sem = NULL;
        } else {
            xQueueReset((QueueHandle_t)tx_wait_sem);
            for (tx_wait_sem_fill = 0; tx_wait_sem_fill < max_txcredit; tx_wait_sem_fill++) {
                (void)xSemaphoreGive(tx_wait_sem);
            }
        }
    }

    memset(fc, 0, sizeof(*fc));

    fc->credit[MR_CREDIT_DIR_TX].init_credit = max_txcredit;
    fc->credit[MR_CREDIT_DIR_TX].threshold = tx_threshold;
    fc->credit[MR_CREDIT_DIR_TX].credit_limit_cnt = max_txcredit;

    fc->credit[MR_CREDIT_DIR_RX].init_credit = max_rxcredit;
    fc->credit[MR_CREDIT_DIR_RX].threshold = rx_threshold;
    fc->credit[MR_CREDIT_DIR_RX].credit_consumed_cnt = max_rxcredit;

    if (max_txcredit != 0U) {
        if (tx_wait_sem == NULL) {
            tx_wait_sem = xSemaphoreCreateCounting(max_txcredit, max_txcredit);
        }
        fc->credit[MR_CREDIT_DIR_TX].wait_sem = tx_wait_sem;
        if (fc->credit[MR_CREDIT_DIR_TX].wait_sem == NULL) {
            return -1;
        }
    }

    fc->tag = name;

    MR_CREDIT_LOG(fc,
                  "init: tx_init=%u tx_th=%u rx_init=%u rx_th=%u\r\n",
                  max_txcredit, tx_threshold, max_rxcredit, rx_threshold);

    return 0;
}

void mr_credit_reset(mr_flow_ctrl_t *fc)
{
    uintptr_t irq_flags;

    if (fc == NULL) {
        return;
    }

    if (fc->credit[MR_CREDIT_DIR_TX].wait_sem != NULL) {
        xQueueReset((QueueHandle_t)fc->credit[MR_CREDIT_DIR_TX].wait_sem);
    }

    irq_flags = qcc74x_irq_save();
    fc->credit[MR_CREDIT_DIR_TX].credit_limit_cnt = 0;
    fc->credit[MR_CREDIT_DIR_TX].credit_consumed_cnt = 0;
    fc->credit[MR_CREDIT_DIR_RX].credit_limit_cnt = 0;
    fc->credit[MR_CREDIT_DIR_RX].credit_consumed_cnt = fc->credit[MR_CREDIT_DIR_RX].init_credit;
    qcc74x_irq_restore(irq_flags);

    MR_CREDIT_LOG(fc,
                  "reset: tx_limit=%u tx_consumed=%u rx_advertised=%u rx_available=%u\r\n",
                  fc->credit[MR_CREDIT_DIR_TX].credit_limit_cnt,
                  fc->credit[MR_CREDIT_DIR_TX].credit_consumed_cnt,
                  fc->credit[MR_CREDIT_DIR_RX].credit_limit_cnt,
                  fc->credit[MR_CREDIT_DIR_RX].credit_consumed_cnt);
}

int mr_tx_credit_get(mr_flow_ctrl_t *fc, uint32_t timeout_ms)
{
    uintptr_t irq_flags;
    mr_credit_t *credit;

    if (fc == NULL) {
        return -1;
    }

    if ((fc->enable & MR_CREDIT_EN_TX) == 0U) {
        return 0;
    }

    credit = &fc->credit[MR_CREDIT_DIR_TX];
    if (credit->wait_sem == NULL) {
        return 0;
    }

    if (xSemaphoreTake((SemaphoreHandle_t)credit->wait_sem, mr_credit_ms_to_ticks(timeout_ms)) != pdTRUE) {
        return -1;
    }

    irq_flags = qcc74x_irq_save();
    credit->credit_consumed_cnt++;
    qcc74x_irq_restore(irq_flags);

    MR_CREDIT_LOG(fc,
                  "tx get: limit=%u consumed=%u\r\n",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);

    return 0;
}

void mr_tx_credit_put(mr_flow_ctrl_t *fc)
{
    uintptr_t irq_flags;
    mr_credit_t *credit;
    bool give_back = false;

    if (fc == NULL) {
        return;
    }

    if ((fc->enable & MR_CREDIT_EN_TX) == 0U) {
        return;
    }

    credit = &fc->credit[MR_CREDIT_DIR_TX];
    if (credit->wait_sem == NULL) {
        return;
    }

    irq_flags = qcc74x_irq_save();
    if (credit->credit_consumed_cnt != 0U) {
        credit->credit_consumed_cnt--;
        give_back = true;
    }
    qcc74x_irq_restore(irq_flags);

    if (give_back) {
        (void)xSemaphoreGive((SemaphoreHandle_t)credit->wait_sem);
        MR_CREDIT_LOG(fc,
                      "tx put: limit=%u consumed=%u\r\n",
                      credit->credit_limit_cnt,
                      credit->credit_consumed_cnt);
    }
}

bool mr_tx_credit_update(mr_flow_ctrl_t *fc, uint8_t new_limit)
{
    uintptr_t irq_flags;
    mr_credit_t *credit;
    uint8_t delta;

    if (fc == NULL) {
        return false;
    }

    if ((fc->enable & MR_CREDIT_EN_TX) == 0U) {
        return false;
    }

    credit = &fc->credit[MR_CREDIT_DIR_TX];
    if (credit->wait_sem == NULL) {
        return false;
    }

    irq_flags = qcc74x_irq_save();
    delta = (uint8_t)(new_limit - credit->credit_limit_cnt);
    if ((int8_t)delta > 0) {
        credit->credit_limit_cnt = new_limit;
    }
    qcc74x_irq_restore(irq_flags);

    if ((int8_t)delta <= 0) {
        return false;
    }

    MR_CREDIT_LOG(fc,
                  "tx update: limit=%u consumed=%u delta=%u\r\n",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt,
                  delta);

    while (delta-- != 0U) {
        if (xSemaphoreGive((SemaphoreHandle_t)credit->wait_sem) != pdTRUE) {
            break;
        }
    }

    return true;
}

void mr_rx_credit_reclaim(mr_flow_ctrl_t *fc)
{
    uintptr_t irq_flags;
    mr_credit_t *credit;

    if (fc == NULL || (fc->enable & MR_CREDIT_EN_RX) == 0U) {
        return;
    }

    credit = &fc->credit[MR_CREDIT_DIR_RX];

    irq_flags = qcc74x_irq_save();
    credit->credit_consumed_cnt++;
    qcc74x_irq_restore(irq_flags);

    MR_CREDIT_LOG(fc,
                  "rx reclaim: advertised=%u available=%u\r\n",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);
}

bool mr_rx_credit_need_update(mr_flow_ctrl_t *fc)
{
    mr_credit_t *credit;
    uint8_t delta;

    if (fc == NULL || (fc->enable & MR_CREDIT_EN_RX) == 0U) {
        return false;
    }

    credit = &fc->credit[MR_CREDIT_DIR_RX];
    delta = (uint8_t)(credit->credit_consumed_cnt - credit->credit_limit_cnt);

    if (credit->threshold == 0U) {
        return delta != 0U;
    }

    return delta >= credit->threshold;
}

int mr_rx_credit_prepare(mr_flow_ctrl_t *fc)
{
    uintptr_t irq_flags;
    int prepared = -1;
    mr_credit_t *credit;

    if (fc == NULL) {
        return -1;
    }

    if ((fc->enable & MR_CREDIT_EN_RX) == 0U) {
        return -1;
    }

    credit = &fc->credit[MR_CREDIT_DIR_RX];

    irq_flags = qcc74x_irq_save();
    if (credit->credit_limit_cnt != credit->credit_consumed_cnt) {
        prepared = credit->credit_consumed_cnt;
    }
    qcc74x_irq_restore(irq_flags);

    MR_CREDIT_LOG(fc,
                  "rx %sprepare: limit=%u consumed=%u\r\n",
                  (prepared >= 0) ? "" : "NOT ",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);

    return prepared;
}

int mr_rx_credit_update(mr_flow_ctrl_t *fc)
{
    uintptr_t irq_flags;
    int need_update = -1;
    mr_credit_t *credit;

    if (fc == NULL) {
        return -1;
    }

    if ((fc->enable & MR_CREDIT_EN_RX) == 0U) {
        return -1;
    }

    credit = &fc->credit[MR_CREDIT_DIR_RX];

    irq_flags = qcc74x_irq_save();
    if (credit->credit_limit_cnt != credit->credit_consumed_cnt) {
        credit->credit_limit_cnt = credit->credit_consumed_cnt;
        need_update = credit->credit_consumed_cnt;
    }
    qcc74x_irq_restore(irq_flags);

    MR_CREDIT_LOG(fc,
                  "rx %supdate: limit=%u consumed=%u\r\n",
                  (need_update >= 0) ? "" : "NOT ",
                  credit->credit_limit_cnt,
                  credit->credit_consumed_cnt);

    return need_update;
}

void mr_credit_deinit(mr_flow_ctrl_t *fc)
{
    if (fc == NULL) {
        return;
    }

    if (fc->credit[MR_CREDIT_DIR_TX].wait_sem != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)fc->credit[MR_CREDIT_DIR_TX].wait_sem);
    }

    memset(fc, 0, sizeof(*fc));
}
