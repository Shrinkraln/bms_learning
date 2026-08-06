/**
 * @file    can_drv.h
 * @brief   BSP 层 CAN 总线驱动
 * @note    CAN1, 500kbps, PA11(RX)/PA12(TX)
 *          - 中断驱动接收 (FIFO0) + ring_buf 环形缓冲区
 *          - 阻塞发送（带超时）
 */

#ifndef BSP_CAN_DRV_H
#define BSP_CAN_DRV_H

#include "stm32f1xx_hal.h"
#include "main.h"
#include "ring_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置宏
 * ============================================================ */

#define CAN_RX_FIFO_DEPTH    32U
#define CAN_TX_TIMEOUT_MS    50U

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief CAN 消息结构（简化版） */
typedef struct {
    uint32_t id;         /**< CAN ID (11-bit)                     */
    uint8_t  len;        /**< 数据长度 (0-8)                      */
    uint8_t  data[8];    /**< 数据内容                            */
} can_msg_t;

/* ============================================================
 * API 函数
 * ============================================================ */

void can_drv_init(void);

/**
 * @brief  发送 CAN 消息（阻塞，带超时）
 * @param  msg  消息指针
 * @retval 0=OK, 1=错误/超时
 */
uint8_t can_send(const can_msg_t *msg);

/**
 * @brief  从接收缓冲区读取一条消息（非阻塞）
 * @param  msg  消息输出指针
 * @retval 0=OK, 1=缓冲区空
 */
uint8_t can_recv(can_msg_t *msg);

/**
 * @brief  查询接收缓冲区中可读消息数
 */
uint16_t can_available(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAN_DRV_H */
