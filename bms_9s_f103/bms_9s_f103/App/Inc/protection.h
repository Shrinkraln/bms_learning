/**
 * @file    protection.h
 * @brief   BMS 保护判断模块
 * @note    基于 BQ76940 硬件保护 + 软件冗余保护
 *          - 硬件保护由 BQ76940 直接执行（响应 < μs 级）
 *          - 软件保护周期性检查（< 10ms），提供第二层防护
 *          - 所有阈值可在运行时配置
 */

#ifndef __APP_PROTECTION_H
#define __APP_PROTECTION_H

#include "stm32f1xx_hal.h"
#include "bq76940.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置
 * ============================================================ */

/** @brief 软件保护检查周期 (ms) */
#define PROT_CHECK_PERIOD_MS    10U

/** @brief 故障恢复迟滞 (mV) */
#define PROT_HYSTERESIS_MV      200U

/** @brief 最大连续故障计数（确认不是瞬态干扰） */
#define PROT_FAULT_CONFIRM_CNT  3U

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 保护等级 */
typedef enum {
    PROT_LVL_NONE     = 0U,  /**< 无保护触发                     */
    PROT_LVL_WARNING  = 1U,  /**< 接近阈值，预警（不停机）       */
    PROT_LVL_ALERT    = 2U,  /**< 超出阈值，关充电或关放电       */
    PROT_LVL_FAULT    = 3U,  /**< 严重故障，全关 + 需手动清除    */
} prot_level_t;

/** @brief 故障码 */
typedef enum {
    FAULT_NONE            = 0x0000U,
    FAULT_CELL_OV         = (1U << 0U),   /**< 电芯过压           */
    FAULT_CELL_UV         = (1U << 1U),   /**< 电芯欠压           */
    FAULT_PACK_OV         = (1U << 2U),   /**< 总压过压           */
    FAULT_PACK_UV         = (1U << 3U),   /**< 总压欠压           */
    FAULT_DISCHARGE_OC    = (1U << 4U),   /**< 放电过流           */
    FAULT_CHARGE_OC       = (1U << 5U),   /**< 充电过流           */
    FAULT_SHORT_CIRCUIT   = (1U << 6U),   /**< 短路               */
    FAULT_OVER_TEMP       = (1U << 7U),   /**< 过温               */
    FAULT_UNDER_TEMP      = (1U << 8U),   /**< 欠温               */
    FAULT_CELL_IMBALANCE  = (1U << 9U),   /**< 电芯压差过大       */
    FAULT_COMM_LOSS       = (1U << 10U),  /**< BQ76940 通信丢失   */
    FAULT_WATCHDOG        = (1U << 11U),  /**< 看门狗复位标记     */
} fault_code_t;

/** @brief FET 状态 */
typedef enum {
    FET_BOTH_OFF    = 0U,  /**< 充放电 FET 全关   */
    FET_CHG_ON      = 1U,  /**< 仅充电 FET 开     */
    FET_DSG_ON      = 2U,  /**< 仅放电 FET 开     */
    FET_BOTH_ON     = 3U,  /**< 充放电 FET 全开   */
} fet_state_t;

/** @brief 保护上下文（完整状态） */
typedef struct {
    prot_level_t   level;          /**< 当前保护等级              */
    uint16_t       active_faults;  /**< 活跃故障码（位掩码）     */
    uint16_t       latched_faults; /**< 锁存故障码               */
    fet_state_t    fet;            /**< FET 状态                  */
    uint32_t       fault_timestamp;/**< 故障发生时间戳            */
} protection_ctx_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化保护模块
 */
void protection_init(void);

/**
 * @brief  执行一次保护检查
 * @note   每 PROT_CHECK_PERIOD_MS 调用一次
 *         检查电芯电压、温度、电流、通信状态
 * @param  data  BQ76940 最新数据
 * @return 当前保护等级
 */
prot_level_t protection_check(const bq76940_data_t *data);

/**
 * @brief  获取保护上下文
 * @return 保护上下文指针
 */
const protection_ctx_t *protection_get_ctx(void);

/**
 * @brief  清除所有锁存故障（需外部确认安全条件）
 */
void protection_clear_latched(void);

/**
 * @brief  获取故障码的描述字符串
 * @param  code  故障码
 * @return 描述字符串（静态，不要释放）
 */
const char *protection_fault_str(fault_code_t code);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PROTECTION_H */
