/**
 * @file    ring_buf.h
 * @brief   BSP 层 — 元素级环形缓冲区（纯头文件）
 * @note    支持 put_front 头插，用于 protect CAN 帧优先发送。
 *          仅任务上下文访问（非 ISR 安全）。
 */

#ifndef BSP_RING_BUF_H
#define BSP_RING_BUF_H

#include "stm32f1xx_hal.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief 元素级环形缓冲区 */
typedef struct {
    uint8_t *buf;          /**< 静态数组指针                           */
    uint16_t elem_size;    /**< 每个元素大小 (字节)                    */
    uint16_t capacity;     /**< 最大元素数                             */
    uint16_t head;         /**< 写入位置 (元素索引)                    */
    uint16_t tail;         /**< 读取位置 (元素索引)                    */
    uint16_t count;        /**< 当前元素数                             */
} ring_buf_t;

/* ============================================================
 * 内联操作函数
 * ============================================================ */

/**
 * @brief  初始化环形缓冲区
 * @param  rb        缓冲区实例指针
 * @param  buf       静态数组指针
 * @param  elem_size 每个元素大小 (字节)
 * @param  capacity  最大元素数
 */
static inline void ring_buf_init(ring_buf_t *rb, uint8_t *buf,
                                  uint16_t elem_size, uint16_t capacity)
{
    rb->buf       = buf;
    rb->elem_size = elem_size;
    rb->capacity  = capacity;
    rb->head      = 0U;
    rb->tail      = 0U;
    rb->count     = 0U;
}

/**
 * @brief  获取当前可用元素数
 */
static inline uint16_t ring_buf_available(const ring_buf_t *rb)
{
    return rb->count;
}

/**
 * @brief  尾插元素
 * @note   满时丢弃最旧元素（tail 前进），新元素写入 head 位置
 * @retval 0 = OK, 1 = 发生过丢弃（满）
 */
static inline uint8_t ring_buf_put(ring_buf_t *rb, const void *elem)
{
    uint8_t dropped = 0U;

    if (rb->count == rb->capacity) {
        /* 满：丢弃最旧元素 */
        rb->tail = (rb->tail + 1U) % rb->capacity;
        rb->count--;
        dropped = 1U;
    }

    /* 写入新元素到 head */
    (void)memcpy(&rb->buf[rb->head * rb->elem_size], elem, rb->elem_size);
    rb->head = (rb->head + 1U) % rb->capacity;
    rb->count++;

    return dropped;
}

/**
 * @brief  头插元素（protect 帧优先发送）
 * @note   满时丢弃最新元素（head 位置不变），写入到 head-1 逻辑队首
 * @retval 0 = OK, 1 = 发生过丢弃（满）
 */
static inline uint8_t ring_buf_put_front(ring_buf_t *rb, const void *elem)
{
    uint8_t dropped = 0U;

    if (rb->count == rb->capacity) {
        /* 满：丢弃最新的元素（当前 tail），tail 回退 */
        if (rb->tail == 0U) {
            rb->tail = rb->capacity - 1U;
        } else {
            rb->tail--;
        }
        rb->count--;
        dropped = 1U;
    }

    /* 回退 head 一个位置（逻辑队首） */
    if (rb->head == 0U) {
        rb->head = rb->capacity - 1U;
    } else {
        rb->head--;
    }

    /* 写入新元素到 head（逻辑队首位置） */
    (void)memcpy(&rb->buf[rb->head * rb->elem_size], elem, rb->elem_size);
    rb->count++;

    return dropped;
}

/**
 * @brief  从 tail 取元素（FIFO 出队）
 * @retval 0 = OK, 1 = 空
 */
static inline uint8_t ring_buf_get(ring_buf_t *rb, void *elem)
{
    if (rb->count == 0U) {
        return 1U;
    }

    (void)memcpy(elem, &rb->buf[rb->tail * rb->elem_size], rb->elem_size);
    rb->tail = (rb->tail + 1U) % rb->capacity;
    rb->count--;

    return 0U;
}

#ifdef __cplusplus
}
#endif

#endif /* BSP_RING_BUF_H */
