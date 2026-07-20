/**
 * @file    can_drv.c
 * @brief   BSP 层 CAN 总线驱动实现
 * @note    CAN1, 500kbps (36MHz / Prescaler=4 / (1+9+8)TQ)
 *          使用 FIFO0 中断接收 (CAN1_RX1_IRQn)
 */

#include "can_drv.h"
#include "can.h"

/* ============================================================
 * 环形缓冲区
 * ============================================================ */

/** @brief CAN 消息环形缓冲区 */
typedef struct {
    can_msg_t buffer[CAN_RX_FIFO_DEPTH];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint8_t  overrun;
} can_rx_fifo_t;

static can_rx_fifo_t can_rx_fifo = {0};

/** @brief 已注册的接收回调 */
static can_rx_callback_t rx_callback = NULL;

/* ============================================================
 * 环形缓冲区操作
 * ============================================================ */

static uint8_t fifo_put(can_rx_fifo_t *f, const can_msg_t *msg)
{
    uint16_t next = (uint16_t)(f->head + 1U);
    if (next >= CAN_RX_FIFO_DEPTH) { next = 0U; }
    if (next == f->tail) { f->overrun = 1U; return 1U; }

    f->buffer[f->head] = *msg;
    f->head = next;
    return 0U;
}

static uint8_t fifo_get(can_rx_fifo_t *f, can_msg_t *msg)
{
    if (f->head == f->tail) { return 1U; }

    *msg = f->buffer[f->tail];
    uint16_t next = (uint16_t)(f->tail + 1U);
    if (next >= CAN_RX_FIFO_DEPTH) { next = 0U; }
    f->tail = next;
    return 0U;
}

/* ============================================================
 * 初始化
 * ============================================================ */

void can_drv_init(void)
{
    /* 清空 FIFO */
    can_rx_fifo.head = 0U;
    can_rx_fifo.tail = 0U;
    can_rx_fifo.overrun = 0U;

    /* 启动 CAN 外设 */
    HAL_CAN_Start(&hcan);

    /* 使能 FIFO0 消息挂起中断 */
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* ============================================================
 * CAN 中断回调
 * ============================================================ */

/**
 * @brief  HAL CAN FIFO0 消息挂起回调
 * @note   由 CAN1_RX1_IRQHandler() → HAL_CAN_IRQHandler() 调用
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    if (hcan_ptr->Instance != CAN1) { return; }

    CAN_RxHeaderTypeDef rx_header;
    can_msg_t msg;

    if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &rx_header, msg.data) != HAL_OK) {
        return;
    }

    /* 转换 HAL 格式 → can_msg_t */
    msg.id   = (rx_header.IDE == CAN_ID_EXT)
               ? rx_header.ExtId : rx_header.StdId;
    msg.type = (rx_header.IDE == CAN_ID_EXT) ? CAN_FRAME_EXT : CAN_FRAME_STD;
    msg.fmt  = (rx_header.RTR == CAN_RTR_REMOTE) ? CAN_FMT_REMOTE : CAN_FMT_DATA;
    msg.dlc  = rx_header.DLC;

    /* 存入环形缓冲区 */
    fifo_put(&can_rx_fifo, &msg);

    /* 调用用户回调（如已注册） */
    if (rx_callback != NULL) {
        rx_callback(&msg);
    }
}

/* ============================================================
 * 发送
 * ============================================================ */

can_drv_status_t can_send(const can_msg_t *msg)
{
    if (msg == NULL || msg->dlc > 8U) {
        return CAN_DRV_ERROR;
    }

    CAN_TxHeaderTypeDef tx_header = {0};

    /* 设置 CAN ID */
    if (msg->type == CAN_FRAME_EXT) {
        tx_header.IDE   = CAN_ID_EXT;
        tx_header.ExtId = msg->id;
    } else {
        tx_header.IDE   = CAN_ID_STD;
        tx_header.StdId = msg->id;
    }

    tx_header.RTR    = (msg->fmt == CAN_FMT_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    tx_header.DLC    = msg->dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    /* 获取空闲邮箱并发送 */
    uint32_t tx_mailbox;
    HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(&hcan, &tx_header,
                                                     msg->data, &tx_mailbox);
    if (status != HAL_OK) {
        return CAN_DRV_BUSY;
    }

    return CAN_DRV_OK;
}

/* ============================================================
 * 接收
 * ============================================================ */

can_drv_status_t can_recv(can_msg_t *msg)
{
    if (msg == NULL) {
        return CAN_DRV_ERROR;
    }

    if (fifo_get(&can_rx_fifo, msg) != 0U) {
        return CAN_DRV_ERROR;
    }

    return CAN_DRV_OK;
}

uint16_t can_available(void)
{
    if (can_rx_fifo.head >= can_rx_fifo.tail) {
        return (uint16_t)(can_rx_fifo.head - can_rx_fifo.tail);
    } else {
        return (uint16_t)(CAN_RX_FIFO_DEPTH - can_rx_fifo.tail + can_rx_fifo.head);
    }
}

void can_register_rx_callback(can_rx_callback_t cb)
{
    rx_callback = cb;
}

/* ============================================================
 * 过滤器
 * ============================================================ */

void can_set_filter(uint32_t filter_id, uint32_t mask_id, uint8_t is_ext)
{
    CAN_FilterTypeDef filter_cfg = {0};

    filter_cfg.FilterBank = 0U;
    filter_cfg.FilterMode = CAN_FILTERMODE_IDMASK;
    filter_cfg.FilterScale = is_ext ? CAN_FILTERSCALE_32BIT : CAN_FILTERSCALE_16BIT;
    filter_cfg.FilterIdHigh   = (uint16_t)(filter_id >> 16U);
    filter_cfg.FilterIdLow    = (uint16_t)(filter_id & 0xFFFFU);
    filter_cfg.FilterMaskIdHigh = (uint16_t)(mask_id >> 16U);
    filter_cfg.FilterMaskIdLow  = (uint16_t)(mask_id & 0xFFFFU);
    filter_cfg.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter_cfg.FilterActivation = CAN_FILTER_ENABLE;
    filter_cfg.SlaveStartFilterBank = 14U;

    HAL_CAN_ConfigFilter(&hcan, &filter_cfg);
}

/* ============================================================
 * CAN 错误回调
 * ============================================================ */

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan_ptr)
{
    if (hcan_ptr->Instance != CAN1) { return; }

    /* 错误恢复：检查总线关闭等状态并尝试恢复 */
    uint32_t error = HAL_CAN_GetError(hcan_ptr);

    if (error & HAL_CAN_ERROR_BOF) {
        /* 总线关闭 — 尝试重新初始化 */
        HAL_CAN_Stop(hcan_ptr);
        HAL_CAN_ResetError(hcan_ptr);
        HAL_CAN_Start(hcan_ptr);
    }
}
