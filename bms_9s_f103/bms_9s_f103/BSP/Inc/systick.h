/**
 * @file    systick.h
 * @brief   BSP 层系统滴答封装
 * @note    基于 HAL 的 TIM1 时基（1ms），提供统一的延时和计时接口
 *          - HAL_GetTick() 返回 32 位毫秒值，约 49.7 天后翻转
 *          - 所有超时计算使用差值法，避免翻转问题
 */

#ifndef __BSP_SYSTICK_H
#define __BSP_SYSTICK_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 滴答操作状态 */
typedef enum {
    BSP_TICK_OK       = 0x00U,
    BSP_TICK_ERROR    = 0x01U,
} bsp_tick_status_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化系统滴答（HAL 已通过 TIM1 完成初始化）
 * @note   仅做存在性检查，实际初始化由 HAL_InitTick() 在 HAL_Init() 中完成
 * @retval BSP_TICK_OK
 */
bsp_tick_status_t bsp_tick_init(void);

/**
 * @brief  获取系统运行毫秒数
 * @retval 自启动以来的毫秒计数值（32 位，翻转周期约 49.7 天）
 */
uint32_t bsp_tick_get(void);

/**
 * @brief  阻塞延时（毫秒）
 * @note   在 FreeRTOS 任务中应优先使用 osDelay()；此函数用于 ISR 或启动阶段
 * @param  ms  延时毫秒数
 */
void bsp_delay_ms(uint32_t ms);

/**
 * @brief  阻塞延时（微秒）
 * @note   使用 DWT 周期计数器实现精确微秒延时
 *         需要 DWT 已使能（由 bsp_tick_init 自动完成）
 * @param  us  延时的微秒数
 */
void bsp_delay_us(uint32_t us);

/**
 * @brief  计算自某个时刻以来经过的毫秒数（自动处理 32 位翻转）
 * @param  start_tick  起始时刻（由 bsp_tick_get() 获取）
 * @retval 经过的毫秒数
 */
uint32_t bsp_tick_elapsed(uint32_t start_tick);

/**
 * @brief  检查是否已超时（自动处理 32 位翻转）
 * @param  start_tick  起始时刻
 * @param  timeout_ms  超时阈值（毫秒）
 * @retval 1 = 已超时, 0 = 未超时
 */
uint8_t bsp_tick_is_timeout(uint32_t start_tick, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SYSTICK_H */
