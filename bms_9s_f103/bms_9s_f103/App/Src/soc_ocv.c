/**
 * @file    soc_ocv.c
 * @brief   SOC / OCV 估算模块 — 增强型安时积分 + OCV 查表
 * @note    5 项增强:
 *          1. OCV-SOC 表 21 点 (5% 间隔) + 边界 clamp
 *          2. 温度补偿容量
 *          3. 动态 OCV 校正权重 (静置计时器状态机)
 *          4. 电流零漂自估计 (EMA + 冻结/超时)
 *          5. 上电 OCV 直接初始化
 */

#include "soc_ocv.h"

/* ============================================================
 * OCV-SOC 曲线 (NMC 典型值, 25°C, 21 点 / 5% SOC 间隔)
 * OCV 为单芯电压 (mV)
 * ============================================================ */

static const ocv_soc_point_t ocv_soc_table[SOC_OCV_TABLE_SIZE] = {
    { 4200U, 1000U },
    { 4180U,  950U },
    { 4150U,  900U },
    { 4080U,  850U },
    { 4020U,  800U },
    { 3960U,  750U },
    { 3900U,  700U },
    { 3850U,  650U },
    { 3810U,  600U },
    { 3780U,  550U },
    { 3750U,  500U },
    { 3720U,  450U },
    { 3700U,  400U },
    { 3670U,  350U },
    { 3620U,  300U },
    { 3550U,  250U },
    { 3500U,  200U },
    { 3450U,  150U },
    { 3420U,  100U },
    { 3300U,   50U },
    { 3000U,    0U },
};

/* ============================================================
 * 温度-容量系数表 (NMC 典型值)
 * 温度 > 25°C 钳位到 100%
 * ============================================================ */

static const struct {
    int16_t  temp_c;       /**< 温度 (°C)          */
    uint8_t  coeff_pct;    /**< 容量系数 (%)        */
} temp_cap_table[] = {
    { -20,  65 },
    { -10,  78 },
    {   0,  88 },
    {  10,  94 },
    {  20,  98 },
    {  25, 100 },
    {  30, 100 },  /* > 25°C 钳位 */
    {  40, 100 },
    {  50, 100 },
    {  60, 100 },
};

#define TEMP_CAP_TABLE_SIZE  (sizeof(temp_cap_table) / sizeof(temp_cap_table[0]))

/* ============================================================
 * 配置
 * ============================================================ */

#define DEFAULT_NOMINAL_MAH      20000L   /**< 默认标称容量 20Ah         */
#define REST_ENTRY_CURRENT_MA       200   /**< 进入静置 |I| 阈值          */
#define REST_EXIT_CURRENT_MA        500   /**< 退出静置 |I| 阈值          */
#define REST_ENTRY_SAMPLES            3U  /**< 进入静置需连续采样数       */
#define REST_FULL_WEIGHT_S        300U   /**< 达到满权重的静置秒数       */
#define CORR_I_THRESH_MA          500U   /**< 校正电流因子分母            */
#define CORR_I_LINEAR_MA          300U   /**< 校正电流线性区间宽度        */
#define OFFSET_REST_MIN_MS      300000U  /**< 零漂估计最小静置时间 (5min)*/
#define OFFSET_EMA_ALPHA_DIV      100U  /**< EMA 除数 (≈α=0.01 @ 1s)    */
#define OFFSET_TIMEOUT_MS     3600000U  /**< 零漂冻结超时 (1 小时)       */

/* ============================================================
 * 模块内变量
 * ============================================================ */

static soc_ocv_ctx_t ctx;

/** @brief 静置连续采样计数 (entry debounce) */
static uint8_t rest_entry_cnt = 0U;

/* ============================================================
 * 辅助: 温度 → 容量系数 (线性插值 + 边界 clamp)
 * ============================================================ */

