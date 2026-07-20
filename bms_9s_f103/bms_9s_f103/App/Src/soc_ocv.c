/**
 * @file    soc_ocv.c
 * @brief   SOC / OCV 估算模块实现
 * @note    算法: 库仑计数 + OCV 校正
 *          SOC(t) = SOC(t-1) + I * dt / C_nominal
 *          当静置 (>60s, |I| < 500mA) 时用 OCV 查表校正
 */

#include "soc_ocv.h"

/* ============================================================
 * OCV-SOC 曲线 (锂离子 NMC 典型值, 25°C)
 * OCV 为单芯电压 (mV)
 * ============================================================ */

static const ocv_soc_point_t ocv_soc_table[SOC_OCV_TABLE_SIZE] = {
    { 4200U, 1000U },   /* 4.200V → 100.0% */
    { 4050U,  900U },   /* 4.050V →  90.0% */
    { 3950U,  800U },   /* 3.950V →  80.0% */
    { 3850U,  700U },   /* 3.850V →  70.0% */
    { 3750U,  600U },   /* 3.750V →  60.0% */
    { 3680U,  500U },   /* 3.680V →  50.0% */
    { 3620U,  400U },   /* 3.620V →  40.0% */
    { 3550U,  300U },   /* 3.550V →  30.0% */
    { 3450U,  200U },   /* 3.450V →  20.0% */
    { 3300U,  100U },   /* 3.300V →  10.0% */
    { 3000U,    0U },   /* 3.000V →   0.0% */
};

/* ============================================================
 * 配置
 * ============================================================ */

#define DEFAULT_NOMINAL_MAH      20000L   /**< 默认标称容量 20Ah          */
#define REST_CURRENT_THRESH_MA      500   /**< 静置电流阈值               */
#define REST_TIME_THRESH_MS      60000U   /**< 静置时间阈值 (60s)         */
#define SOC_OCV_CORR_WEIGHT         10U   /**< OCV 校正权重 (1/N)         */

/* ============================================================
 * 模块内变量
 * ============================================================ */

static soc_ocv_ctx_t ctx;

/* ============================================================
 * OCV → SOC 查表 (线性插值)
 * ============================================================ */

uint16_t soc_ocv_lookup(uint16_t ocv_mv)
{
    /* 边界处理 */
    if (ocv_mv >= ocv_soc_table[0].ocv_mv) {
        return ocv_soc_table[0].soc_permil;
    }
    if (ocv_mv <= ocv_soc_table[SOC_OCV_TABLE_SIZE - 1U].ocv_mv) {
        return ocv_soc_table[SOC_OCV_TABLE_SIZE - 1U].soc_permil;
    }

    /* 线性插值 */
    for (uint8_t i = 0U; i < (SOC_OCV_TABLE_SIZE - 1U); i++) {
        uint16_t v_hi = ocv_soc_table[i].ocv_mv;
        uint16_t v_lo = ocv_soc_table[i + 1U].ocv_mv;

        if (ocv_mv <= v_hi && ocv_mv >= v_lo) {
            int32_t soc_hi = (int32_t)ocv_soc_table[i].soc_permil;
            int32_t soc_lo = (int32_t)ocv_soc_table[i + 1U].soc_permil;
            int32_t dv     = (int32_t)(v_hi - v_lo);
            int32_t dsoc   = soc_hi - soc_lo;

            if (dv > 0) {
                return (uint16_t)(soc_hi - dsoc * (int32_t)(v_hi - ocv_mv) / dv);
            }
            return (uint16_t)soc_hi;
        }
    }

    return 500U;  /* fallback: 50% */
}

/* ============================================================
 * 初始化
 * ============================================================ */

