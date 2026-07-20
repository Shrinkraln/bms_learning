/**
 * @file    io_ctrl.c
 * @brief   BSP 层 GPIO 控制驱动实现
 * @note    所有引脚由 CubeMX 在 MX_GPIO_Init() 中配置为推挽输出
 *          本模块在此基础上提供带语义的访问接口
 */

#include "io_ctrl.h"

/* ============================================================
 * GPIO 硬件映射表
 * ============================================================ */

/** @brief 控制 IO 硬件配置 */
typedef struct {
    GPIO_TypeDef *port;      /**< GPIO 端口       */
    uint16_t      pin;       /**< GPIO 引脚       */
    GPIO_PinState default_level;  /**< 默认安全电平  */
} io_hw_cfg_t;

/** @brief 控制引脚硬件配置查找表 */
static const io_hw_cfg_t io_cfg[IO_ID_MAX] = {
    [IO_ID_BQ_POWER]  = { MCU_KZ_BQ_POWER_GPIO_Port,  MCU_KZ_BQ_POWER_Pin,  GPIO_PIN_RESET },
    [IO_ID_D_POWER]   = { MCU_D_POWER_GPIO_Port,       MCU_D_POWER_Pin,       GPIO_PIN_RESET },
    [IO_ID_WAKE_BQ]   = { MCU_WAKE_BQ_GPIO_Port,       MCU_WAKE_BQ_Pin,       GPIO_PIN_RESET },
    [IO_ID_STA_WAKE]  = { STA_WAKE_GPIO_Port,           STA_WAKE_Pin,          GPIO_PIN_RESET },
    [IO_ID_BQ1_ALERT] = { MCU_BQ1_ART_GPIO_Port,        MCU_BQ1_ART_Pin,      GPIO_PIN_RESET },
    [IO_ID_IN_STA]    = { MCU_IN_STA_GPIO_Port,          MCU_IN_STA_Pin,       GPIO_PIN_RESET },
};

/* ============================================================
 * 初始化
 * ============================================================ */

io_ctrl_status_t io_ctrl_init(void)
{
    /* 所有控制引脚初始化为默认安全电平（低电平 = 关闭/不使能） */
    for (uint8_t i = 0U; i < IO_ID_MAX; i++) {
        HAL_GPIO_WritePin(io_cfg[i].port, io_cfg[i].pin, io_cfg[i].default_level);
    }

    return IO_CTRL_OK;
}

/* ============================================================
 * 引脚读写
 * ============================================================ */

io_ctrl_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level)
{
    if (id >= IO_ID_MAX) {
        return IO_CTRL_ERROR;
    }

    GPIO_PinState hal_level = (level == IO_LEVEL_HIGH)
                              ? GPIO_PIN_SET
                              : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(io_cfg[id].port, io_cfg[id].pin, hal_level);

    return IO_CTRL_OK;
}

io_level_t io_ctrl_get(io_ctrl_pin_t id)
{
    if (id >= IO_ID_MAX) {
        return IO_LEVEL_LOW;
    }

    GPIO_PinState state = HAL_GPIO_ReadPin(io_cfg[id].port, io_cfg[id].pin);

    return (state == GPIO_PIN_SET) ? IO_LEVEL_HIGH : IO_LEVEL_LOW;
}

io_ctrl_status_t io_ctrl_toggle(io_ctrl_pin_t id)
{
    if (id >= IO_ID_MAX) {
        return IO_CTRL_ERROR;
    }

    HAL_GPIO_TogglePin(io_cfg[id].port, io_cfg[id].pin);

    return IO_CTRL_OK;
}

/* ============================================================
 * 批量控制
 * ============================================================ */

void io_ctrl_power_off_all(void)
{
    /* 同时关闭所有电源 */
    HAL_GPIO_WritePin(io_cfg[IO_ID_BQ_POWER].port,
                      io_cfg[IO_ID_BQ_POWER].pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(io_cfg[IO_ID_D_POWER].port,
                      io_cfg[IO_ID_D_POWER].pin, GPIO_PIN_RESET);
}
