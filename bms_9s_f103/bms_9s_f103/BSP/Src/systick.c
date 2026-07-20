/**
 * @file    systick.c
 * @brief   BSP 层系统滴答实现
 * @note    HAL 使用 TIM1 作为时基源（见 stm32f1xx_hal_timebase_tim.c）
 *          - HAL_GetTick() 精度 1ms
 *          - DWT 用于微秒级精确延时
 */

#include "systick.h"

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief 标记 DWT 是否已初始化 */
static uint8_t dwt_enabled = 0U;

/* ============================================================
 * 初始化
 * ============================================================ */

bsp_tick_status_t bsp_tick_init(void)
{
    /* 使能 DWT (Data Watchpoint and Trace) 周期计数器 */
    /* DWT 是 Cortex-M3 调试组件，需先使能 TRCENA 位 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 清零并启动 DWT 周期计数器 */
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    dwt_enabled = 1U;

    return BSP_TICK_OK;
}

/* ============================================================
 * 滴答获取
 * ============================================================ */

uint32_t bsp_tick_get(void)
{
    return HAL_GetTick();
}

/* ============================================================
 * 阻塞延时
 * ============================================================ */

void bsp_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

void bsp_delay_us(uint32_t us)
{
    if (dwt_enabled == 0U) {
        /* DWT 未初始化，回退到简单循环（不精确，仅用于紧急情况） */
        for (volatile uint32_t i = 0U; i < us * 10U; i++) {
            __NOP();
        }
        return;
    }

    /* 使用 DWT 周期计数器实现精确微秒延时 */
    /* 系统时钟 72MHz → 每微秒 72 个周期 */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < ticks) {
        /* 等待 */
    }
}

/* ============================================================
 * 超时计算（自动处理 32 位翻转）
 * ============================================================ */

uint32_t bsp_tick_elapsed(uint32_t start_tick)
{
    uint32_t now = HAL_GetTick();

    /* 差值法天然处理翻转：now - start 在 32 位无符号数下总是正确 */
    return now - start_tick;
}

uint8_t bsp_tick_is_timeout(uint32_t start_tick, uint32_t timeout_ms)
{
    return (bsp_tick_elapsed(start_tick) >= timeout_ms) ? 1U : 0U;
}
