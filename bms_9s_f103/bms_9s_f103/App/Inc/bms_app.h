/**
 * @file    bms_app.h
 * @brief   BMS 主应用 — 系统状态机、任务协调
 * @note    基于 FreeRTOS，协调采集/保护/上报流程
 *
 *          FreeRTOS 任务分配:
 *          - bmsTask (优先级 Normal, 1ms 周期):
 *            采集调度 → 保护检查 → 数据上报 → 均衡控制
 *
 *          状态机:
 *          INIT → IDLE → [CHARGE | DISCHARGE] → FAULT → SHUTDOWN
 */

#ifndef __APP_BMS_APP_H
#define __APP_BMS_APP_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "bq76940.h"
#include "protection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 系统状态
 * ============================================================ */

/** @brief BMS 系统状态机状态 */
typedef enum {
    BMS_STATE_INIT       = 0U,  /**< 初始化中                      */
    BMS_STATE_IDLE       = 1U,  /**< 空闲（无充放电）              */
    BMS_STATE_CHARGE     = 2U,  /**< 充电中                        */
    BMS_STATE_DISCHARGE  = 3U,  /**< 放电中                        */
    BMS_STATE_FAULT      = 4U,  /**< 故障状态（锁存）              */
    BMS_STATE_SHUTDOWN   = 5U,  /**< 关机                          */
} bms_state_t;

/** @brief BMS 系统完整上下文 */
typedef struct {
    bms_state_t        state;        /**< 当前系统状态               */
    bq76940_data_t     data;         /**< 最新数据快照               */
    protection_ctx_t   prot;         /**< 保护状态（从 protection 模块同步） */
    uint32_t           loop_count;   /**< 主循环计数                 */
    uint32_t           uptime_ms;    /**< 运行时间 (ms)              */
} bms_ctx_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  BMS 应用初始化
 * @note   初始化所有 BSP 模块和应用模块
 *         在 FreeRTOS 调度器启动前调用（main.c 的 MX_FREERTOS_Init 之前）
 */
void bms_app_init(void);

/**
 * @brief  BMS 主任务入口（FreeRTOS 任务函数）
 * @note   直接传给 osThreadNew() 作为任务函数
 *         周期: 10ms（100Hz）
 * @param  argument  未使用
 */
void bms_task_entry(void *argument);

/**
 * @brief  获取 BMS 全局上下文
 * @return 上下文指针（只读）
 */
const bms_ctx_t *bms_get_ctx(void);

/**
 * @brief  获取系统状态字符串
 */
const char *bms_state_str(bms_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BMS_APP_H */
