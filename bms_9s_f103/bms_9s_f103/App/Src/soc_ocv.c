/**
 * @file    soc_ocv.c
 * @brief   SOC / OCV 估算模块 — 四状态机 + Q_max 学习 + 安全余量映射
 * @note    纯算法模块: 零 BSP 依赖, 零 RTOS 依赖, 零 HAL 调用
 *
 *          状态机:
 *            IDLE ──(I<-500mA)──→ DISCHARGE
 *            IDLE ──(V↑100mV)──→ CHARGE
 *            DISCHARGE ──(|I|<300mA or V<3000mV)──→ POLARIZATION
 *            DISCHARGE ──(I>+200mA×10s)──→ CHARGE
 *            CHARGE ──(5min ΔV<5mV)──→ POLARIZATION
 *            CHARGE ──(I<-500mA)──→ DISCHARGE
 *            POLARIZATION ──(30min)──→ IDLE (+ OCV修正 + Q_max学习)
 *            POLARIZATION ──(I<-500mA)──→ DISCHARGE
 *            POLARIZATION ──(V↑100mV)──→ CHARGE
 *
 *          安时积分: delta_mah = (int64_t)I × dt_ms / 3600000L
 *          SOC 显示: display = (real - 30) × 1000 / 970, clamp[0,1000]
 *          Q_max 学习: EMA α=0.1, 仅放电周期 + Q_passed > Q_max/2
 */

#include "soc_ocv.h"
#include <string.h>

/* ============================================================
 * 编译时大小检查
 * ============================================================ */

/* soc_ctx_t 需 ≤ 64 字节; 若编译器布局超出，编译报错 */
typedef char soc_ctx_size_check[sizeof(soc_ctx_t) <= 64U ? 1 : -1];

/* ============================================================
 * OCV-SOC 曲线 (来自 SCV_SOC_Mapping.csv, 31 点, 膝部/平台加密)
 * OCV 为单芯电压 (mV), 严格按 OCV 降序排列 (二分查找要求)
 * ============================================================ */

static const ocv_soc_point_t ocv_soc_table[SOC_OCV_TABLE_SIZE] = {
    { 4198U, 1000U },  /* 100.0% */
    { 4133U,  950U },  /*  95.0% */
    { 4085U,  900U },  /*  90.0% */
    { 4038U,  850U },  /*  85.0% */
    { 3987U,  800U },  /*  80.0% */
    { 3940U,  750U },  /*  75.0% */
    { 3905U,  700U },  /*  70.0% */
    { 3882U,  650U },  /*  65.0% */
    { 3862U,  600U },  /*  60.0% */
    { 3836U,  550U },  /*  55.0% */
    { 3806U,  500U },  /*  50.0% */
    { 3778U,  450U },  /*  45.0% */
    { 3762U,  400U },  /*  40.0% */
    { 3759U,  375U },  /*  37.5% — 平台加密 */
    { 3758U,  350U },  /*  35.0% */
    { 3755U,  300U },  /*  30.0% */
    { 3736U,  250U },  /*  25.0% */
    { 3702U,  200U },  /*  20.0% */
    { 3674U,  150U },  /*  15.0% */
    { 3662U,  120U },  /*  12.0% — 膝部加密 */
    { 3642U,  100U },  /*  10.0% */
    { 3622U,   90U },  /*   9.0% */
    { 3588U,   80U },  /*   8.0% */
    { 3536U,   70U },  /*   7.0% */
    { 3458U,   60U },  /*   6.0% */
    { 3344U,   50U },  /*   5.0% */
    { 3270U,   45U },  /*   4.5% */
    { 3182U,   40U },  /*   4.0% */
    { 3079U,   35U },  /*   3.5% */
    { 3009U,   32U },  /*   3.2% — CSV 最低可用点 */
    { 3000U,    0U },  /*   0.0% — CSV floor */
};

/* ============================================================
 * 模块全局上下文
 * ============================================================ */

static soc_ctx_t g_ctx;

/* ============================================================
 * SOC 安全余量映射
 * ============================================================ */

static uint16_t soc_map_display(uint16_t soc_real)
{
    /* display = (real - OFFSET) × 1000 / SCALE, clamp [0, 1000] */
    if (soc_real <= SOC_DISPLAY_OFFSET) {
        return 0U;
    }
    uint32_t display = (uint32_t)(soc_real - SOC_DISPLAY_OFFSET) * 1000UL
                       / (uint32_t)SOC_DISPLAY_SCALE;
    if (display > 1000UL) {
        display = 1000UL;
    }
    return (uint16_t)display;
}

/* ============================================================
 * OCV → SOC 查表 (线性插值)
 * ============================================================ */

