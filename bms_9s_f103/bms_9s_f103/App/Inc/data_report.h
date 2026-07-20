/**
 * @file    data_report.h
 * @brief   BMS 数据上报模块
 * @note    通过 CAN 总线和 USART1 串口上报 BMS 数据
 *          - CAN: 周期性发送电池状态、电压、温度、故障码
 *          - USART1: 调试信息输出（115200bps）
 *
 *          CAN ID 分配 (11-bit):
 *          - 0x100: 电池状态 (SOC, 电流, FET 状态)
 *          - 0x110: 电芯电压 1-4
 *          - 0x111: 电芯电压 5-8
 *          - 0x112: 电芯电压 9 + 温度
 *          - 0x120: 保护状态 + 故障码
 */

#ifndef __APP_DATA_REPORT_H
#define __APP_DATA_REPORT_H

#include "stm32f1xx_hal.h"
#include "bq76940.h"
#include "protection.h"
#include "can_drv.h"
#include "usart_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * CAN ID 定义
 * ============================================================ */

#define CAN_ID_BMS_STATUS     0x100U   /**< 电池状态                    */
#define CAN_ID_CELL_VOLT1     0x110U   /**< 电芯电压 1-4                */
#define CAN_ID_CELL_VOLT2     0x111U   /**< 电芯电压 5-8                */
#define CAN_ID_CELL_VOLT3     0x112U   /**< 电芯电压 9 + 温度           */
#define CAN_ID_PROTECTION     0x120U   /**< 保护状态 + 故障码           */

/* ============================================================
 * 上报周期 (ms)
 * ============================================================ */

#define REPORT_PERIOD_FAST    100U     /**< 快速上报周期                */
#define REPORT_PERIOD_SLOW    1000U    /**< 慢速上报周期                */

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 上报状态 */
typedef enum {
    REPORT_OK     = 0x00U,
    REPORT_ERROR  = 0x01U,
} report_status_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化数据上报模块
 */
void data_report_init(void);

/**
 * @brief  上报全部数据（CAN + 串口）
 * @note   按需调用：正常周期 100ms，故障时加速到 10ms
 * @param  data  BMS 数据快照
 * @param  prot  保护状态
 */
void data_report_all(const bq76940_data_t *data,
                      const protection_ctx_t *prot);

/**
 * @brief  仅通过 CAN 上报
 */
void data_report_can(const bq76940_data_t *data,
                      const protection_ctx_t *prot);

/**
 * @brief  仅通过串口上报（调试）
 */
void data_report_serial(const bq76940_data_t *data,
                         const protection_ctx_t *prot);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DATA_REPORT_H */
