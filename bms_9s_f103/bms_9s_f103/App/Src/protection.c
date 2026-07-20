/**
 * @file    protection.c
 * @brief   BMS 保护判断模块实现
 * @note    保护策略:
 *          1. 硬件保护 (BQ76940) — 最快，直接控制 FET
 *          2. 软件保护 (本模块) — 冗余检查，故障确认
 *          3. 故障分级: WARNING → ALERT → FAULT
 */

#include "protection.h"
#include "io_ctrl.h"
#include "systick.h"

/* ============================================================
 * 保护阈值默认值（9 串锂离子电池）
 * ============================================================ */

#define CELL_OV_MV          4250U    /**< 单芯过压阈值 (mV)         */
#define CELL_UV_MV          2800U    /**< 单芯欠压阈值 (mV)         */
#define PACK_OV_MV          38250U   /**< 总压过压 (9×4250)         */
#define PACK_UV_MV          25200U   /**< 总压欠压 (9×2800)         */
#define DISCHARGE_OC_MA     30000U   /**< 放电过流 (mA)             */
#define CHARGE_OC_MA        15000U   /**< 充电过流 (mA)             */
#define SHORT_CIRCUIT_MA    60000U   /**< 短路 (mA)                 */
#define OVER_TEMP_MDEG      600U     /**< 过温 60.0°C               */
#define UNDER_TEMP_MDEG     0U       /**< 欠温 0.0°C                */
#define CELL_DIFF_MAX_MV    500U     /**< 电芯最大压差 (mV)         */

/* ============================================================
 * 模块内变量
 * ============================================================ */

static protection_ctx_t prot_ctx;

/** @brief 故障确认计数器（需连续 N 次确认才生效） */
static uint8_t fault_confirm_cnt[12] = { 0U };

/* ============================================================
 * 初始化
 * ============================================================ */

void protection_init(void)
{
    prot_ctx.level          = PROT_LVL_NONE;
    prot_ctx.active_faults  = 0x0000U;
    prot_ctx.latched_faults = 0x0000U;
    prot_ctx.fet            = FET_BOTH_ON;
    prot_ctx.fault_timestamp = 0U;

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
 * 故障确认 / 清除
 * ============================================================ */

/**
 * @brief  故障确认逻辑：连续 N 次触发才确认
 */
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
 * 主检查函数
 * ============================================================ */

prot_level_t protection_check(const bq76940_data_t *data)
{
    uint16_t new_faults = 0x0000U;

    if (data == NULL) {
        return prot_ctx.level;
    }

    /* ---- 检查数据有效性 ---- */
    if (!data->cells.valid || !data->temps.valid) {
        /* 通信可能丢失 — 不立即触发故障，等 BQ76940 硬件保护接管 */
        new_faults |= FAULT_COMM_LOSS;
    }

    /* ---- 电芯过压 ---- */
    if (data->cells.valid &&
        data->cells.max_mv > (CELL_OV_MV + PROT_HYSTERESIS_MV)) {
        new_faults |= FAULT_CELL_OV;
    }

    /* ---- 电芯欠压 ---- */
    if (data->cells.valid &&
        data->cells.min_mv < (CELL_UV_MV - PROT_HYSTERESIS_MV)) {
        new_faults |= FAULT_CELL_UV;
    }

    /* ---- 电芯压差 ---- */
    if (data->cells.valid &&
        data->cells.diff_mv > CELL_DIFF_MAX_MV) {
        new_faults |= FAULT_CELL_IMBALANCE;
    }

    /* ---- 总压检查 ---- */
    if (data->cells.valid) {
        if (data->cells.total_mv > PACK_OV_MV) {
            new_faults |= FAULT_PACK_OV;
        }
        if (data->cells.total_mv < PACK_UV_MV) {
            new_faults |= FAULT_PACK_UV;
        }
    }

    /* ---- 过流 / 短路 ---- */
    if (data->current.valid) {
        if (data->current.current_ma > SHORT_CIRCUIT_MA) {
            new_faults |= FAULT_SHORT_CIRCUIT;
        } else if (data->current.current_ma > DISCHARGE_OC_MA) {
            new_faults |= FAULT_DISCHARGE_OC;
        } else if (data->current.current_ma < -(int32_t)CHARGE_OC_MA) {
            new_faults |= FAULT_CHARGE_OC;
        }
    }

    /* ---- 温度保护 ---- */
    if (data->temps.valid) {
        for (uint8_t i = 0U; i < BQ76940_TS_COUNT; i++) {
            if (data->temps.ts_mdeg_c[i] > OVER_TEMP_MDEG) {
                new_faults |= FAULT_OVER_TEMP;
            }
            if (data->temps.ts_mdeg_c[i] < UNDER_TEMP_MDEG) {
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

    /* ---- 确认故障（去抖） ---- */
    uint16_t confirmed = 0x0000U;
    for (uint8_t i = 0U; i < 12U; i++) {
        uint16_t mask = (uint16_t)(1U << i);
        if (fault_confirm(i, (new_faults & mask) ? 1U : 0U)) {
            confirmed |= mask;
        }
    }

    /* ---- 故障分级 ---- */
    prot_ctx.active_faults = confirmed;

    if (confirmed & (FAULT_SHORT_CIRCUIT | FAULT_DISCHARGE_OC)) {
        /* 严重故障：立即全关 */
        prot_ctx.level = PROT_LVL_FAULT;
        prot_ctx.fet   = FET_BOTH_OFF;
        prot_ctx.latched_faults |= (confirmed
                                    & (FAULT_SHORT_CIRCUIT | FAULT_DISCHARGE_OC));
        io_ctrl_power_off_all();
    } else if (confirmed & (FAULT_CELL_OV | FAULT_CELL_UV |
                            FAULT_PACK_OV | FAULT_PACK_UV |
                            FAULT_CHARGE_OC)) {
        /* 告警：选择性关闭 */
        prot_ctx.level = PROT_LVL_ALERT;
        if (confirmed & (FAULT_CELL_OV | FAULT_PACK_OV | FAULT_CHARGE_OC)) {
            prot_ctx.fet = FET_DSG_ON;  /* 关充电，保留放电 */
        }
        if (confirmed & (FAULT_CELL_UV | FAULT_PACK_UV)) {
            prot_ctx.fet = FET_CHG_ON;  /* 关放电，保留充电 */
        }
    } else if (confirmed & (FAULT_OVER_TEMP | FAULT_UNDER_TEMP |
                            FAULT_CELL_IMBALANCE)) {
        /* 预警 */
        prot_ctx.level = PROT_LVL_WARNING;
    } else {
        /* 正常 */
        prot_ctx.level = PROT_LVL_NONE;
        prot_ctx.fet   = FET_BOTH_ON;
    }

    /* 记录故障时间戳 */
    if (prot_ctx.level >= PROT_LVL_ALERT && prot_ctx.fault_timestamp == 0U) {
        prot_ctx.fault_timestamp = bsp_tick_get();
    }

    return prot_ctx.level;
}

/* ============================================================
 * 上下文查询
 * ============================================================ */

const protection_ctx_t *protection_get_ctx(void)
{
    return &prot_ctx;
}

void protection_clear_latched(void)
{
    prot_ctx.latched_faults = 0x0000U;
    prot_ctx.fault_timestamp = 0U;

    /* 如果当前无活跃故障，恢复 FET 全开 */
    if (prot_ctx.active_faults == 0x0000U) {
        prot_ctx.level = PROT_LVL_NONE;
        prot_ctx.fet   = FET_BOTH_ON;
    }
}