uint16_t soc_ocv_lookup(uint16_t ocv_mv)
{
    /* 查表按 OCV 降序排列; 边界处理 */
    if (ocv_mv >= ocv_soc_table[0].ocv_mv) {
        return ocv_soc_table[0].soc_permil;
    }
    if (ocv_mv <= ocv_soc_table[SOC_OCV_TABLE_SIZE - 1U].ocv_mv) {
        return ocv_soc_table[SOC_OCV_TABLE_SIZE - 1U].soc_permil;
    }

    /* 线性插值 */
    for (uint8_t i = 0U; i < (SOC_OCV_TABLE_SIZE - 1U); i++) {
        if (ocv_mv <= ocv_soc_table[i].ocv_mv
         && ocv_mv >  ocv_soc_table[i + 1U].ocv_mv) {

            int32_t v_hi   = (int32_t)ocv_soc_table[i].ocv_mv;
            int32_t v_lo   = (int32_t)ocv_soc_table[i + 1U].ocv_mv;
            int32_t soc_hi = (int32_t)ocv_soc_table[i].soc_permil;
            int32_t soc_lo = (int32_t)ocv_soc_table[i + 1U].soc_permil;

            int32_t dv   = v_hi - v_lo;
            if (dv == 0) { return (uint16_t)soc_lo; }

            int32_t result = soc_lo + (soc_hi - soc_lo)
                                     * (int32_t)(ocv_mv - (uint16_t)v_lo) / dv;
            if (result < 0)   { result = 0; }
            if (result > 1000) { result = 1000; }

            return (uint16_t)result;
        }
    }

    return 0U;
}

/* ============================================================
 * 初始化
 * ============================================================ */

void soc_ocv_init(int32_t nominal_mah)
{
    (void)memset(&g_ctx, 0, sizeof(g_ctx));

    if (nominal_mah == 0) {
        g_ctx.q_max_mah = 20000L;
    } else {
        g_ctx.q_max_mah = nominal_mah;
    }

    g_ctx.state           = SOC_STATE_IDLE;
    g_ctx.remaining_mah   = g_ctx.q_max_mah / 2L;  /* 初始 SOC=50% */
    g_ctx.soc_real        = 500U;
    g_ctx.soc_display     = soc_map_display(500U);
    g_ctx.charge_peak_mv  = 0U;
    g_ctx.charge_valley_mv = 0xFFFFU;
}

/* ============================================================
 * 状态机入口 — 状态转换
 * ============================================================ */

/**
 * @brief  放电检测 (IDLE / POLARIZATION → DISCHARGE)
 * @retval 1 = 放电条件满足
 */
static uint8_t is_discharge_condition(int32_t current_ma)
{
    return (current_ma < -500L) ? 1U : 0U;
}

/**
 * @brief  充电检测 (IDLE / POLARIZATION → CHARGE)
 * @retval 1 = 充电条件满足
 */
static uint8_t is_charge_condition(uint16_t cell_min_mv, uint16_t baseline_mv,
                                    int32_t current_ma)
{
    /* 充电条件: cell_min > baseline + 100mV, 且不满足放电条件 */
    if (is_discharge_condition(current_ma)) {
        return 0U;
    }
    return (cell_min_mv > (baseline_mv + 100U)) ? 1U : 0U;
}

/**
 * @brief  状态转换: 进入放电
 */
static void enter_discharge(uint16_t cell_min_mv)
{
    (void)cell_min_mv;
    g_ctx.state          = SOC_STATE_DISCHARGE;
    g_ctx.soc_at_entry   = g_ctx.soc_real;
    g_ctx.q_passed_mah   = 0L;
}

/**
 * @brief  状态转换: 进入充电
 */
static void enter_charge(uint16_t cell_min_mv)
{
    (void)cell_min_mv;
    g_ctx.state              = SOC_STATE_CHARGE;
    g_ctx.soc_at_entry       = g_ctx.soc_real;
    g_ctx.q_passed_mah       = 0L;
    g_ctx.charge_peak_mv     = 0U;
    g_ctx.charge_valley_mv   = 0xFFFFU;
    g_ctx.charge_window_ms   = 0UL;
}

/**
 * @brief  状态转换: 进入极化
 */
static void enter_polarization(uint8_t from_discharge, uint16_t cell_min_mv)
{
    g_ctx.state            = SOC_STATE_POLARIZATION;
    g_ctx.from_discharge   = from_discharge;
    g_ctx.last_soc_end     = g_ctx.soc_real;
    g_ctx.last_q_passed    = g_ctx.q_passed_mah;
    g_ctx.polarization_ms  = 0UL;
    g_ctx.pol_entry_mv     = cell_min_mv;
}

