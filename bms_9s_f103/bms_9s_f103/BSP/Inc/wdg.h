/**
 * @file    wdg.h
 * @brief   BSP 层独立看门狗 (IWDG) 驱动
 * @note    使用 STM32F1 内置独立看门狗
 *          - 由内部 LSI (~40kHz) 驱动
 *          - 一旦使能，只能由硬件复位关闭
 *          - 在调试模式下可配置为暂停计数
 */

#ifndef __BSP_WDG_H
#define __BSP_WDG_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 看门狗操作状态 */
typedef enum {
    WDG_OK      = 0x00U,
    WDG_ERROR   = 0x01U,
} wdg_status_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化并启动独立看门狗
 * @note   启动后无法软件停止，需周期性调用 wdg_kick()
 *         CubeMX 生成的 MX_IWDG_Init() 已配置基础参数
 *         此函数在 MX_IWDG_Init() 基础上做 BSP 层封装
 * @param  timeout_ms  看门狗超时时间 (ms)
 *                     LSI=40kHz, Prescaler=4 → 100μs/tick
 *                     Reload 范围: 0-4095 → 最大超时 ~409ms@/4
 *                     对于更长超时使用更大的 Prescaler
 * @retval 操作状态
 */
wdg_status_t wdg_init(uint32_t timeout_ms);

/**
 * @brief  喂狗 — 刷新看门狗计数器
 * @note   必须在超时前周期性调用，否则触发系统复位
 *         推荐在 FreeRTOS 最低优先级任务中调用
 * @retval 操作状态
 */
wdg_status_t wdg_kick(void);

/**
 * @brief  获取看门狗当前计数值（调试用）
 * @retval 剩余计数值
 */
uint16_t wdg_get_counter(void);

/**
 * @brief  检查上次复位是否由看门狗引起
 * @retval 1 = 看门狗复位, 0 = 其他原因
 */
uint8_t wdg_is_reset_source(void);

/**
 * @brief  使能调试模式下的看门狗暂停
 * @note   调试时暂停计数，避免断点导致复位
 */
void wdg_debug_pause(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_WDG_H */
