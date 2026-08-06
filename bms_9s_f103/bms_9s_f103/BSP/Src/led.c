/**
 * @file    led.c
 * @brief   BSP 层 LED 驱动实现 — PA15 单 LED
 * @note    低电平点亮 (active-low)。直接操作 BSRR/BRR 寄存器，避免 HAL 开销。
 */

#include "led.h"

/* ============================================================
 * 硬件常量
 * ============================================================ */

/* LED 连接 PA15，低电平点亮 */
#define LED_PORT     GPIOA
#define LED_PIN      GPIO_PIN_15
#define LED_ACTIVE   0U   /* 0 = 低电平点亮 */

/* ============================================================
 * 初始化
 * ============================================================ */

void led_init(void)
{
    /* PA15 由 MX_GPIO_Init 配置为推挽输出。此处确保熄灭。 */
    if (LED_ACTIVE == 0U) {
        LED_PORT->BSRR = (uint32_t)LED_PIN << 16U;  /* BRR: 置高 = 灭 */
    } else {
        LED_PORT->BSRR = (uint32_t)LED_PIN << 16U;  /* BRR: 置低 = 灭 */
    }
}

/* ============================================================
 * LED 控制
 * ============================================================ */

void led_on(void)
{
    if (LED_ACTIVE == 0U) {
        LED_PORT->BRR = (uint32_t)LED_PIN;   /* BRR: 置低 = 亮 */
    } else {
        LED_PORT->BSRR = (uint32_t)LED_PIN;  /* BSRR: 置高 = 亮 */
    }
}

void led_off(void)
{
    if (LED_ACTIVE == 0U) {
        LED_PORT->BSRR = (uint32_t)LED_PIN;  /* BSRR: 置高 = 灭 */
    } else {
        LED_PORT->BRR = (uint32_t)LED_PIN;   /* BRR: 置低 = 灭 */
    }
}

void led_toggle(void)
{
    /* 读取当前输出电平并翻转 */
    if (LED_PORT->ODR & LED_PIN) {
        /* 当前高电平 → 置低 */
        LED_PORT->BRR = (uint32_t)LED_PIN;
    } else {
        /* 当前低电平 → 置高 */
        LED_PORT->BSRR = (uint32_t)LED_PIN;
    }
}
