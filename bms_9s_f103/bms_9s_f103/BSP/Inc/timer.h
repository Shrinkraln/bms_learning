/**
 * @file    timer.h
 * @brief   BSP 层通用定时器驱动
 * @note    基于 TIM2，提供周期性定时回调
 *          - TIM1 已被 HAL 时基占用（不要使用）
 *          - TIM2 用于 BMS 应用定时（采样周期、保护检测等）
 *          - TIM2 挂载在 APB1 (36MHz)，16 位计数器
 */

#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 定时器操作状态 */
typedef enum {
    TIMER_OK      = 0x00U,
    TIMER_ERROR   = 0x01U,
    TIMER_BUSY    = 0x02U,
} timer_status_t;

/** @brief 定时器回调函数类型 */
typedef void (*timer_callback_t)(void);

/** @brief 定时器编号 */
typedef enum {
    TIMER_ID_2 = 0U,   /**< TIM2 — 通用定时器 */
    TIMER_ID_MAX
} timer_id_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化定时器
 * @note   配置 TIM2 为向上计数模式，APB1=36MHz
 * @retval 操作状态
 */
timer_status_t timer_init(void);

/**
 * @brief  启动周期性定时器
 * @param  id        定时器编号
 * @param  period_ms 周期 (ms), 最大约 1800ms (16-bit @ 36MHz / 65535)
 * @param  cb        回调函数，在 TIM IRQ 中调用
 * @retval 操作状态
 */
timer_status_t timer_start_periodic(timer_id_t id, uint16_t period_ms,
                                     timer_callback_t cb);

/**
 * @brief  停止周期性定时器
 * @param  id  定时器编号
 * @retval 操作状态
 */
timer_status_t timer_stop(timer_id_t id);

/**
 * @brief  启动单次定时器
 * @param  id     定时器编号
 * @param  ms     延时 (ms)
 * @param  cb     回调函数（单次触发后自动停止）
 * @retval 操作状态
 */
timer_status_t timer_start_oneshot(timer_id_t id, uint16_t ms,
                                    timer_callback_t cb);

/**
 * @brief  获取定时器计数值（μs 精度，用于测量）
 * @param  id  定时器编号
 * @retval 当前计数值
 */
uint32_t timer_get_count(timer_id_t id);

/**
 * @brief  BSP 定时器中断处理（需在 main.c 的 TIM 回调中调用）
 * @note   调用方式:
 *         void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *             if (htim->Instance == TIM1) { HAL_IncTick(); }
 *             bsp_timer_irq_handler(htim);  // <-- 添加此行
 *         }
 * @param  htim  TIM 句柄
 */
void bsp_timer_irq_handler(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_TIMER_H */