static uint8_t temp_to_cap_coeff(int16_t temp_mdeg)
{
    int16_t temp_c = (int16_t)(temp_mdeg / 10);

    /* 边界 clamp */
    if (temp_c <= temp_cap_table[0].temp_c) {
        return temp_cap_table[0].coeff_pct;
    }
    uint8_t last = (uint8_t)(TEMP_CAP_TABLE_SIZE - 1U);
    if (temp_c >= temp_cap_table[last].temp_c) {
        return temp_cap_table[last].coeff_pct;
    }

    /* 线性插值 */
    for (uint8_t i = 0U; i < (uint8_t)(TEMP_CAP_TABLE_SIZE - 1U); i++) {
        int16_t t_lo = temp_cap_table[i].temp_c;
        int16_t t_hi = temp_cap_table[i + 1U].temp_c;
        if (temp_c >= t_lo && temp_c <= t_hi) {
            int16_t dt = (int16_t)(t_hi - t_lo);
            if (dt == 0) { return temp_cap_table[i].coeff_pct; }
            int32_t c_lo = (int32_t)temp_cap_table[i].coeff_pct;
            int32_t c_hi = (int32_t)temp_cap_table[i + 1U].coeff_pct;
            int32_t result = c_lo + (c_hi - c_lo)
                                  * (int32_t)(temp_c - t_lo) / dt;
            return (uint8_t)result;
        }
    }
    return 100U;  /* fallback */
}

/* ============================================================
 * 辅助: 动态 OCV 校正权重
 *   weight = clamp(rest_s/300,0,1) × clamp((500-|I|)/300,0,1)
 * ============================================================ */

static uint16_t calc_correction_weight(uint32_t rest_ms, int32_t abs_i_ma)
{
    /* 时间因子: 0 → 1.0 (300s 达满) */
    uint32_t weight_time = (rest_ms / 1000U) * 1000U / REST_FULL_WEIGHT_S;
    /* weight_time = rest_s*1000/300 → 满格=1000 */
    if (weight_time > 1000U) { weight_time = 1000U; }

    /* 电流因子: |I|<200→1000, |I|>500→0, 线性过渡 */
    int32_t i_factor;
    if (abs_i_ma <= REST_ENTRY_CURRENT_MA) {
        i_factor = 1000;
    } else if (abs_i_ma >= CORR_I_THRESH_MA) {
        i_factor = 0;
    } else {
        i_factor = 1000 * (int32_t)(CORR_I_THRESH_MA - abs_i_ma)
                   / (int32_t)CORR_I_LINEAR_MA;
    }

    /* weight = wt × wi / 1000 (千分比) */
    return (uint16_t)(weight_time * (uint32_t)i_factor / 1000U);
}

/* ============================================================
 * OCV → SOC 查表 (线性插值)
 * ============================================================ */

uint16_t soc_ocv_lookup(uint16_t ocv_mv)
{
    /* 边界 clamp */
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
    ctx.soc_permil        = 0U;
    ctx.ocv_mv            = 0U;
    ctx.remaining_mah     = 0;
    ctx.last_sample_ms    = 0U;
    ctx.rest_timer_ms     = 0U;
    ctx.current_offset_ma = 0;
    ctx.rest_exit_ms      = 0U;
    ctx.initialized       = 0U;
    rest_entry_cnt        = 0U;

    if (nominal_mah > 0) {
        ctx.nominal_mah = nominal_mah;
    } else {
        ctx.nominal_mah = DEFAULT_NOMINAL_MAH;
    }
}

/* ============================================================
 * SOC / OCV 更新
 * ============================================================ */

