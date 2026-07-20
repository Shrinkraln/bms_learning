/**
 * @file    usart_drv.h
 * @brief   BSP 层串口驱动
 * @note    封装 HAL UART，提供中断驱动的环形缓冲区收发
 *          - USART1: PA9/PA10, 115200-8N1（调试控制台，中断 RX）
 *          - USART2: PA2/PA3,  9600-8N1 （外部通信，轮询 TX/RX）
 */

#ifndef __BSP_USART_DRV_H
#define __BSP_USART_DRV_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置宏
 * ============================================================ */

/** @brief USART1 接收环形缓冲区大小（字节） */
#define USART1_RX_BUF_SIZE   256U

/** @brief USART2 接收环形缓冲区大小（字节） */
#define USART2_RX_BUF_SIZE   128U

/** @brief 发送超时（毫秒） */
#define USART_TX_TIMEOUT_MS  100U

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 串口编号 */
typedef enum {
    USART_ID_1 = 0U,     /**< USART1 — 调试控制台 */
    USART_ID_2 = 1U,     /**< USART2 — 外部通信   */
    USART_ID_MAX
} usart_id_t;

/** @brief 串口操作状态 */
typedef enum {
    USART_OK        = 0x00U,
    USART_ERROR     = 0x01U,
    USART_BUSY      = 0x02U,
    USART_TIMEOUT   = 0x03U,
    USART_OVERRUN   = 0x04U,  /**< 接收缓冲区溢出 */
} usart_status_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化所有串口
 * @note   必须在 MX_USART1_UART_Init() / MX_USART2_UART_Init() 之后调用
 *         - 启动 USART1 中断接收
 *         - USART2 设置为轮询模式
 */
void usart_drv_init(void);

/**
 * @brief  通过指定串口发送单个字节
 * @param  id    串口编号
 * @param  byte  要发送的字节
 * @retval 操作状态
 */
usart_status_t usart_send_byte(usart_id_t id, uint8_t byte);

/**
 * @brief  通过指定串口发送数据块
 * @param  id    串口编号
 * @param  data  数据缓冲区
 * @param  len   数据长度
 * @retval 操作状态
 */
usart_status_t usart_send(usart_id_t id, const uint8_t *data, uint16_t len);

/**
 * @brief  发送字符串（以 '\0' 结尾）
 * @param  id   串口编号
 * @param  str  字符串指针
 * @retval 操作状态
 */
usart_status_t usart_send_string(usart_id_t id, const char *str);

/**
 * @brief  从指定串口接收缓冲区读取一个字节（非阻塞）
 * @param  id    串口编号
 * @param  byte  输出字节指针
 * @retval USART_OK = 有数据可取, USART_ERROR = 缓冲区为空
 */
usart_status_t usart_recv_byte(usart_id_t id, uint8_t *byte);

/**
 * @brief  查询指定串口接收缓冲区中可读的字节数
 * @param  id  串口编号
 * @retval 可读字节数
 */
uint16_t usart_available(usart_id_t id);

/**
 * @brief  清空指定串口的接收缓冲区
 * @param  id  串口编号
 */
void usart_flush_rx(usart_id_t id);

/**
 * @brief  获取底层 HAL 句柄（供高级操作使用）
 * @param  id  串口编号
 * @retval UART_HandleTypeDef 指针，id 无效时返回 NULL
 */
UART_HandleTypeDef *usart_get_handle(usart_id_t id);

/* ============================================================
 * 调试控制台便捷宏
 * ============================================================ */

/**
 * @brief  通过 USART1 发送格式化字符串（类 printf）
 * @note   需要重定向 __io_putchar() 或使用 sprintf 到缓冲区
 *         推荐使用 usart_send_string(USART_ID_1, buf)
 */
#define DBG_PRINT(str)  usart_send_string(USART_ID_1, (str))

#ifdef __cplusplus
}
#endif

#endif /* __BSP_USART_DRV_H */