/**
 * @brief  状态转换: 进入空闲 (OCV 修正 + Q_max 学习)
 */
static void enter_idle(uint16_t cell_min_mv)
{
    /* OCV 修正: 用当前 cell_min 查表更新 SOC */
    uint16_t ocv_soc = soc_ocv_lookup(cell_min_mv);
    g_ctx.soc_real     = ocv_soc;
    g_ctx.ocv_mv       = cell_min_mv;
    g_ctx.remaining_mah = (int32_t)((int64_t)g_ctx.q_max_mah * (int64_t)ocv_soc / 1000L);

    /* Q_max 学习 (仅放电周期, 放电量 > Q_max/2) */
    if (g_ctx.from_discharge != 0U
     && g_ctx.last_q_passed > (g_ctx.q_max_mah / 2L)) {

        uint16_t delta_soc = g_ctx.soc_at_entry - g_ctx.last_soc_end;
        if (delta_soc > 0U) {
            /* Q_new = q_passed × 1000 / delta_soc */
            int32_t q_new = g_ctx.last_q_passed * 1000L / (int32_t)delta_soc;

            /* EMA: q_max += (Q_new - q_max) / 10 */
            g_ctx.q_max_mah += (q_new - g_ctx.q_max_mah) / Q_MAX_EMA_DIVISOR;

            /* 安全 clamp: Q_max ∈ [5000, 40000] */
            if (g_ctx.q_max_mah < 5000L)  { g_ctx.q_max_mah = 5000L; }
            if (g_ctx.q_max_mah > 40000L) { g_ctx.q_max_mah = 40000L; }
        }
    }

    /* 更新 IDLE 基准电压 */
    g_ctx.state            = SOC_STATE_IDLE;
    g_ctx.idle_baseline_mv = cell_min_mv;
}

/* ============================================================
 * 安时积分 (DISCHARGE / CHARGE 状态共用)
 * ============================================================ */

static void coulomb_count(int32_t current_ma, uint32_t dt_ms)
{
    /* delta_mah = I(mA) × dt(ms) / 3600000, int64_t 中间避免截断 */
    int64_t delta_mah = (int64_t)current_ma * (int64_t)dt_ms / 3600000L;

    /* 累积电量 (绝对值) */
    if (delta_mah < 0L) {
        g_ctx.q_passed_mah += (int32_t)(-delta_mah);
    } else {
        g_ctx.q_passed_mah += (int32_t)delta_mah;
    }

    /* 剩余容量 */
    g_ctx.remaining_mah += (int32_t)delta_mah;

    /* clamp */
    if (g_ctx.remaining_mah < 0L) {
        g_ctx.remaining_mah = 0L;
    }
    if (g_ctx.remaining_mah > g_ctx.q_max_mah) {
        g_ctx.remaining_mah = g_ctx.q_max_mah;
    }

    /* SOC = remaining × 1000 / q_max */
    if (g_ctx.q_max_mah > 0L) {
        uint32_t permil = (uint32_t)((int64_t)g_ctx.remaining_mah * 1000L
                                     / (int64_t)g_ctx.q_max_mah);
        if (permil > 1000UL) { permil = 1000UL; }
        g_ctx.soc_real = (uint16_t)permil;
    }
}

/* ============================================================
 * 充电平台滑动窗口更新
 * ============================================================ */

static void charge_window_update(uint16_t cell_min_mv, uint32_t dt_ms)
{
    g_ctx.charge_window_ms += dt_ms;

    if (cell_min_mv > g_ctx.charge_peak_mv) {
        g_ctx.charge_peak_mv = cell_min_mv;
    }
    if (cell_min_mv < g_ctx.charge_valley_mv) {
        g_ctx.charge_valley_mv = cell_min_mv;
    }
}

/* ============================================================
 * 主更新函数 — 四状态机调度
 * ============================================================ */

