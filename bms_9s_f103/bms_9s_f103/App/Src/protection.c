/**
 * @file    protection.c
 * @brief   BMS 保护判断模块实现 — 纯判断, 零 BSP 调用
 * @note    对称去抖: 12 种故障各 3 次确认/恢复
 *          所有阈值从 bms_settings_t 参数读取
 */

#include "bms_shared.h"  /* 先包含 bms_shared.h: 定义 bms_settings_t → 然后 protection.h */

/* ============================================================
 * 模块内变量
 * ============================================================ */

static uint8_t fault_confirm_cnt[12] = { 0U };

/* ============================================================
 * 初始化
 * ============================================================ */

void protection_init(void)
{
    for (uint8_t i = 0U; i < 12U; i++) {
        fault_confirm_cnt[i] = 0U;
    }
}

/* ============================================================
 * 故障码描述
 * ============================================================ */

const char *protection_fault_str(fault_code_t code)
{
    switch (code) {
        case FAULT_CELL_OV:        return "CELL_OV";
        case FAULT_CELL_UV:        return "CELL_UV";
        case FAULT_PACK_OV:        return "PACK_OV";
        case FAULT_PACK_UV:        return "PACK_UV";
        case FAULT_DISCHARGE_OC:   return "DISCHARGE_OC";
        case FAULT_CHARGE_OC:      return "CHARGE_OC";
        case FAULT_SHORT_CIRCUIT:  return "SHORT_CIRCUIT";
        case FAULT_OVER_TEMP:      return "OVER_TEMP";
        case FAULT_UNDER_TEMP:     return "UNDER_TEMP";
        case FAULT_CELL_IMBALANCE: return "CELL_IMBALANCE";
        case FAULT_COMM_LOSS:      return "COMM_LOSS";
        case FAULT_WATCHDOG:       return "WATCHDOG";
        default:                   return "UNKNOWN";
    }
}

/* ============================================================
 * 故障确认 (对称去抖: 3 次进入 / 3 次恢复)
 * ============================================================ */

static uint8_t fault_confirm(uint8_t index, uint8_t triggered)
{
    if (triggered) {
        if (fault_confirm_cnt[index] < PROT_FAULT_CONFIRM_CNT) {
            fault_confirm_cnt[index]++;
        }
    } else {
        if (fault_confirm_cnt[index] > 0U) {
            fault_confirm_cnt[index]--;
        }
    }
    return (fault_confirm_cnt[index] >= PROT_FAULT_CONFIRM_CNT) ? 1U : 0U;
}

/* ============================================================
 * 主检查函数 (纯判断, 零 BSP 调用)
 * ============================================================ */

prot_result_t protection_check(const bq76940_data_t *data,
                                const bms_settings_t *settings)
{
    prot_result_t result = { PROT_LVL_NONE, 0x0000U, 0U };
    uint16_t new_faults = 0x0000U;

    if (data == NULL || settings == NULL) {
        return result;
    }

    /* ---- 检查数据有效性 ---- */
    if (!data->cells.valid || !data->temps.valid) {
        new_faults |= FAULT_COMM_LOSS;
    }

    /* ---- 电芯过压 ---- */
    if (data->cells.valid && settings->cell_ov_mv > 0U &&
        data->cells.max_mv > settings->cell_ov_mv) {
        new_faults |= FAULT_CELL_OV;
    }

    /* ---- 电芯欠压 ---- */
    if (data->cells.valid && settings->cell_uv_mv > 0U &&
        data->cells.min_mv < settings->cell_uv_mv) {
        new_faults |= FAULT_CELL_UV;
    }

    /* ---- 电芯压差 ---- */
    if (data->cells.valid && settings->cell_diff_max_mv > 0U &&
        data->cells.diff_mv > settings->cell_diff_max_mv) {
        new_faults |= FAULT_CELL_IMBALANCE;
    }

    /* ---- 总压 ---- */
    if (data->cells.valid) {
        if (settings->pack_ov_mv > 0U && data->cells.total_mv > settings->pack_ov_mv) {
            new_faults |= FAULT_PACK_OV;
        }
        if (settings->pack_uv_mv > 0U && data->cells.total_mv < settings->pack_uv_mv) {
            new_faults |= FAULT_PACK_UV;
        }
    }

    /* ---- 过流 / 短路 ---- */
    if (data->current.valid) {
        uint16_t scd_ma   = settings->short_circuit_ma;
        uint16_t oc_disch = settings->discharge_oc_ma;
        uint16_t oc_chg   = settings->charge_oc_ma;

        if (scd_ma > 0U && data->current.current_ma > (int32_t)scd_ma) {
            new_faults |= FAULT_SHORT_CIRCUIT;
        } else if (oc_disch > 0U && data->current.current_ma > (int32_t)oc_disch) {
            new_faults |= FAULT_DISCHARGE_OC;
        } else if (oc_chg > 0U && data->current.current_ma < -(int32_t)oc_chg) {
            new_faults |= FAULT_CHARGE_OC;
        }
    }

    /* ---- 温度保护 ---- */
    if (data->temps.valid) {
        for (uint8_t i = 0U; i < BQ76940_TS_COUNT; i++) {
            if (settings->over_temp_mdeg  != 0 &&
                data->temps.ts_mdeg_c[i] > settings->over_temp_mdeg) {
                new_faults |= FAULT_OVER_TEMP;
            }
            if (settings->under_temp_mdeg != 0 &&
                data->temps.ts_mdeg_c[i] < settings->under_temp_mdeg) {
                new_faults |= FAULT_UNDER_TEMP;
            }
        }
    }

    /* ---- 硬件故障合并 ---- */
    if (data->faults.scd)  { new_faults |= FAULT_SHORT_CIRCUIT; }
    if (data->faults.ocd)  { new_faults |= FAULT_DISCHARGE_OC; }
    if (data->faults.oc_c) { new_faults |= FAULT_CHARGE_OC; }
    if (data->faults.ov)   { new_faults |= FAULT_CELL_OV; }
    if (data->faults.uv)   { new_faults |= FAULT_CELL_UV; }
    if (data->faults.ot)   { new_faults |= FAULT_OVER_TEMP; }
    if (data->faults.ut)   { new_faults |= FAULT_UNDER_TEMP; }

    /* ---- 确认故障（对称去抖） ---- */
    uint16_t confirmed = 0x0000U;
    for (uint8_t i = 0U; i < 12U; i++) {
        uint16_t mask = (uint16_t)(1U << i);
        if (fault_confirm(i, (new_faults & mask) ? 1U : 0U)) {
            confirmed |= mask;
        }
    }

    result.active_faults = confirmed;

    /* ---- 故障分级 ---- */
    if (confirmed & (FAULT_SHORT_CIRCUIT | FAULT_DISCHARGE_OC)) {
        result.level = PROT_LVL_FAULT;
        result.request_power_off = 1U;
    } else if (confirmed & (FAULT_CELL_OV | FAULT_CELL_UV |
                            FAULT_PACK_OV | FAULT_PACK_UV |
                            FAULT_CHARGE_OC)) {
        result.level = PROT_LVL_ALERT;
    } else if (confirmed & (FAULT_OVER_TEMP | FAULT_UNDER_TEMP |
                            FAULT_CELL_IMBALANCE)) {
        result.level = PROT_LVL_WARNING;
    } else {
        result.level = PROT_LVL_NONE;
    }

    return result;
}
