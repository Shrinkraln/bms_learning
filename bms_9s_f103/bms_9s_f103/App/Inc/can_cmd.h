/**
 * @file    can_cmd.h
 * @brief   CAN 指令协议 — 接收上位机命令并分发处理
 * @note    CAN ID 分配:
 *          上行 (MCU→上位机, 周期性):
 *            0x100  电池综合状态 (总压/电流/SOC/FET/故障)
 *            0x110  电芯电压 1-4
 *            0x111  电芯电压 5-9
 *            0x120  保护状态 + 故障码
 *            0x130  SOC/OCV 数据
 *
 *          下行 (上位机→MCU, 事件驱动):
 *            0x200  查询命令 — 请求立即回传指定数据
 *            0x201  控制命令 — FET 开关/均衡/清除故障
 *            0x202  参数配置 — 修改保护阈值
 */

#ifndef __APP_CAN_CMD_H
#define __APP_CAN_CMD_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "can_drv.h"
#include "bms_shared.h"
#include "bq76940.h"
#include "protection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * CAN ID 定义
 * ============================================================ */

/** @brief 上行 CAN ID (MCU→上位机) */
#define CAN_TX_BMS_STATUS     0x100U
#define CAN_TX_CELL_VOLT1     0x110U
#define CAN_TX_CELL_VOLT2     0x111U
#define CAN_TX_PROTECTION     0x120U
#define CAN_TX_SOC_OCV        0x130U

/** @brief 下行 CAN ID (上位机→MCU) */
#define CAN_RX_QUERY          0x200U
#define CAN_RX_CONTROL        0x201U
#define CAN_RX_CONFIG         0x202U

/* ============================================================
 * 查询子命令 (0x200, Byte 0)
 * ============================================================ */

#define CAN_QUERY_ALL          0x00U  /**< 回传全部数据                 */
#define CAN_QUERY_STATUS       0x01U  /**< 回传电池状态                 */
#define CAN_QUERY_CELLS        0x02U  /**< 回传电芯电压                 */
#define CAN_QUERY_PROTECTION   0x03U  /**< 回传保护状态                 */
#define CAN_QUERY_SOC          0x04U  /**< 回传 SOC/OCV                 */

/* ============================================================
 * 控制命令 (0x201, Byte 0)
 * ============================================================ */

#define CAN_CTRL_CLEAR_FAULT   0x01U  /**< 清除所有锁存故障             */
#define CAN_CTRL_FET_CHG_ON    0x10U  /**< 充电 FET 开                  */
#define CAN_CTRL_FET_CHG_OFF   0x11U  /**< 充电 FET 关                  */
#define CAN_CTRL_FET_DSG_ON    0x20U  /**< 放电 FET 开                  */
#define CAN_CTRL_FET_DSG_OFF   0x21U  /**< 放电 FET 关                  */
#define CAN_CTRL_BALANCE_SET   0x30U  /**< 设置均衡 (Byte1-2=mask)      */
#define CAN_CTRL_BALANCE_OFF   0x31U  /**< 关闭所有均衡                 */
#define CAN_CTRL_SHUTDOWN      0xFFU  /**< 系统关机                     */

/* ============================================================
 * CAN 接收队列
 * ============================================================ */

/** @brief CAN 接收队列长度 */
#define CAN_RX_QUEUE_DEPTH     16U

/** @brief CAN 接收消息队列句柄（由 can_cmd_init 创建） */
extern osMessageQueueId_t can_rx_queue;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化 CAN 指令模块
 * @note   创建接收消息队列，注册 CAN 接收回调
 */
void can_cmd_init(void);

/**
 * @brief  CAN 接收任务（FreeRTOS 任务函数）
 * @note   阻塞在消息队列上，收到 CAN 帧后分发处理
 * @param  argument  未使用
 */
void can_cmd_task_entry(void *argument);

/**
 * @brief  处理一条 CAN 指令
 * @param  msg  CAN 消息
 */
void can_cmd_dispatch(const can_msg_t *msg);

/**
 * @brief  CAN 上报任务（FreeRTOS 任务函数）
 * @note   周期性发送 BMS 数据到上位机
 * @param  argument  未使用
 */
void can_tx_task_entry(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CAN_CMD_H */
