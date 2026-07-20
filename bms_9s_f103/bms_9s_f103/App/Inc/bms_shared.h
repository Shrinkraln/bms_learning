/**
 * @file    bms_shared.h
 * @brief   BMS 共享数据中心 — 多任务间的数据交换枢纽
 * @note    所有共享数据通过 FreeRTOS mutex 保护
 *          - bq_data: 最新采集数据（acquisition_task 写入，其他 task 只读）
 *          - prot_ctx: 保护状态（protection_task 写入，can_tx_task 读取）
 *          - soc/ocv: SOC 和 OCV 值（soc_task 写入，can_tx_task 读取）
 *          - settings: 运行时可配置参数（can_rx_task 写入，各 task 读取）
 */

#ifndef __APP_BMS_SHARED_H
#define __APP_BMS_SHARED_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "bq76940.h"
#include "protection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 运行时可配置参数
 * ============================================================ */

/** @brief BMS 运行参数（可通过 CAN 指令修改） */
typedef struct {
    uint16_t cell_ov_mv;        /**< 电芯过压阈值 (mV)                  */
    uint16_t cell_uv_mv;        /**< 电芯欠压阈值 (mV)                  */
    uint16_t discharge_oc_ma;   /**< 放电过流阈值 (mA)                  */
    uint16_t charge_oc_ma;      /**< 充电过流阈值 (mA)                  */
    int16_t  over_temp_mdeg;    /**< 过温阈值 (0.1°C)                   */
    int16_t  under_temp_mdeg;   /**< 欠温阈值 (0.1°C)                   */
    uint16_t balance_thresh_mv; /**< 均衡开启压差阈值 (mV)              */
    uint16_t balance_min_mv;    /**< 均衡最低电压阈值 (mV)              */
} bms_settings_t;

/* ============================================================
 * 共享数据
 * ============================================================ */

/** @brief BMS 共享数据中心 */
typedef struct {
    /* ---- 传感器数据 ---- */
    bq76940_data_t    bq_data;       /**< 最新 BQ76940 采集数据          */

    /* ---- 保护状态 ---- */
    protection_ctx_t  prot_ctx;      /**< 保护上下文                     */
    prot_level_t      prot_level;    /**< 最后检查的保护等级              */

    /* ---- SOC / OCV ---- */
    uint16_t          soc_permil;    /**< SOC (0-1000 = 0.0%-100.0%)    */
    uint16_t          ocv_mv;        /**< 开路电压估算值 (mV)            */
    int32_t           remaining_mah; /**< 剩余容量 (mAh)                */

    /* ---- 运行参数 ---- */
    bms_settings_t    settings;      /**< 可配置参数                     */

    /* ---- 同步 ---- */
    osMutexId_t       data_mutex;    /**< 数据互斥锁                    */
    osMutexId_t       settings_mutex;/**< 参数互斥锁                    */
} bms_shared_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化共享数据中心
 * @note   创建 mutex，设置默认参数
 */
void bms_shared_init(void);

/**
 * @brief  获取共享数据指针（不锁定，仅供 ISR 快速读取单个字段）
 */
bms_shared_t *bms_shared_get_ptr(void);

/**
 * @brief  锁定数据互斥锁并返回数据指针
 * @note   使用后必须调用 bms_shared_data_unlock()
 * @param  timeout_ms  超时 (ms), osWaitForever = 永久等待
 * @retval NULL = 超时, 非 NULL = 数据指针
 */
bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms);

/**
 * @brief  解锁数据互斥锁
 */
void bms_shared_data_unlock(void);

/**
 * @brief  锁定参数互斥锁并返回 settings 指针
 */
bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms);

/**
 * @brief  解锁参数互斥锁
 */
void bms_shared_settings_unlock(void);

/**
 * @brief  更新采集数据（acquisition_task 调用）
 * @param  data  BQ76940 最新数据
 */
void bms_shared_update_bq_data(const bq76940_data_t *data);

/**
 * @brief  更新保护状态（protection_task 调用）
 */
void bms_shared_update_protection(const protection_ctx_t *ctx, prot_level_t level);

/**
 * @brief  更新 SOC/OCV（soc_task 调用）
 */
void bms_shared_update_soc(uint16_t soc_permil, uint16_t ocv_mv, int32_t remaining_mah);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BMS_SHARED_H */
