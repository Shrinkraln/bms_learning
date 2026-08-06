/**
 * @file    can_cmd.h
 * @brief   CAN 指令协议 — 纯协议解析, 零 BSP 调用
 */

#ifndef APP_CAN_CMD_H
#define APP_CAN_CMD_H

#include "stm32f1xx_hal.h"
#include "can_drv.h"
#include "bms_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * CAN ID 定义
 * ============================================================ */

#define CAN_TX_BMS_STATUS     0x100U
#define CAN_TX_BMS_FAULT      0x101U  /* 故障帧独立 ID, 不与 STATUS 碰撞 */
#define CAN_TX_CELL_VOLT_1_4  0x110U
#define CAN_TX_CELL_VOLT_5_9  0x111U
#define CAN_TX_STATUS          0x120U
#define CAN_TX_SOC_OCV        0x130U

#define CAN_RX_QUERY          0x200U
#define CAN_RX_CONTROL        0x201U
#define CAN_RX_CONFIG          0x202U

/* ============================================================
 * 控制命令
 * ============================================================ */

#define CAN_CTRL_CLEAR_FAULT   0x01U
#define CAN_CTRL_FET_CHG_ON    0x10U
#define CAN_CTRL_FET_CHG_OFF   0x11U
#define CAN_CTRL_FET_DSG_ON    0x20U
#define CAN_CTRL_FET_DSG_OFF   0x21U
#define CAN_CTRL_BALANCE_SET   0x30U
#define CAN_CTRL_BALANCE_OFF   0x31U
#define CAN_CTRL_SHUTDOWN      0xFFU

/* ============================================================
 * 动作请求类型
 * ============================================================ */

typedef enum {
    CAN_ACTION_NONE = 0,
    CAN_ACTION_CLEAR_FAULT,
    CAN_ACTION_FET_CHG_ON,
    CAN_ACTION_FET_CHG_OFF,
    CAN_ACTION_FET_DSG_ON,
    CAN_ACTION_FET_DSG_OFF,
    CAN_ACTION_BALANCE_SET,
    CAN_ACTION_BALANCE_OFF,
    CAN_ACTION_SHUTDOWN,
} can_action_t;

typedef struct {
    can_action_t action;
    uint16_t     balance_mask;
} can_action_req_t;

/* ============================================================
 * API
 * ============================================================ */

void can_cmd_init(void);

/**
 * @brief  处理一条 CAN 下行指令 (纯协议解析, 不调 BSP)
 * @param  msg           CAN 消息
 * @param  active_faults  当前活跃故障掩码 (安全条件检查)
 * @param  resp_frame    查询响应帧输出 (可为 NULL, 非 NULL 时写入响应)
 * @return can_action_req_t  任务层据此执行 BSP 操作
 */
can_action_req_t can_cmd_dispatch(const can_msg_t *msg, uint16_t active_faults,
                                   can_msg_t *resp_frame);

/**
 * @brief  生成周期性 CAN 上报帧
 * @param  bms    共享数据快照
 * @param  frames 输出帧数组 (至少 4 帧空间)
 * @return 生成的帧数
 */
uint8_t can_pub(const bms_shared_t *bms, can_msg_t *frames);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_CMD_H */
