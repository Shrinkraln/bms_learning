/**
 * @file    can_drv.h
 * @brief   BSP 层 CAN 总线驱动
 * @note    CAN1, 500kbps, PA11(RX)/PA12(TX)
 *          - 支持标准帧 (11-bit) 和扩展帧 (29-bit)
 *          - 中断驱动接收 (FIFO0) + 环形缓冲区
 *          - 阻塞发送（带超时），适用于 FreeRTOS 任务
 */

#ifndef __BSP_CAN_DRV_H
#define __BSP_CAN_DRV_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置宏
 * ============================================================ */

/** @brief CAN 接收环形缓冲区深度（消息数） */
#define CAN_RX_FIFO_DEPTH    32U

/** @brief CAN 发送超时 (ms) */
#define CAN_TX_TIMEOUT_MS    50U

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief CAN 帧类型 */
typedef enum {
    CAN_FRAME_STD = 0U,    /**< 标准帧 (11-bit ID)  */
    CAN_FRAME_EXT = 1U,    /**< 扩展帧 (29-bit ID)  */
} can_frame_type_t;

/** @brief CAN 帧格式 (兼容 HAL) */
typedef enum {
    CAN_FMT_DATA = 0U,     /**< 数据帧 */
    CAN_FMT_REMOTE = 1U,   /**< 远程帧 */
} can_frame_fmt_t;

/** @brief CAN 消息结构 */
typedef struct {
    uint32_t        id;        /**< CAN ID (11-bit 或 29-bit)      */
    can_frame_type_t type;     /**< 帧类型 (标准/扩展)             */
    can_frame_fmt_t  fmt;      /**< 帧格式 (数据/远程)             */
    uint8_t         dlc;       /**< 数据长度 (0-8)                 */
    uint8_t         data[8];   /**< 数据内容                       */
} can_msg_t;

/** @brief CAN 操作状态 */
typedef enum {
    CAN_DRV_OK       = 0x00U,
    CAN_DRV_ERROR    = 0x01U,
    CAN_DRV_BUSY     = 0x02U,
    CAN_DRV_TIMEOUT  = 0x03U,
    CAN_DRV_OVERRUN  = 0x04U,
} can_drv_status_t;

/** @brief CAN 接收回调函数类型 */
typedef void (*can_rx_callback_t)(const can_msg_t *msg);

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化 CAN 驱动
 * @note   必须在 MX_CAN_Init() 之后调用
 *         - 启动 CAN 外设
 *         - 配置接收过滤器（接收所有消息）
 *         - 使能 FIFO0 中断接收
 */
void can_drv_init(void);

/**
 * @brief  发送 CAN 消息（阻塞，带超时）
 * @param  msg  消息指针
 * @retval 操作状态
 */
can_drv_status_t can_send(const can_msg_t *msg);

/**
 * @brief  从接收缓冲区读取一条消息（非阻塞）
 * @param  msg  消息输出指针
 * @retval CAN_DRV_OK = 有数据, CAN_DRV_ERROR = 缓冲区空
 */
can_drv_status_t can_recv(can_msg_t *msg);

/**
 * @brief  查询接收缓冲区中可读消息数
 * @retval 消息数
 */
uint16_t can_available(void);

/**
 * @brief  注册接收回调（每条消息到达时调用）
 * @note   回调在 CAN IRQ 上下文中执行，需保持简短
 * @param  cb  回调函数指针, NULL = 取消注册
 */
void can_register_rx_callback(can_rx_callback_t cb);

/**
 * @brief  设置 CAN 过滤器
 * @param  filter_id   过滤 ID
 * @param  mask_id     掩码 ID
 * @param  is_ext      1 = 扩展帧, 0 = 标准帧
 */
void can_set_filter(uint32_t filter_id, uint32_t mask_id, uint8_t is_ext);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CAN_DRV_H */
