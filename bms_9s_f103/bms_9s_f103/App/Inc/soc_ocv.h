/**
 * @file    soc_ocv.h
 * @brief   SOC / OCV 估算模块
 * @note    算法:
 *          1. OCV 估算: 取静置后的电芯电压作为 OCV 参考
 *             - 静置条件: |电流| < 500mA 持续 > 60s
 *             - OCV = 最低电芯电压 (保守估算)
 *          2. SOC 估算: 库仑计数 + OCV 校正
 *             - 基准: OCV → SOC 查表 (锂离子 3.0V-4.2V 曲线)
 *             - 积分: SOC = SOC_base + ∫(I*dt) / C_nominal
 *             - 校正: 当满足静置条件时用 OCV 修正漂移
 *
 *          电池参数:
 *          - 标称容量: 可在初始化时配置（默认 20000mAh）
 *          - 9 串额定电压: 3.7V × 9 = 33.3V
 */

#ifndef __APP_SOC_OCV_H
#define __APP_SOC_OCV_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief SOC 估算上下文 */
typedef struct {
    uint16_t  soc_permil;       /**< SOC (0-1000 = 0.0%-100.0%)       */
    uint16_t  ocv_mv;           /**< 当前 OCV 估算 (mV)               */
    int32_t   remaining_mah;    /**< 剩余容量 (mAh)                   */
    int32_t   nominal_mah;      /**< 标称容量 (mAh), 默认 20000      */
    uint32_t  last_sample_ms;   /**< 上次采样时间戳                   */
    uint32_t  rest_start_ms;    /**< 静置开始时间, 0=非静置           */
    uint8_t   initialized;      /**< OCV 初始值是否已校准             */
} soc_ocv_ctx_t;

/* ============================================================
 * 锂离子 OCV-SOC 曲线（3.0V-4.2V, 10 点简化）
 * ============================================================ */

/** @brief OCV-SOC 查表条目 */
typedef struct {
    uint16_t ocv_mv;    /**< 单芯 OCV (mV)     */
    uint16_t soc_permil; /**< 对应 SOC (0-1000) */
} ocv_soc_point_t;

/** @brief OCV-SOC 曲线点数 */
#define SOC_OCV_TABLE_SIZE  11U

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化 SOC/OCV 模块
 * @param  nominal_mah  标称容量 (mAh), 0 = 使用默认 20000mAh
 */
void soc_ocv_init(int32_t nominal_mah);

/**
 * @brief  执行一次 SOC/OCV 更新
 * @note   每 1 秒调用一次
 * @param  cell_min_mv   最低单芯电压 (mV)
 * @param  current_ma    当前电流 (mA), 充电+/放电-
 * @param  dt_ms         距上次调用的时间 (ms)
 */
void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma, uint32_t dt_ms);

/**
 * @brief  获取当前 SOC (0-1000)
 */
uint16_t soc_ocv_get_soc(void);

/**
 * @brief  获取当前 OCV 估算 (mV, 单芯)
 */
uint16_t soc_ocv_get_ocv(void);

/**
 * @brief  获取剩余容量 (mAh)
 */
int32_t soc_ocv_get_remaining_mah(void);

/**
 * @brief  获取 SOC/OCV 上下文（调试用）
 */
const soc_ocv_ctx_t *soc_ocv_get_ctx(void);

/**
 * @brief  OCV → SOC 查表
 * @param  ocv_mv  单芯 OCV (mV)
 * @retval SOC (0-1000)
 */
uint16_t soc_ocv_lookup(uint16_t ocv_mv);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SOC_OCV_H */
