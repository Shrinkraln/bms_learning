/**
 * @file    io_ctrl.h
 * @brief   BSP 层 GPIO 控制驱动 — PA8 WAKE_BQ 单引脚
 * @note    仅管理 PA8 (MCU_WAKE_BQ)，用于 BQ76940 启动唤醒。
 *          引脚由 CubeMX MX_GPIO_Init 配置为推挽输出。
 */

#ifndef BSP_IO_CTRL_H
#define BSP_IO_CTRL_H

#include "stm32f1xx_hal.h"
#include "main.h"
#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 控制引脚编号 */
typedef enum {
    IO_ID_WAKE_BQ = 0U,      /**< PA8  MCU_WAKE_BQ  唤醒 BQ76940 */
} io_ctrl_pin_t;

/** @brief 引脚电平 */
typedef enum {
    IO_LEVEL_LOW  = 0U,
    IO_LEVEL_HIGH = 1U,
} io_level_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化控制 IO（设置默认安全状态 = 低电平）
 * @retval BSP_OK
 */
io_ctrl_status_t io_ctrl_init(void);

/**
 * @brief  设置指定控制引脚输出电平
 * @param  id     引脚编号 (当前仅 IO_ID_WAKE_BQ)
 * @param  level  输出电平 (IO_LEVEL_LOW / IO_LEVEL_HIGH)
 * @retval BSP_OK / BSP_ERROR
 */
io_ctrl_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IO_CTRL_H */
