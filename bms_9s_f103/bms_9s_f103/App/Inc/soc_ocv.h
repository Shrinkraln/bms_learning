/**
 * @file    soc_ocv.h
 * @brief   SOC / OCV 估算模块
 * @note    算法: 增强型安时积分 + OCV 查表校正
 *          1. 温度补偿容量: C_effective = C_nominal × 系数(T)
 *          2. 动态 OCV 校正: 连续权重 × 静置计时器
 *          3. 电流零漂自估计: EMA + 冻结/超时
 *          4. 上电 OCV 直接初始化
 *          5. OCV-SOC 表 21 点 (5% 间隔)
 *
 *          电流符号: current_ma > 0 = 充电, < 0 = 放电
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
    uint16_t  ocv_mv;           /**< 当前 OCV 参考值 (mV)             */
    int32_t   remaining_mah;    /**< 剩余容量 (mAh)                   */
    int32_t   nominal_mah;      /**< 标称容量 (mAh), 默认 20000      */
    uint32_t  last_sample_ms;   /**< 上次采样时间戳 (内部)            */
    uint32_t  rest_timer_ms;    /**< 静置累计时间 (ms)                */
    int32_t   current_offset_ma;/**< 零漂估计值 (mA)                  */
    uint32_t  rest_exit_ms;     /**< 退出静置的时间戳 (1h 超时用)    */
    uint8_t   initialized;      /**< 初始化标记                       */
} soc_ocv_ctx_t;

/* ============================================================
 * 锂离子 OCV-SOC 曲线
 * ============================================================ */

/** @brief OCV-SOC 查表条目 */
typedef struct {
    uint16_t ocv_mv;       /**< 单芯 OCV (mV)     */
    uint16_t soc_permil;    /**< 对应 SOC (0-1000) */
} ocv_soc_point_t;

/** @brief OCV-SOC 曲线点数 */
#define SOC_OCV_TABLE_SIZE  21U

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
 * @param  current_ma    当前电流 (mA), 正=充电, 负=放电
 * @param  temp_mdeg     电池温度 (0.1°C), 取最高温度传感器值
 * @param  dt_ms         距上次调用的时间 (ms)
 */
void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma,
                     int16_t temp_mdeg, uint32_t dt_ms);

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
 * @brief  OCV → SOC 查表 (线性插值)
 * @param  ocv_mv  单芯 OCV (mV)
 * @retval SOC (0-1000)
 */
uint16_t soc_ocv_lookup(uint16_t ocv_mv);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SOC_OCV_H */