void soc_ocv_init(int32_t nominal_mah)
{
    ctx.soc_permil     = 0U;
    ctx.ocv_mv         = 0U;
    ctx.remaining_mah  = 0;
    ctx.last_sample_ms = 0U;
    ctx.rest_start_ms  = 0U;
    ctx.initialized    = 0U;

    if (nominal_mah > 0) {
        ctx.nominal_mah = nominal_mah;
    } else {
        ctx.nominal_mah = DEFAULT_NOMINAL_MAH;
    }
}

/* ============================================================
 * SOC / OCV 更新
 * ============================================================ */

void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma, uint32_t dt_ms)
{
    /* 首次调用：用 OCV 初始化 SOC */
    if (ctx.initialized == 0U) {
        ctx.ocv_mv        = cell_min_mv;
        ctx.soc_permil    = soc_ocv_lookup(cell_min_mv);
        ctx.remaining_mah = (int32_t)((int64_t)ctx.nominal_mah
                                      * ctx.soc_permil / 1000L);
        ctx.last_sample_ms = 0U;  /* 由外部时间戳填充 */
        ctx.initialized    = 1U;
        return;
    }

    /* === 库仑计数 === */
    /* I(mA) * dt(s) = I * dt_ms / 1000 → mAh = I * dt_ms / 3600000 */
    /* remaining_mah += I(mA) * dt_ms / 3600 / 1000 */
    int64_t delta_mah = (int64_t)current_ma * (int64_t)dt_ms / 3600000L;
    ctx.remaining_mah += (int32_t)delta_mah;

    /* 钳位 */
    if (ctx.remaining_mah < 0) {
        ctx.remaining_mah = 0;
    } else if (ctx.remaining_mah > ctx.nominal_mah) {
        ctx.remaining_mah = ctx.nominal_mah;
    }

    /* SOC = remaining / nominal * 1000 */
    ctx.soc_permil = (uint16_t)((int64_t)ctx.remaining_mah * 1000L
                                / ctx.nominal_mah);

    /* === 静置检测 === */
    int32_t abs_current = (current_ma >= 0) ? current_ma : -current_ma;

    if (abs_current < REST_CURRENT_THRESH_MA) {
        /* 可能静置 */
        if (ctx.rest_start_ms == 0U) {
            ctx.rest_start_ms = ctx.last_sample_ms;
        }
    } else {
        /* 有负载，退出静置 */
        ctx.rest_start_ms = 0U;
    }

    /* === OCV 校正（满足静置条件时缓慢修正） === */
    if (ctx.rest_start_ms != 0U) {
        uint32_t rest_duration = ctx.last_sample_ms - ctx.rest_start_ms;

        if (rest_duration >= REST_TIME_THRESH_MS) {
            /* 静置 > 60s，用 OCV 校正 SOC */
            uint16_t ocv_soc = soc_ocv_lookup(cell_min_mv);

            /* 指数平滑: SOC = SOC + (OCV_SOC - SOC) / N */
            int32_t soc_corrected = (int32_t)ctx.soc_permil
                                   + ((int32_t)ocv_soc - (int32_t)ctx.soc_permil)
                                     / (int32_t)SOC_OCV_CORR_WEIGHT;
            if (soc_corrected < 0)   { soc_corrected = 0; }
            if (soc_corrected > 1000) { soc_corrected = 1000; }
            ctx.soc_permil = (uint16_t)soc_corrected;

            /* 同步剩余容量 */
            ctx.remaining_mah = (int32_t)((int64_t)ctx.nominal_mah
                                          * ctx.soc_permil / 1000L);
        }
    }

    /* 更新 OCV 参考 */
    ctx.ocv_mv = cell_min_mv;
}

/* ============================================================
 * 查询
 * ============================================================ */

uint16_t soc_ocv_get_soc(void) { return ctx.soc_permil; }
uint16_t soc_ocv_get_ocv(void) { return ctx.ocv_mv; }
int32_t soc_ocv_get_remaining_mah(void) { return ctx.remaining_mah; }
const soc_ocv_ctx_t *soc_ocv_get_ctx(void) { return &ctx; }
