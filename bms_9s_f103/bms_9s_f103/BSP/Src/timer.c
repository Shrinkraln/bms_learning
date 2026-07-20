/**
 * @file    timer.c
 * @brief   BSP 层通用定时器驱动实现
 * @note    TIM2 挂载在 APB1 (36MHz)，16 位向上计数器
 *          - Prescaler: 36-1 → 1MHz 计数频率 (1μs 分辨率)
 *          - Period: 可配置，最大 65535 → 65ms (带重载) 或级联
 *          - 对于 >65ms 的周期，使用软件分频
 */

#include "timer.h"
#include "tim.h"

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief 定时器上下文 */
typedef struct {
    timer_callback_t callback;     /**< 用户回调      */
    uint32_t         period_ms;    /**< 目标周期 (ms) */
    volatile uint32_t tick_count;  /**< 软件滴答计数  */
    volatile uint32_t tick_target; /**< 触发阈值      */
    uint8_t           is_oneshot;  /**< 单次模式标志  */
    uint8_t           is_running;  /**< 运行标志      */
} timer_ctx_t;

static timer_ctx_t timer_ctx[TIMER_ID_MAX];

/* ============================================================
 * 初始化
 * ============================================================ */

timer_status_t timer_init(void)
{
    /* TIM2 由 CubeMX 初始化 (MX_TIM2_Init) — 此处做 BSP 层附加配置 */
    for (uint8_t i = 0U; i < TIMER_ID_MAX; i++) {
        timer_ctx[i].callback   = NULL;
        timer_ctx[i].period_ms  = 0U;
        timer_ctx[i].tick_count = 0U;
        timer_ctx[i].tick_target = 0U;
        timer_ctx[i].is_oneshot = 0U;
        timer_ctx[i].is_running = 0U;
    }

    return TIMER_OK;
}

/* ============================================================
 * 启动 / 停止
 * ============================================================ */

timer_status_t timer_start_periodic(timer_id_t id, uint16_t period_ms,
                                     timer_callback_t cb)
{
    if (id >= TIMER_ID_MAX || cb == NULL || period_ms == 0U) {
        return TIMER_ERROR;
    }

    timer_ctx_t *ctx = &timer_ctx[id];

    /* 配置 TIM2: 1kHz 计数 → 1ms 分辨率, Period = period_ms-1 */
    /* APB1=36MHz, PSC=35→1MHz, ARR=999→1kHz */
    uint32_t psc = 35U;   /* 36MHz / 36 = 1MHz */
    uint32_t arr = 999U;  /* 1MHz / 1000 = 1kHz → 1ms */

    __HAL_TIM_SET_PRESCALER(&htim2, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);

    ctx->callback    = cb;
    ctx->period_ms   = period_ms;
    ctx->tick_count  = 0U;
    ctx->tick_target = (uint32_t)period_ms;  /* 每 1ms 中断一次，累计到 period_ms */
    ctx->is_oneshot  = 0U;
    ctx->is_running  = 1U;

    /* 启动 TIM2 中断模式 */
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim2);

    /* 使能 TIM2 中断（CubeMX 已配置 NVIC） */
    return TIMER_OK;
}

timer_status_t timer_start_oneshot(timer_id_t id, uint16_t ms,
                                    timer_callback_t cb)
{
    timer_status_t ret = timer_start_periodic(id, ms, cb);
    if (ret == TIMER_OK) {
        timer_ctx[id].is_oneshot = 1U;
    }
    return ret;
}

timer_status_t timer_stop(timer_id_t id)
{
    if (id >= TIMER_ID_MAX) {
        return TIMER_ERROR;
    }

    HAL_TIM_Base_Stop_IT(&htim2);
    timer_ctx[id].is_running = 0U;
    timer_ctx[id].callback   = NULL;

    return TIMER_OK;
}

uint32_t timer_get_count(timer_id_t id)
{
    (void)id;
    return (uint32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

/* ============================================================
 * TIM2 中断回调
 * ============================================================ */

/**
 * @brief  BSP 定时器中断处理函数
 * @note   需要在 main.c 的 HAL_TIM_PeriodElapsedCallback() 中调用
 *         用法: if (htim->Instance == TIM2) { bsp_timer_irq_handler(htim); }
 * @param  htim  TIM 句柄指针
 */
void bsp_timer_irq_handler(TIM_HandleTypeDef *htim)
{
    if (htim == NULL || htim->Instance != TIM2) {
        return;
    }

    /* 检查每个定时器上下文 */
    for (uint8_t i = 0U; i < TIMER_ID_MAX; i++) {
        timer_ctx_t *ctx = &timer_ctx[i];

        if (!ctx->is_running || ctx->callback == NULL) {
            continue;
        }

        ctx->tick_count++;

        if (ctx->tick_count >= ctx->tick_target) {
            ctx->tick_count = 0U;

            /* 调用用户回调 */
            if (ctx->callback != NULL) {
                ctx->callback();
            }

            /* 单次模式自动停止 */
            if (ctx->is_oneshot) {
                ctx->is_running = 0U;
                ctx->callback   = NULL;
            }
        }
    }
}
