/**
 * @file    led.h
 * @brief   BSP 层 LED 驱动
 * @note    板载 2 个 LED，低电平点亮（推挽输出驱动）
 *          - LED1: PC13
 *          - LED2: PB0
 */

#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief LED 操作状态 */
typedef enum {
    LED_OK       = 0x00U,
    LED_ERROR    = 0x01U,
    LED_BUSY     = 0x02U,
    LED_TIMEOUT  = 0x03U,
} led_status_t;

/** @brief LED 索引 */
typedef enum {
    LED_ID_1 = 0U,       /**< LED1 (PC13) */
    LED_ID_2 = 1U,       /**< LED2 (PB0)  */
    LED_ID_MAX            /**< LED 总数   */
} led_id_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化所有 LED（GPIO 由 MX_GPIO_Init 配置，此处设置初始状态）
 * @retval LED_OK
 */
led_status_t led_init(void);

/**
 * @brief  点亮指定 LED
 * @param  id  LED 编号 (LED_ID_1 / LED_ID_2)
 * @retval LED_OK / LED_ERROR
 */
led_status_t led_on(led_id_t id);

/**
 * @brief  熄灭指定 LED
 * @param  id  LED 编号 (LED_ID_1 / LED_ID_2)
 * @retval LED_OK / LED_ERROR
 */
led_status_t led_off(led_id_t id);

/**
 * @brief  翻转指定 LED 状态
 * @param  id  LED 编号 (LED_ID_1 / LED_ID_2)
 * @retval LED_OK / LED_ERROR
 */
led_status_t led_toggle(led_id_t id);

/**
 * @brief  获取指定 LED 当前状态
 * @param  id  LED 编号
 * @retval 1 = 亮, 0 = 灭
 */
uint8_t led_get_state(led_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LED_H */
