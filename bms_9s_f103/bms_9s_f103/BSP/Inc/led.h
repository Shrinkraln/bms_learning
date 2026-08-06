/**
 * @file    led.h
 * @brief   BSP 层 LED 驱动 — PA15 单 LED
 * @note    PA15 (LED_0_ON), 低电平点亮。由 CubeMX MX_GPIO_Init 配置为推挽输出。
 */

#ifndef BSP_LED_H
#define BSP_LED_H

#include "stm32f1xx_hal.h"
#include "main.h"
#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化 LED（设为熄灭状态）
 */
void led_init(void);

/**
 * @brief  点亮 LED
 */
void led_on(void);

/**
 * @brief  熄灭 LED
 */
void led_off(void);

/**
 * @brief  翻转 LED 状态
 */
void led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