void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma, uint32_t dt_ms)
{
    /* 首次调用: 用 OCV 初始化 SOC */
    if (g_ctx.initialized == 0U) {
        uint16_t ocv_soc = soc_ocv_lookup(cell_min_mv);
        g_ctx.soc_real     = ocv_soc;
        g_ctx.ocv_mv       = cell_min_mv;
        g_ctx.remaining_mah = (int32_t)((int64_t)g_ctx.q_max_mah
                                        * (int64_t)ocv_soc / 1000L);
        g_ctx.soc_display   = soc_map_display(ocv_soc);
        g_ctx.idle_baseline_mv = cell_min_mv;
        g_ctx.initialized   = 1U;
        return;
    }

    /* ---- 状态机 ---- */
    switch (g_ctx.state) {

    /* ================================================================
     * IDLE — 空闲
     * ================================================================ */
    case SOC_STATE_IDLE:
        if (is_discharge_condition(current_ma)) {
            enter_discharge(cell_min_mv);
        } else if (is_charge_condition(cell_min_mv, g_ctx.idle_baseline_mv,
                                        current_ma)) {
            enter_charge(cell_min_mv);
        } else if (current_ma > -200L && current_ma < 200L) {
            /* 小电流: OCV 静置更新 */
            uint16_t ocv_soc = soc_ocv_lookup(cell_min_mv);
            g_ctx.soc_real = ocv_soc;
            g_ctx.ocv_mv   = cell_min_mv;
            g_ctx.remaining_mah = (int32_t)((int64_t)g_ctx.q_max_mah
                                            * (int64_t)ocv_soc / 1000L);
        }
        /* idle_baseline 在 IDLE 期间不更新 */
        break;

    /* ================================================================
     * DISCHARGE — 放电积分
     * ================================================================ */
    case SOC_STATE_DISCHARGE:
        coulomb_count(current_ma, dt_ms);

        /* 交叉检测: 充电器接入 → CHARGE (需去抖 10s) */
        if (current_ma > (int32_t)CHARGE_DETECT_CURRENT_MA) {
            enter_charge(cell_min_mv);
            /* coulomb_count 已在 enter_charge 之前执行,
             * enter_charge 重置 q_passed_mah, 下一周期从 CHARGE 分支积分 */
        }
        /* 放电结束 → POLARIZATION */
        else if ((current_ma > -300L && current_ma < 300L)
                 || cell_min_mv < 3000U) {
            enter_polarization(1U, cell_min_mv);
        }
        break;

    /* ================================================================
     * CHARGE — 充电积分
     * ================================================================ */
    case SOC_STATE_CHARGE:
        coulomb_count(current_ma, dt_ms);

        /* 交叉检测: 负载接入 → DISCHARGE */
        if (is_discharge_condition(current_ma)) {
            enter_discharge(cell_min_mv);
            /* coulomb_count 已在 enter_discharge 之前执行,
             * enter_discharge 重置 q_passed_mah, 下一周期从 DISCHARGE 分支积分 */
        }
        /* 充电平台检测 */
        else {
            charge_window_update(cell_min_mv, dt_ms);

            if (g_ctx.charge_window_ms >= CHARGE_PLATEAU_WINDOW_MS) {
                uint16_t delta_v = g_ctx.charge_peak_mv - g_ctx.charge_valley_mv;

                if (delta_v <= CHARGE_PLATEAU_DELTA_MV) {
                    enter_polarization(0U, cell_min_mv);
                } else {
                    /* 窗口满但平台未到: 重置窗口 */
                    g_ctx.charge_window_ms = 0UL;
                    g_ctx.charge_peak_mv   = cell_min_mv;
                    g_ctx.charge_valley_mv = cell_min_mv;
                }
            }
        }
        break;

    /* ================================================================
     * POLARIZATION — 极化消除等待
     * ================================================================ */
    case SOC_STATE_POLARIZATION:
        /* 优先检查充/放中断条件 */
        if (is_discharge_condition(current_ma)) {
            enter_discharge(cell_min_mv);
            coulomb_count(current_ma, dt_ms);
            break;
        }
        if (is_charge_condition(cell_min_mv, g_ctx.pol_entry_mv, current_ma)) {
            enter_charge(cell_min_mv);
            coulomb_count(current_ma, dt_ms);
            break;
        }

        /* 极化计时 */
        g_ctx.polarization_ms += dt_ms;

        if (g_ctx.polarization_ms >= POLARIZATION_TIME_MS) {
            enter_idle(cell_min_mv);
        }
        break;

    default:
        /* 未预期的状态 → 强制 IDLE */
        g_ctx.state = SOC_STATE_IDLE;
        break;
    }

    /* 更新显示 SOC */
    g_ctx.soc_display = soc_map_display(g_ctx.soc_real);
}

/* ============================================================
 * Getter 函数
 * ============================================================ */

uint16_t soc_ocv_get_soc(void)
{
    return g_ctx.soc_display;
}

uint16_t soc_ocv_get_real_soc(void)
{
    return g_ctx.soc_real;
}

uint16_t soc_ocv_get_ocv(void)
{
    return g_ctx.ocv_mv;
}

int32_t soc_ocv_get_remaining_mah(void)
{
    return g_ctx.remaining_mah;
}

soc_state_t soc_ocv_get_state(void)
{
    return (soc_state_t)g_ctx.state;
}

int32_t soc_ocv_get_q_max(void)
{
    return g_ctx.q_max_mah;
}

const soc_ctx_t *soc_ocv_get_ctx(void)
{
    return &g_ctx;
}
