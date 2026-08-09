/**
 * @file    can_drv.c
 * @brief   BSP 层 CAN 总线驱动实现
 * @note    CAN1, 500kbps (APB1=36MHz, prescaler=4, BS1=9, BS2=8)
 *          中断接收 → ring_buf，阻塞发送（轮询空闲邮箱 + 超时）
 */

#include "can_drv.h"
#include "can.h"

/* ============================================================
 * 环形缓冲区 (ring_buf)
 * ============================================================ */

static uint8_t  can_rx_buf[CAN_RX_FIFO_DEPTH * sizeof(can_msg_t)];
static ring_buf_t can_rx_fifo;

/* ============================================================
 * 初始化
 * ============================================================ */

void can_drv_init(void)
{
    ring_buf_init(&can_rx_fifo, can_rx_buf,
                  (uint16_t)sizeof(can_msg_t), CAN_RX_FIFO_DEPTH);

    /* ---- CAN 过滤器: 接收所有报文到 FIFO0 ---- */
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank           = 0;
    sFilterConfig.FilterMode           = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale          = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh         = 0x0000U;
    sFilterConfig.FilterIdLow          = 0x0000U;
    sFilterConfig.FilterMaskIdHigh     = 0x0000U;
    sFilterConfig.FilterMaskIdLow      = 0x0000U;
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    sFilterConfig.FilterActivation     = CAN_FILTER_ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan, &sFilterConfig);

    /* 使能 FIFO0 接收中断 (与 HAL_CAN_ActivateNotification 的 FIFO0 匹配) */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

    if (HAL_CAN_Start(&hcan) != HAL_OK) {
        return;
    }

    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* ============================================================
 * CAN 中断回调 — FIFO0 消息挂起
 * ============================================================ */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    can_msg_t msg;

    if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK) {
        return;
    }

    msg.id  = rx_header.StdId;
    msg.len = rx_header.DLC;
    if (msg.len > 8U) { msg.len = 8U; }
    for (uint8_t i = 0U; i < msg.len; i++) {
        msg.data[i] = rx_data[i];
    }

    (void)ring_buf_put(&can_rx_fifo, &msg);
}

/* ============================================================
 * 发送 (阻塞, 带超时)
 * ============================================================ */

uint8_t can_send(const can_msg_t *msg)
{
    if (msg == NULL || msg->len > 8U) {
        return 1U;
    }

    CAN_TxHeaderTypeDef tx_header = {0};
    tx_header.StdId     = msg->id;
    tx_header.IDE       = CAN_ID_STD;
    tx_header.RTR       = CAN_RTR_DATA;
    tx_header.DLC       = msg->len;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t start = HAL_GetTick();

    /* 轮询空闲邮箱 */
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U) {
        if ((HAL_GetTick() - start) > CAN_TX_TIMEOUT_MS) {
            return 1U;
        }
    }

    uint32_t tx_mailbox;
    if (HAL_CAN_AddTxMessage(&hcan, &tx_header, (uint8_t *)msg->data,
                              &tx_mailbox) != HAL_OK) {
        return 1U;
    }

    return 0U;
}

/* ============================================================
 * 接收 (非阻塞)
 * ============================================================ */

uint8_t can_recv(can_msg_t *msg)
{
    return ring_buf_get(&can_rx_fifo, msg);
}

uint16_t can_available(void)
{
    return ring_buf_available(&can_rx_fifo);
}

/* ============================================================
 * CAN 错误回调 — 总线关闭自动恢复
 * ============================================================ */

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan_ptr)
{
    if (hcan_ptr->ErrorCode & HAL_CAN_ERROR_BOF) {
        HAL_CAN_Stop(hcan_ptr);
        HAL_CAN_ResetError(hcan_ptr);
        HAL_CAN_Start(hcan_ptr);
    }
}
