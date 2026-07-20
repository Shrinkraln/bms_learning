/**
 * @file    bms_app.h
 * @brief   BMS 主应用 — 多任务创建和协调
 * @note    FreeRTOS 任务架构:
 *
 *          ┌─────────────────────────────────────────────────────┐
 *          │ Task              Priority      Period     Stack    │
 *          ├─────────────────────────────────────────────────────┤
 *          │ can_rx_task       osPriorityHigh    事件驱动  512w  │
 *          │ protection_task   osPriorityAboveNormal 10ms  256w │
 *          │ acquisition_task  osPriorityNormal    100ms  512w  │
 *          │ can_tx_task       osPriorityNormal    100ms  256w  │
 *          │ soc_task          osPriorityNormal   1000ms  512w  │
 *          │ watchdog_task     osPriorityLow       500ms  128w  │
 *          └─────────────────────────────────────────────────────┘
 *
 *          数据流:
 *          acquisition_task → [bms_shared] → can_tx_task → CAN
 *          acquisition_task → [bms_shared] → protection_task → FET/io_ctrl
 *          acquisition_task → [bms_shared] → soc_task → [bms_shared]
 *          CAN IRX → can_rx_task → 指令分发 → FET/均衡/参数/查询
 */

#ifndef __APP_BMS_APP_H
#define __APP_BMS_APP_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 任务配置
 * ============================================================ */

#define ACQ_TASK_PERIOD_MS      100U
#define PROT_TASK_PERIOD_MS      10U
#define SOC_TASK_PERIOD_MS     1000U
#define CAN_TX_PERIOD_MS        100U
#define WDG_TASK_PERIOD_MS      500U

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  BMS 应用初始化
 * @note   初始化所有模块并创建 FreeRTOS 任务
 *         在 main.c 中 MX_FREERTOS_Init() 之前调用
 */
void bms_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BMS_APP_H */
