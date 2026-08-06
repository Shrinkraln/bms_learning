/**
 * @file    bms_shared.h
 * @brief   BMS 共享数据中心 — 多任务间数据交换枢纽
 * @note    双互斥锁: mutex_data (batteryval[] + prot + soc), mutex_settings (参数)
 *          App 层模块 — 不包含 BSP 依赖
 */

#ifndef APP_BMS_SHARED_H
#define APP_BMS_SHARED_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "bq76940.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 可配置参数 (掉电不保存)
 * 必须在 #include "protection.h" 之前定义, 因为 protection_check() 使用此类型
 * ============================================================ */

typedef struct {
    uint16_t cell_ov_mv;
    uint16_t cell_uv_mv;
    uint16_t pack_ov_mv;
    uint16_t pack_uv_mv;
    uint16_t discharge_oc_ma;
    uint16_t charge_oc_ma;
    uint16_t short_circuit_ma;
    int16_t  over_temp_mdeg;
    int16_t  under_temp_mdeg;
    uint16_t balance_thresh_mv;
    uint16_t balance_min_mv;
    uint16_t cell_diff_max_mv;
} bms_settings_t;

#include "protection.h"

/* ============================================================
 * 共享数据中心
 * ============================================================ */

typedef struct {
    bq76940_data_t  battery_val;
    prot_level_t    prot_level;
    uint16_t        active_faults;
    uint16_t        latched_faults;
    uint16_t        soc_permil;
    uint16_t        ocv_mv;
    int32_t         remaining_mah;
    bms_settings_t  settings;
    osMutexId_t     data_mutex;
    osMutexId_t     settings_mutex;
} bms_shared_t;

/* ============================================================
 * API
 * ============================================================ */

void bms_shared_init(void);
bms_shared_t *bms_shared_get_ptr(void);

void bms_shared_set_data_mutex(osMutexId_t mutex);
void bms_shared_set_settings_mutex(osMutexId_t mutex);

bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms);
void bms_shared_data_unlock(void);

bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms);
void bms_shared_settings_unlock(void);

void bms_shared_update_bq_data(const bq76940_data_t *data);
void bms_shared_update_protection(prot_level_t level, uint16_t active_faults);
void bms_shared_update_soc(uint16_t soc_permil, uint16_t ocv_mv, int32_t remaining_mah);

#ifdef __cplusplus
}
#endif

#endif /* APP_BMS_SHARED_H */
