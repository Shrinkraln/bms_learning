/**
 * @file    io_ctrl.h
 * @brief   BSP 层 GPIO 控制驱动
 * @note    封装所有控制 GPIO 的读写操作，上层无需关心引脚/端口细节
 *          - 所有引脚在 MX_GPIO_Init() 中已初始化为推挽输出
 *          - 本模块提供带名称的语义化访问接口
 */

#ifndef __BSP_IO_CTRL_H
#define __BSP_IO_CTRL_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief IO 控制操作状态 */
typedef enum {
    IO_CTRL_OK      = 0x00U,
    IO_CTRL_ERROR   = 0x01U,
} io_ctrl_status_t;

/** @brief 控制引脚编号 */
typedef enum {
    IO_ID_BQ_POWER    = 0U,   /**< PA0  MCU_KZ_BQ_POWER  BQ 电源控制    */
    IO_ID_D_POWER     = 1U,   /**< PA1  MCU_D_POWER      D 电源控制     */
    IO_ID_WAKE_BQ     = 2U,   /**< PA8  MCU_WAKE_BQ      唤醒 BQ        */
    IO_ID_STA_WAKE    = 3U,   /**< PB1  STA_WAKE          状态唤醒       */
    IO_ID_BQ1_ALERT   = 4U,   /**< PB2  MCU_BQ1_ART       BQ1 报警       */
    IO_ID_IN_STA      = 5U,   /**< PB12 MCU_IN_STA         输入状态       */
    IO_ID_MAX                  /**< 控制引脚总数                           */
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
 * @brief  初始化所有控制 IO（设置默认安全状态）
 * @note   应在 MX_GPIO_Init() 之后调用
 *         - 所有电源控制引脚初始化为低电平（关闭）
 *         - 唤醒引脚初始化为低电平（不唤醒）
 * @retval IO_CTRL_OK
 */
io_ctrl_status_t io_ctrl_init(void);

/**
 * @brief  设置指定控制引脚输出电平
 * @param  id     引脚编号
 * @param  level  输出电平 (IO_LEVEL_LOW / IO_LEVEL_HIGH)
 * @retval IO_CTRL_OK / IO_CTRL_ERROR
 */
io_ctrl_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level);

/**
 * @brief  读取指定控制引脚当前输出电平
 * @param  id  引脚编号
 * @retval 当前电平 (0 = 低, 1 = 高)
 */
io_level_t io_ctrl_get(io_ctrl_pin_t id);

/**
 * @brief  翻转指定控制引脚电平
 * @param  id  引脚编号
 * @retval IO_CTRL_OK / IO_CTRL_ERROR
 */
io_ctrl_status_t io_ctrl_toggle(io_ctrl_pin_t id);

/**
 * @brief  关闭所有电源控制（紧急断电）
 * @note   同时拉低 BQ_POWER 和 D_POWER
 */
void io_ctrl_power_off_all(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_IO_CTRL_H */
