/**
 * @file    soc_ocv.h
 * @brief   SOC / OCV 估算模块 — 四状态机 + Q_max 学习
 * @note    状态机: IDLE → DISCHARGE/CHARGE → POLARIZATION → IDLE
 *          - 充/放电期间: 安时积分 (int64_t 中间精度)
 *          - 极化消除后: OCV 查表修正 (30 分钟等待)
 *          - Q_max: 深度放电后 EMA 自学习 (α=0.1)
 *          - SOC 显示: 底部 3% 安全余量映射
 *          - 零 BSP 依赖, 零 RTOS 依赖 — 纯算法模块
 *
 *          电流符号: current_ma > 0 = 充电, < 0 = 放电
 */

#ifndef APP_SOC_OCV_H
#define APP_SOC_OCV_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置宏
 * ============================================================ */

#define SOC_DISPLAY_OFFSET      30      /**< 底部隐藏 3% (30‰)              */
#define SOC_DISPLAY_SCALE       970     /**< 映射分母                        */
#define Q_MAX_EMA_DIVISOR       10      /**< Q_max EMA 除数 (α=0.1)         */
#define POLARIZATION_TIME_MS    1800000UL /**< 极化消除时间 (30 分钟)        */
#define CHARGE_PLATEAU_DELTA_MV 5U      /**< 充电平台 ΔV 阈值 (mV)          */
#define CHARGE_PLATEAU_WINDOW_MS 300000UL /**< 充电平台检测窗口 (5 分钟)     */
#define CHARGE_DETECT_CURRENT_MA 200    /**< 充电器接入检测电流 (mA)         */
#define CHARGE_DETECT_DEBOUNCE_S 10U    /**< 充电器接入去抖时间 (秒)         */

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief SOC 状态 */
typedef enum {
    SOC_STATE_IDLE          = 0U,
    SOC_STATE_DISCHARGE     = 1U,
    SOC_STATE_CHARGE        = 2U,
    SOC_STATE_POLARIZATION  = 3U,
} soc_state_t;

/** @brief OCV-SOC 查表条目 */
typedef struct {
    uint16_t ocv_mv;       /**< 单芯 OCV (mV)     */
    uint16_t soc_permil;   /**< 对应 SOC (0-1000) */
} ocv_soc_point_t;

/** @brief OCV-SOC 曲线点数 */
#define SOC_OCV_TABLE_SIZE  31U

/** @brief SOC 估算上下文 (≤64B) */
typedef struct {
    /* 输出 */
    uint16_t     soc_display;       /**< 显示 SOC [0,1000]                  */
    uint16_t     soc_real;          /**< 真实 SOC [0,1000]                  */
    uint16_t     ocv_mv;            /**< OCV 参考值 (mV)                    */
    int32_t      remaining_mah;     /**< 剩余容量 (mAh)                     */

    /* Q_max */
    int32_t      q_max_mah;         /**< 学习到的满容量 (mAh)               */
    int32_t      q_passed_mah;      /**< 本周期累计电量 (mAh)               */

    /* 状态机 */
    uint8_t      state;             /**< soc_state_t, 显式 uint8 压缩       */
    uint16_t     idle_baseline_mv;  /**< IDLE 基准电压 (仅 POL→IDLE 更新)   */
    uint16_t     pol_entry_mv;      /**< 进入 POL 时的 cell_min (mV)        */
    uint16_t     soc_at_entry;      /**< 进入充/放时的 SOC (‰)              */
    uint32_t     polarization_ms;   /**< 极化计时器 (ms)                    */
    uint8_t      from_discharge;    /**< 0=充电周期, 1=放电周期             */

    /* Q_max 学习历史 */
    uint16_t     last_soc_end;      /**< 充放结束时的 SOC (‰)               */
    int32_t      last_q_passed;     /**< 充放周期的累计电量 (mAh)           */

    /* 充电平台检测 (5min 滑动窗口) */
    uint16_t     charge_peak_mv;    /**< 窗口内最高电压 (mV)                */
    uint16_t     charge_valley_mv;  /**< 窗口内最低电压 (mV)                */
    uint32_t     charge_window_ms;  /**< 窗口累计时间 (ms)                  */

    uint8_t      initialized;       /**< 初始化标记                         */
} soc_ctx_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化 SOC/OCV 模块
 * @param  nominal_mah  标称容量 (mAh), 0 = 使用默认 20000mAh
 */
void soc_ocv_init(int32_t nominal_mah);

/**
 * @brief  执行一次 SOC/OCV 更新 (四状态机)
 * @note   每 ~1 秒调用一次
 * @param  cell_min_mv   最低单芯电压 (mV)
 * @param  current_ma    当前电流 (mA), 正=充电, 负=放电
 * @param  dt_ms         距上次调用的时间 (ms)
 */
void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma, uint32_t dt_ms);

/** @brief 获取映射后显示 SOC (0-1000), 底部 3% 隐藏 */
uint16_t soc_ocv_get_soc(void);

/** @brief 获取真实 SOC (0-1000) */
uint16_t soc_ocv_get_real_soc(void);

/** @brief 获取当前 OCV 估算 (mV, 单芯) */
uint16_t soc_ocv_get_ocv(void);

/** @brief 获取剩余容量 (mAh) */
int32_t soc_ocv_get_remaining_mah(void);

/** @brief 获取当前状态 */
soc_state_t soc_ocv_get_state(void);

/** @brief 获取学习到的 Q_max (mAh) */
int32_t soc_ocv_get_q_max(void);

/** @brief 获取 SOC/OCV 上下文（调试用） */
const soc_ctx_t *soc_ocv_get_ctx(void);

/** @brief OCV → SOC 查表 (线性插值) */
uint16_t soc_ocv_lookup(uint16_t ocv_mv);

#ifdef __cplusplus
}
#endif

#endif /* APP_SOC_OCV_H */
