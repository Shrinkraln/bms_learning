/**
 * @file    bsp_common.h
 * @brief   BSP 层 — 通用定义（纯头文件）
 * @note    定义通用状态枚举，各 BSP 模块通过 typedef 派生自己的状态类型。
 */

#ifndef BSP_BSP_COMMON_H
#define BSP_BSP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 通用状态枚举
 * ============================================================ */

/** @brief BSP 层通用操作状态 */
typedef enum {
    BSP_OK      = 0x00U,   /**< 操作成功                              */
    BSP_ERROR   = 0x01U,   /**< 通用错误                              */
    BSP_BUSY    = 0x02U,   /**< 资源忙                                */
    BSP_TIMEOUT = 0x03U,   /**< 操作超时                              */
} bsp_status_t;

/* ============================================================
 * 模块级状态别名（各 BSP 模块 typedef 派生）
 * 注: bq76940/i2c_sw/can_drv 有自己的独立枚举，此处不派生
 * ============================================================ */

typedef bsp_status_t systick_status_t;
typedef bsp_status_t led_status_t;
typedef bsp_status_t io_ctrl_status_t;
/* timer/wdg define their own enums */

#ifdef __cplusplus
}
#endif

#endif /* BSP_BSP_COMMON_H */
