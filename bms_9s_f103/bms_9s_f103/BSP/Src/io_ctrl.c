/**
 * @file    io_ctrl.c
 * @brief   BSP 层 GPIO 控制驱动实现 — PA8 WAKE_BQ 单引脚
 * @note    PA8 由 CubeMX MX_GPIO_Init 配置为推挽输出。
 *          直接操作 BSRR/BRR 寄存器，避免 HAL 开销。
 *          BQ76940 唤醒：HIGH 脉冲 ≥100µs → LOW，芯片从 SHIP 进入 NORMAL。
 */

#include "io_ctrl.h"

/* ============================================================
 * GPIO 硬件映射
 * ============================================================ */

/** @brief 控制引脚硬件配置 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} io_hw_cfg_t;

/** @brief 控制引脚硬件配置查找表 */
static const io_hw_cfg_t io_cfg[] = {
    [IO_ID_WAKE_BQ] = { MCU_WAKE_BQ_GPIO_Port, MCU_WAKE_BQ_Pin },
};

/* ============================================================
 * 初始化
 * ============================================================ */

io_ctrl_status_t io_ctrl_init(void)
{
    /* PA8 初始化为低电平（不唤醒） */
    io_cfg[IO_ID_WAKE_BQ].port->BRR = (uint32_t)io_cfg[IO_ID_WAKE_BQ].pin;

    return BSP_OK;
}

/* ============================================================
 * 引脚控制
 * ============================================================ */

io_ctrl_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level)
{
    if (id != IO_ID_WAKE_BQ) {
        return BSP_ERROR;
    }

    const io_hw_cfg_t *cfg = &io_cfg[id];

    if (level == IO_LEVEL_HIGH) {
        cfg->port->BSRR = (uint32_t)cfg->pin;   /* 置高 */
    } else {
        cfg->port->BRR  = (uint32_t)cfg->pin;   /* 置低 */
    }

    return BSP_OK;
}
