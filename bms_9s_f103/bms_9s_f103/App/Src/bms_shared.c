/**
 * @file    bms_shared.c
 * @brief   BMS 共享数据中心实现
 */

#include "bms_shared.h"

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief 全局共享数据实例 */
static bms_shared_t g_bms;

/* ============================================================
 * 默认参数
 * ============================================================ */

#define DEFAULT_CELL_OV_MV         4250U
#define DEFAULT_CELL_UV_MV         2800U
#define DEFAULT_DISCHARGE_OC_MA   30000U
#define DEFAULT_CHARGE_OC_MA      15000U
#define DEFAULT_OVER_TEMP_MDEG      600
#define DEFAULT_UNDER_TEMP_MDEG       0
#define DEFAULT_BALANCE_THRESH_MV   200U
#define DEFAULT_BALANCE_MIN_MV     3200U

/* ============================================================
 * 初始化
 * ============================================================ */

void bms_shared_init(void)
{
    /* 创建互斥锁 */
    g_bms.data_mutex     = osMutexNew(NULL);
    g_bms.settings_mutex = osMutexNew(NULL);

    /* 默认参数 */
    g_bms.settings.cell_ov_mv        = DEFAULT_CELL_OV_MV;
    g_bms.settings.cell_uv_mv        = DEFAULT_CELL_UV_MV;
    g_bms.settings.discharge_oc_ma   = DEFAULT_DISCHARGE_OC_MA;
    g_bms.settings.charge_oc_ma      = DEFAULT_CHARGE_OC_MA;
    g_bms.settings.over_temp_mdeg    = DEFAULT_OVER_TEMP_MDEG;
    g_bms.settings.under_temp_mdeg   = DEFAULT_UNDER_TEMP_MDEG;
    g_bms.settings.balance_thresh_mv = DEFAULT_BALANCE_THRESH_MV;
    g_bms.settings.balance_min_mv    = DEFAULT_BALANCE_MIN_MV;

    /* 初始 SOC 未知 */
    g_bms.soc_permil    = 0U;
    g_bms.ocv_mv        = 0U;
    g_bms.remaining_mah = 0;
    g_bms.prot_level    = PROT_LVL_NONE;
}

/* ============================================================
 * 指针获取
 * ============================================================ */

bms_shared_t *bms_shared_get_ptr(void)
{
    return &g_bms;
}

/* ============================================================
 * 数据锁
 * ============================================================ */

bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms)
{
    if (g_bms.data_mutex == NULL) {
        return NULL;
    }

    osStatus_t status = osMutexAcquire(g_bms.data_mutex, timeout_ms);

    return (status == osOK) ? &g_bms : NULL;
}

void bms_shared_data_unlock(void)
{
    if (g_bms.data_mutex != NULL) {
        osMutexRelease(g_bms.data_mutex);
    }
}

/* ============================================================
 * 参数锁
 * ============================================================ */

bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms)
{
    if (g_bms.settings_mutex == NULL) {
        return NULL;
    }

    osStatus_t status = osMutexAcquire(g_bms.settings_mutex, timeout_ms);

    return (status == osOK) ? &g_bms.settings : NULL;
}

void bms_shared_settings_unlock(void)
{
    if (g_bms.settings_mutex != NULL) {
        osMutexRelease(g_bms.settings_mutex);
    }
}

/* ============================================================
 * 数据更新
 * ============================================================ */

void bms_shared_update_bq_data(const bq76940_data_t *data)
{
    if (data == NULL) { return; }
    if (osMutexAcquire(g_bms.data_mutex, osWaitForever) != osOK) { return; }

    g_bms.bq_data = *data;

    osMutexRelease(g_bms.data_mutex);
}

void bms_shared_update_protection(const protection_ctx_t *ctx, prot_level_t level)
{
    if (ctx == NULL) { return; }
    if (osMutexAcquire(g_bms.data_mutex, osWaitForever) != osOK) { return; }

    g_bms.prot_ctx   = *ctx;
    g_bms.prot_level = level;

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
