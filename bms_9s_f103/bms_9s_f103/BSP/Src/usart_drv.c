/**
 * @file    usart_drv.c
 * @brief   BSP 层串口驱动实现
 * @note    USART1: 中断接收 → 环形缓冲区（用于调试控制台）
 *          USART2: 轮询收发（用于外部设备通信）
 *
 *          HAL UART 中断回调链:
 *          USARTx_IRQHandler() → HAL_UART_IRQHandler() → HAL_UART_RxCpltCallback()
 *          stm32f1xx_it.c 中已有 USART1_IRQHandler，直接复用
 */

#include "usart_drv.h"
#include "usart.h"

#include <string.h>

/* ============================================================
 * 环形缓冲区结构
 * ============================================================ */

/** @brief 环形接收缓冲区 */
typedef struct {
    uint8_t *buffer;            /**< 数据存储区指针（指向外部数组） */
    volatile uint16_t head;     /**< 写指针（ISR 中递增） */
    volatile uint16_t tail;     /**< 读指针（主循环中递增） */
    volatile uint8_t  overrun;  /**< 溢出标志 */
    uint16_t size;              /**< 缓冲区容量 */
} ring_buf_t;

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief USART1 接收环形缓冲区 */
static uint8_t usart1_rx_data[USART1_RX_BUF_SIZE];
static ring_buf_t usart1_rb = {
    .buffer  = usart1_rx_data,
    .head    = 0U,
    .tail    = 0U,
    .overrun = 0U,
    .size    = USART1_RX_BUF_SIZE,
};

/** @brief USART2 接收环形缓冲区 */
static uint8_t usart2_rx_data[USART2_RX_BUF_SIZE];
static ring_buf_t usart2_rb = {
    .buffer  = usart2_rx_data,
    .head    = 0U,
    .tail    = 0U,
    .overrun = 0U,
    .size    = USART2_RX_BUF_SIZE,
};

/** @brief 用于中断接收的临时缓冲区（每串口一个字节） */
static volatile uint8_t usart1_rx_byte;
static volatile uint8_t usart2_rx_byte;

/* ============================================================
 * 环形缓冲区操作
 * ============================================================ */

/**
 * @brief  向环形缓冲区写入一个字节
 * @retval 0 = 成功, 1 = 溢出
 */
static uint8_t ring_buf_put(ring_buf_t *rb, uint8_t byte)
{
    uint16_t next = (uint16_t)(rb->head + 1U);

    if (next >= rb->size) {
        next = 0U;
    }

    if (next == rb->tail) {
        rb->overrun = 1U;
        return 1U;  /* 缓冲区满，丢弃 */
    }

    rb->buffer[rb->head] = byte;
    rb->head = next;

    return 0U;
}

/** @brief  从环形缓冲区读取一个字节 */
static uint8_t ring_buf_get(ring_buf_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail) {
        return 1U;  /* 缓冲区空 */
    }

    *byte = rb->buffer[rb->tail];

    uint16_t next = (uint16_t)(rb->tail + 1U);
    if (next >= rb->size) {
        next = 0U;
    }
    rb->tail = next;

    return 0U;
}

/** @brief  获取环形缓冲区中可读字节数 */
static uint16_t ring_buf_available(const ring_buf_t *rb)
{
    if (rb->head >= rb->tail) {
        return (uint16_t)(rb->head - rb->tail);
    } else {
        return (uint16_t)(rb->size - rb->tail + rb->head);
    }
}

/** @brief  清空环形缓冲区 */
static void ring_buf_flush(ring_buf_t *rb)
{
    rb->head = 0U;
    rb->tail = 0U;
    rb->overrun = 0U;
}

/* ============================================================
 * 初始化
 * ============================================================ */

void usart_drv_init(void)
{
    ring_buf_flush(&usart1_rb);
    ring_buf_flush(&usart2_rb);

    /* 启动 USART1 中断接收（每次接收 1 字节，在回调中重新触发） */
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&usart1_rx_byte, 1U);

    /* USART2 使用轮询模式，不需要启动中断接收 */
}

/* ============================================================
 * HAL UART 接收完成回调
 * ============================================================ */

/**
 * @brief  HAL 库 UART 接收完成回调
 * @note   由 HAL_UART_IRQHandler() 在接收完成后调用
 *         将接收到的字节存入环形缓冲区，并重新触发中断接收
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        ring_buf_put(&usart1_rb, usart1_rx_byte);
        /* 重新启动中断接收 */
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&usart1_rx_byte, 1U);
    }
    /* USART2 不使用中断接收 */
}

/**
 * @brief  HAL 库 UART 错误回调
 * @note   发生溢出/帧错误/噪声错误时，重新启动接收
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* 清除错误后重新启动接收 */
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&usart1_rx_byte, 1U);
    }
}

/* ============================================================
 * API — 发送
 * ============================================================ */

usart_status_t usart_send_byte(usart_id_t id, uint8_t byte)
{
    UART_HandleTypeDef *huart;

    switch (id) {
        case USART_ID_1: huart = &huart1; break;
        case USART_ID_2: huart = &huart2; break;
        default: return USART_ERROR;
    }

    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, &byte, 1U, USART_TX_TIMEOUT_MS);

    return (status == HAL_OK) ? USART_OK : USART_TIMEOUT;
}

usart_status_t usart_send(usart_id_t id, const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U) {
        return USART_ERROR;
    }

    UART_HandleTypeDef *huart;

    switch (id) {
        case USART_ID_1: huart = &huart1; break;
        case USART_ID_2: huart = &huart2; break;
        default: return USART_ERROR;
    }

    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t *)data, len, USART_TX_TIMEOUT_MS);

    return (status == HAL_OK) ? USART_OK : USART_TIMEOUT;
}

usart_status_t usart_send_string(usart_id_t id, const char *str)
{
    if (str == NULL) {
        return USART_ERROR;
    }

    uint16_t len = (uint16_t)strlen(str);

    if (len == 0U) {
        return USART_OK;
    }

    return usart_send(id, (const uint8_t *)str, len);
}

/* ============================================================
 * API — 接收
 * ============================================================ */

usart_status_t usart_recv_byte(usart_id_t id, uint8_t *byte)
{
    if (byte == NULL) {
        return USART_ERROR;
    }

    ring_buf_t *rb;

    switch (id) {
        case USART_ID_1: rb = &usart1_rb; break;
        case USART_ID_2: rb = &usart2_rb; break;
        default: return USART_ERROR;
    }

    if (ring_buf_get(rb, byte) != 0U) {
        return USART_ERROR;  /* 缓冲区为空 */
    }

    return USART_OK;
}

uint16_t usart_available(usart_id_t id)
{
    switch (id) {
        case USART_ID_1: return ring_buf_available(&usart1_rb);
        case USART_ID_2: return ring_buf_available(&usart2_rb);
        default: return 0U;
    }
}

void usart_flush_rx(usart_id_t id)
{
    switch (id) {
        case USART_ID_1: ring_buf_flush(&usart1_rb); break;
        case USART_ID_2: ring_buf_flush(&usart2_rb); break;
        default: break;
    }
}

/* ============================================================
 * API — 句柄获取
 * ============================================================ */

UART_HandleTypeDef *usart_get_handle(usart_id_t id)
{
    switch (id) {
        case USART_ID_1: return &huart1;
        case USART_ID_2: return &huart2;
        default: return NULL;
    }
}
