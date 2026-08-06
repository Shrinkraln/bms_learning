/**
 * @file    bms_shared.c
 * @brief   BMS 共享数据中心实现
 */

#include "bms_shared.h"

/* ============================================================
 * 默认参数
 * ============================================================ */

#define DEFAULT_CELL_OV_MV          4250U
#define DEFAULT_CELL_UV_MV          2800U
#define DEFAULT_PACK_OV_MV         38250U
#define DEFAULT_PACK_UV_MV         25200U
#define DEFAULT_DISCHARGE_OC_MA    30000U
#define DEFAULT_CHARGE_OC_MA       15000U
#define DEFAULT_SHORT_CIRCUIT_MA   60000U
#define DEFAULT_OVER_TEMP_MDEG       600
#define DEFAULT_UNDER_TEMP_MDEG        0
#define DEFAULT_BALANCE_THRESH_MV    200U
#define DEFAULT_BALANCE_MIN_MV      3200U
#define DEFAULT_CELL_DIFF_MAX_MV     500U

/* ============================================================
 * 模块内变量
 * ============================================================ */

static bms_shared_t g_bms;

/* ============================================================
 * 初始化
 * ============================================================ */

void bms_shared_init(void)
{
    g_bms.settings.cell_ov_mv        = DEFAULT_CELL_OV_MV;
    g_bms.settings.cell_uv_mv        = DEFAULT_CELL_UV_MV;
    g_bms.settings.pack_ov_mv       = DEFAULT_PACK_OV_MV;
    g_bms.settings.pack_uv_mv       = DEFAULT_PACK_UV_MV;
    g_bms.settings.discharge_oc_ma  = DEFAULT_DISCHARGE_OC_MA;
    g_bms.settings.charge_oc_ma     = DEFAULT_CHARGE_OC_MA;
    g_bms.settings.short_circuit_ma = DEFAULT_SHORT_CIRCUIT_MA;
    g_bms.settings.over_temp_mdeg   = DEFAULT_OVER_TEMP_MDEG;
    g_bms.settings.under_temp_mdeg  = DEFAULT_UNDER_TEMP_MDEG;
    g_bms.settings.balance_thresh_mv = DEFAULT_BALANCE_THRESH_MV;
    g_bms.settings.balance_min_mv   = DEFAULT_BALANCE_MIN_MV;
    g_bms.settings.cell_diff_max_mv = DEFAULT_CELL_DIFF_MAX_MV;

    g_bms.soc_permil    = 0U;
    g_bms.ocv_mv        = 0U;
    g_bms.remaining_mah = 0;
    g_bms.prot_level    = PROT_LVL_NONE;
    g_bms.active_faults = 0x0000U;
    g_bms.latched_faults = 0x0000U;
}

/* ============================================================
 * Mutex 外部注入 (由 bms_app_init 创建)
 * ============================================================ */

void bms_shared_set_data_mutex(osMutexId_t mutex)
{
    g_bms.data_mutex = mutex;
}

void bms_shared_set_settings_mutex(osMutexId_t mutex)
{
    g_bms.settings_mutex = mutex;
}

bms_shared_t *bms_shared_get_ptr(void)
{
    return &g_bms;
}

/* ============================================================
 * 锁操作
 * ============================================================ */

bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms)
{
    if (g_bms.data_mutex == NULL) { return NULL; }
    return (osMutexAcquire(g_bms.data_mutex, timeout_ms) == osOK) ? &g_bms : NULL;
}

void bms_shared_data_unlock(void)
{
    if (g_bms.data_mutex != NULL) { osMutexRelease(g_bms.data_mutex); }
}

bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms)
{
    if (g_bms.settings_mutex == NULL) { return NULL; }
    return (osMutexAcquire(g_bms.settings_mutex, timeout_ms) == osOK)
           ? &g_bms.settings : NULL;
}

void bms_shared_settings_unlock(void)
{
    if (g_bms.settings_mutex != NULL) { osMutexRelease(g_bms.settings_mutex); }
}

/* ============================================================
 * 便捷更新
 * ============================================================ */

void bms_shared_update_bq_data(const bq76940_data_t *data)
{
    if (data == NULL) { return; }
    if (osMutexAcquire(g_bms.data_mutex, osWaitForever) != osOK) { return; }
    g_bms.battery_val = *data;
    osMutexRelease(g_bms.data_mutex);
}

void bms_shared_update_protection(prot_level_t level, uint16_t active_faults)
{
    if (osMutexAcquire(g_bms.data_mutex, osWaitForever) != osOK) { return; }
    g_bms.prot_level    = level;
    g_bms.active_faults = active_faults;
    osMutexRelease(g_bms.data_mutex);
}

void bms_shared_update_soc(uint16_t soc_permil, uint16_t ocv_mv, int32_t remaining_mah)
{
    if (osMutexAcquire(g_bms.data_mutex, osWaitForever) != osOK) { return; }
    g_bms.soc_permil    = soc_permil;
    g_bms.ocv_mv        = ocv_mv;
    g_bms.remaining_mah = remaining_mah;
    osMutexRelease(g_bms.data_mutex);
}
