/**
 * @file    protection.h
 * @brief   BMS 保护判断模块 — 纯判断, 零 BSP 调用
 * @note    protection_check() 接收 data + settings，返回 prot_result_t。
 *          对称去抖: 12 种故障各 3 次确认进入/3 次确认恢复。
 *          紧急断电通过 request_power_off 标志返回，由 task_protect 执行。
 */

#ifndef APP_PROTECTION_H
#define APP_PROTECTION_H

#include "stm32f1xx_hal.h"
#include "bq76940.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置
 * ============================================================ */

#define PROT_FAULT_CONFIRM_CNT  3U

/* ============================================================
 * 类型定义
 * ============================================================ */

typedef enum {
    PROT_LVL_NONE     = 0U,
    PROT_LVL_WARNING  = 1U,
    PROT_LVL_ALERT    = 2U,
    PROT_LVL_FAULT    = 3U,
} prot_level_t;

typedef enum {
    FAULT_CELL_OV         = (1U << 0U),
    FAULT_CELL_UV         = (1U << 1U),
    FAULT_PACK_OV         = (1U << 2U),
    FAULT_PACK_UV         = (1U << 3U),
    FAULT_DISCHARGE_OC    = (1U << 4U),
    FAULT_CHARGE_OC       = (1U << 5U),
    FAULT_SHORT_CIRCUIT   = (1U << 6U),
    FAULT_OVER_TEMP       = (1U << 7U),
    FAULT_UNDER_TEMP      = (1U << 8U),
    FAULT_CELL_IMBALANCE  = (1U << 9U),
    FAULT_COMM_LOSS       = (1U << 10U),
    FAULT_WATCHDOG        = (1U << 11U),
} fault_code_t;

typedef enum {
    FET_BOTH_OFF = 0U,
    FET_CHG_ON   = 1U,
    FET_DSG_ON   = 2U,
    FET_BOTH_ON  = 3U,
} fet_state_t;

/** @brief 保护检查结果 */
typedef struct {
    prot_level_t level;
    uint16_t     active_faults;
    uint8_t      request_power_off;  /**< 需断电标志 (SCD/OCD) */
} prot_result_t;

/* bms_settings_t 定义在 bms_shared.h, 通过 include 顺序解析 */

/* ============================================================
 * API
 * ============================================================ */

void protection_init(void);

/**
 * @brief  执行一次保护检查 (纯判断，零 BSP 调用)
 * @param  data      BQ76940 最新采集数据
 * @param  settings  保护阈值 (栈上快照)
 * @return prot_result_t 检查结果
 */
prot_result_t protection_check(const bq76940_data_t *data,
                                const bms_settings_t *settings);

const char *protection_fault_str(fault_code_t code);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTECTION_H */
