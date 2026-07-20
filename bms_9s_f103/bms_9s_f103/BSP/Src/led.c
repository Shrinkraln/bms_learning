/**
 * @file    led.c
 * @brief   BSP 层 LED 驱动实现
 * @note    低电平点亮 — HAL_GPIO_WritePin(PORT, PIN, GPIO_PIN_RESET) = 亮
 */

#include "led.h"

/* ============================================================
 * LED 硬件映射表
 * ============================================================ */

/** @brief LED 属性结构 */
typedef struct {
    GPIO_TypeDef *port;    /**< GPIO 端口        */
    uint16_t      pin;     /**< GPIO 引脚        */
    uint8_t       active;  /**< 有效电平: 0=低电平, 1=高电平 */
} led_hw_cfg_t;

/** @brief LED 硬件配置查找表 */
static const led_hw_cfg_t led_cfg[LED_ID_MAX] = {
    [LED_ID_1] = { LED1_GPIO_Port, LED1_Pin, 0U },   /* PC13, 低电平亮 */
    [LED_ID_2] = { LED2_GPIO_Port, LED2_Pin, 0U },   /* PB0,  低电平亮 */
};

/** @brief LED 运行状态（记录当前亮灭） */
static uint8_t led_state[LED_ID_MAX] = { 0U };

/* ============================================================
 * 初始化
 * ============================================================ */

led_status_t led_init(void)
{
    for (uint8_t i = 0U; i < LED_ID_MAX; i++) {
        /* 初始化为熄灭状态 */
        GPIO_PinState init_level = (led_cfg[i].active == 0U)
                                   ? GPIO_PIN_SET
                                   : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(led_cfg[i].port, led_cfg[i].pin, init_level);
        led_state[i] = 0U;
    }

    return LED_OK;
}

/* ============================================================
 * LED 控制
 * ============================================================ */

led_status_t led_on(led_id_t id)
{
    if (id >= LED_ID_MAX) {
        return LED_ERROR;
    }

    GPIO_PinState level = (led_cfg[id].active == 0U)
                          ? GPIO_PIN_RESET
                          : GPIO_PIN_SET;

    HAL_GPIO_WritePin(led_cfg[id].port, led_cfg[id].pin, level);
    led_state[id] = 1U;

    return LED_OK;
}

led_status_t led_off(led_id_t id)
{
    if (id >= LED_ID_MAX) {
        return LED_ERROR;
    }

    GPIO_PinState level = (led_cfg[id].active == 0U)
                          ? GPIO_PIN_SET
                          : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(led_cfg[id].port, led_cfg[id].pin, level);
    led_state[id] = 0U;

    return LED_OK;
}

led_status_t led_toggle(led_id_t id)
{
    if (id >= LED_ID_MAX) {
        return LED_ERROR;
    }

    HAL_GPIO_TogglePin(led_cfg[id].port, led_cfg[id].pin);

    /* 翻转后读取实际引脚电平来确定状态 */
    GPIO_PinState current = HAL_GPIO_ReadPin(led_cfg[id].port, led_cfg[id].pin);

    /* 根据有效电平换算逻辑状态 */
    if (led_cfg[id].active == 0U) {
        led_state[id] = (current == GPIO_PIN_RESET) ? 1U : 0U;
    } else {
        led_state[id] = (current == GPIO_PIN_SET)   ? 1U : 0U;
    }

    return LED_OK;
}

/* ============================================================
 * 状态查询
 * ============================================================ */

uint8_t led_get_state(led_id_t id)
{
    if (id >= LED_ID_MAX) {
        return 0U;
    }

    return led_state[id];
}