void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma,
                     int16_t temp_mdeg, uint32_t dt_ms)
{
    /* === 首次调用：OCV 直接初始化 === */
    if (ctx.initialized == 0U) {
        ctx.ocv_mv        = cell_min_mv;
        ctx.soc_permil    = soc_ocv_lookup(cell_min_mv);
        ctx.remaining_mah = (int32_t)((int64_t)ctx.nominal_mah
                                      * ctx.soc_permil / 1000L);
        ctx.last_sample_ms = 0U;
        ctx.initialized    = 1U;
        return;
    }

    /* ============================================================
     * 1. 静置计时器状态机
     * ============================================================ */
    int32_t abs_i = (current_ma >= 0) ? current_ma : -current_ma;

    if (abs_i < REST_ENTRY_CURRENT_MA) {
        /* 低电流 → 累加进入计数 */
        if (rest_entry_cnt < REST_ENTRY_SAMPLES) {
            rest_entry_cnt++;
        }
        if (rest_entry_cnt >= REST_ENTRY_SAMPLES) {
            /* 已确认静置 → 累加计时器 */
            ctx.rest_timer_ms += dt_ms;
        }
    } else if (abs_i >= REST_EXIT_CURRENT_MA) {
        /* 大电流 → 退出静置 */
        rest_entry_cnt     = 0U;
        ctx.rest_exit_ms   = ctx.last_sample_ms;  /* 记录退出时间 */
        ctx.rest_timer_ms  = 0U;
    }
    /* else: 200-500mA → hysteresis, 保持当前状态 */

    /* ============================================================
     * 2. 零漂 EMA 超时清零
     * ============================================================ */
    if (ctx.current_offset_ma != 0
        && ctx.rest_exit_ms != 0U
        && (ctx.last_sample_ms - ctx.rest_exit_ms) >= OFFSET_TIMEOUT_MS) {
        ctx.current_offset_ma = 0;
    }

    /* ============================================================
     * 3. 温度补偿有效容量
     * ============================================================ */
    uint8_t coeff = temp_to_cap_coeff(temp_mdeg);
    int32_t effective_cap = (int32_t)((int64_t)ctx.nominal_mah
                                      * coeff / 100L);

    /* ============================================================
     * 4. 库仑积分 (扣除零漂)
     * ============================================================ */
    int32_t i_corrected = current_ma - ctx.current_offset_ma;
    int64_t delta_mah = (int64_t)i_corrected * (int64_t)dt_ms / 3600000L;
    ctx.remaining_mah += (int32_t)delta_mah;

    /* 钳位 */
    if (ctx.remaining_mah < 0) {
        ctx.remaining_mah = 0;
    } else if (ctx.remaining_mah > effective_cap) {
        ctx.remaining_mah = effective_cap;
    }

    /* SOC = remaining / effective_cap * 1000 */
    if (effective_cap > 0) {
        ctx.soc_permil = (uint16_t)((int64_t)ctx.remaining_mah * 1000L
                                    / effective_cap);
    }

    /* ============================================================
     * 5. 电流零漂 EMA 估计
     * ============================================================ */
    if (abs_i < REST_ENTRY_CURRENT_MA
        && ctx.rest_timer_ms >= OFFSET_REST_MIN_MS) {
        /* EMA: offset += (I - offset) / N */
        ctx.current_offset_ma += (current_ma - ctx.current_offset_ma)
                                 / (int32_t)OFFSET_EMA_ALPHA_DIV;
    }
    /* 退出静置时: EMA 冻结 (不更新, 不清零) */

    /* ============================================================
     * 6. 动态 OCV 校正
     * ============================================================ */
    if (ctx.rest_timer_ms > 0U) {
        uint16_t weight_permil = calc_correction_weight(ctx.rest_timer_ms, abs_i);

        if (weight_permil > 0U) {
            uint16_t ocv_soc = soc_ocv_lookup(cell_min_mv);

            /* SOC = SOC + (OCV_SOC - SOC) × weight / 1000 */
            int32_t delta = ((int32_t)ocv_soc - (int32_t)ctx.soc_permil)
                           * (int32_t)weight_permil / 1000L;
            int32_t soc_new = (int32_t)ctx.soc_permil + delta;

            if (soc_new < 0)     { soc_new = 0; }
            if (soc_new > 1000)  { soc_new = 1000; }
            ctx.soc_permil = (uint16_t)soc_new;

            /* 同步剩余容量 (用 effective_cap) */
            ctx.remaining_mah = (int32_t)((int64_t)effective_cap
                                          * ctx.soc_permil / 1000L);
        }
    }

    /* ============================================================
     * 7. 更新状态
     * ============================================================ */
    ctx.ocv_mv         = cell_min_mv;
    ctx.last_sample_ms += dt_ms;
}

/* ============================================================
 * 查询
 * ============================================================ */

uint16_t soc_ocv_get_soc(void)              { return ctx.soc_permil; }
uint16_t soc_ocv_get_ocv(void)              { return ctx.ocv_mv; }
int32_t  soc_ocv_get_remaining_mah(void)    { return ctx.remaining_mah; }
const soc_ocv_ctx_t *soc_ocv_get_ctx(void) { return &ctx; }
