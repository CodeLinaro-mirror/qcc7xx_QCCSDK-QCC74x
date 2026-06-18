#ifndef MR_LP_GPIO_H
#define MR_LP_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include "qcc74x_core.h"
#include "qcc74x_gpio.h"
#include "qcc74x_irq.h"

typedef void (*mr_lp_gpio_ready_cb_t)(void *arg);

typedef struct {
    uint8_t wake_notify_pin;
    uint8_t ready_pin;
    uint8_t wake_out_active_level;
    uint32_t resume_cfg;
    uint32_t notify_pin_cfg;
    uint32_t ready_pin_cfg;
    uint8_t ready_irq_trig;
} mr_lp_gpio_cfg_t;

typedef struct {
    struct qcc74x_device_s *gpio;
    mr_lp_gpio_cfg_t cfg;
    mr_lp_gpio_ready_cb_t ready_cb;
    void *ready_arg;
} mr_lp_gpio_t;

static mr_lp_gpio_t *g_mr_lp_gpio_irq_ctx;

static inline void mr_lp_gpio_wakeup_notify(mr_lp_gpio_t *ctx, bool notify);

static inline void mr_lp_gpio_restore_pin(mr_lp_gpio_t *ctx, uint8_t pin)
{
    qcc74x_gpio_int_mask(ctx->gpio, pin, true);
    qcc74x_gpio_int_clear(ctx->gpio, pin);
    if (ctx->cfg.resume_cfg) {
        qcc74x_gpio_init(ctx->gpio, pin, ctx->cfg.resume_cfg);
    }
}

static void mr_lp_gpio_irq_cb(uint8_t pin)
{
    mr_lp_gpio_t *ctx = g_mr_lp_gpio_irq_ctx;

    if ((ctx == NULL) || (pin != ctx->cfg.ready_pin)) {
        return;
    }

    mr_lp_gpio_wakeup_notify(ctx, 1);
    mr_lp_gpio_restore_pin(ctx, ctx->cfg.ready_pin);
    if (ctx->ready_cb) {
        ctx->ready_cb(ctx->ready_arg);
    }
}

static inline int mr_lp_gpio_init(mr_lp_gpio_t *ctx, const mr_lp_gpio_cfg_t *cfg)
{
    if ((ctx == NULL) || (cfg == NULL)) {
        return -1;
    }

    ctx->gpio = qcc74x_device_get_by_name("gpio");
    if (ctx->gpio == NULL) {
        return -1;
    }

    ctx->cfg = *cfg;
    ctx->ready_cb = NULL;
    ctx->ready_arg = NULL;
    g_mr_lp_gpio_irq_ctx = ctx;
    return 0;
}

static inline void mr_lp_gpio_wakeup_notify(mr_lp_gpio_t *ctx, bool notify)
{
    if ((ctx == NULL) || (ctx->gpio == NULL)) {
        return;
    }

    if (notify && ctx->cfg.resume_cfg) {
        qcc74x_gpio_set(ctx->gpio, ctx->cfg.wake_notify_pin);
        qcc74x_gpio_init(ctx->gpio, ctx->cfg.wake_notify_pin, ctx->cfg.resume_cfg);
        return;
    }

    qcc74x_gpio_init(ctx->gpio, ctx->cfg.wake_notify_pin, ctx->cfg.notify_pin_cfg);
    if (ctx->cfg.wake_out_active_level) {
        qcc74x_gpio_set(ctx->gpio, ctx->cfg.wake_notify_pin);
    } else {
        qcc74x_gpio_reset(ctx->gpio, ctx->cfg.wake_notify_pin);
    }
}

static inline int mr_lp_gpio_peer_ready(mr_lp_gpio_t *ctx, mr_lp_gpio_ready_cb_t cb, void *arg)
{
    if ((ctx == NULL) || (ctx->gpio == NULL)) {
        return -1;
    }

    ctx->ready_cb = cb;
    ctx->ready_arg = arg;

    qcc74x_gpio_init(ctx->gpio, ctx->cfg.ready_pin, ctx->cfg.ready_pin_cfg);
    qcc74x_gpio_int_init(ctx->gpio, ctx->cfg.ready_pin, ctx->cfg.ready_irq_trig);
    qcc74x_gpio_irq_attach(ctx->cfg.ready_pin, mr_lp_gpio_irq_cb);
    arch_delay_us(70);
    mr_lp_gpio_wakeup_notify(ctx, 0);
    qcc74x_irq_enable(ctx->gpio->irq_num);

    return 0;
}

#endif
