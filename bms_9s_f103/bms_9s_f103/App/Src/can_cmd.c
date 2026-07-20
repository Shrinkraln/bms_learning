/**
 * @file    can_cmd.c
 * @brief   CAN 指令协议实现 — 接收分发 + 周期上报
 */

#include "can_cmd.h"
#include "soc_ocv.h"
#include "io_ctrl.h"

/* ============================================================
 * 前向声明（静态辅助函数）
 * ============================================================ */

static void can_cmd_handle_query(uint8_t sub_cmd);
static void can_cmd_handle_control(const can_msg_t *msg);
static void can_cmd_handle_config(const can_msg_t *msg);
static void can_tx_status(const bms_shared_t *bms);
static void can_tx_cells(const bms_shared_t *bms);
static void can_tx_protection(const bms_shared_t *bms);
static void can_tx_soc_ocv(const bms_shared_t *bms);

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief CAN 接收消息队列 */
osMessageQueueId_t can_rx_queue = NULL;

/* ============================================================
 * CAN 接收回调（ISR 中调用 → 快速入队）
 * ============================================================ */

static void can_rx_isr_callback(const can_msg_t *msg)
{
    if (can_rx_queue != NULL && msg != NULL) {
        /* 非阻塞发送到队列，忽略满队情况 */
        osMessageQueuePut(can_rx_queue, msg, 0U, 0U);
    }
}

/* ============================================================
 * 初始化
 * ============================================================ */

void can_cmd_init(void)
{
    /* 创建消息队列 */
    can_rx_queue = osMessageQueueNew(CAN_RX_QUEUE_DEPTH,
                                      sizeof(can_msg_t), NULL);

    /* 注册 CAN 接收回调 */
    can_register_rx_callback(can_rx_isr_callback);
}

/* ============================================================
 * CAN 接收任务
 * ============================================================ */

void can_cmd_task_entry(void *argument)
{
    (void)argument;
    can_msg_t msg;

    for (;;) {
        /* 阻塞等待 CAN 消息 */
        osStatus_t status = osMessageQueueGet(can_rx_queue, &msg, NULL,
                                               osWaitForever);
        if (status == osOK) {
            can_cmd_dispatch(&msg);
        }
    }
}

/* ============================================================
 * 指令分发
 * ============================================================ */

void can_cmd_dispatch(const can_msg_t *msg)
{
    if (msg == NULL) { return; }

    switch (msg->id) {
        case CAN_RX_QUERY:
            /* 查询命令 */
            if (msg->dlc >= 1U) {
                can_cmd_handle_query(msg->data[0]);
            }
            break;

        case CAN_RX_CONTROL:
            /* 控制命令 */
            if (msg->dlc >= 1U) {
                can_cmd_handle_control(msg);
            }
            break;

        case CAN_RX_CONFIG:
            /* 参数配置 */
            if (msg->dlc >= 4U) {
                can_cmd_handle_config(msg);
            }
            break;

        default:
            break;
    }
}

/* ============================================================
 * 查询处理
 * ============================================================ */

static void can_cmd_handle_query(uint8_t sub_cmd)
{
    bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
    if (bms == NULL) { return; }

    switch (sub_cmd) {
        case CAN_QUERY_ALL:
            can_tx_status(bms);
            can_tx_cells(bms);
            can_tx_protection(bms);
            can_tx_soc_ocv(bms);
            break;
        case CAN_QUERY_STATUS:
            can_tx_status(bms);
            break;
        case CAN_QUERY_CELLS:
            can_tx_cells(bms);
            break;
        case CAN_QUERY_PROTECTION:
            can_tx_protection(bms);
            break;
        case CAN_QUERY_SOC:
            can_tx_soc_ocv(bms);
            break;
        default:
            break;
    }

    bms_shared_data_unlock();
}

/* ============================================================
 * 控制处理
 * ============================================================ */

static void can_cmd_handle_control(const can_msg_t *msg)
{
    uint8_t cmd = msg->data[0];

    switch (cmd) {
        case CAN_CTRL_CLEAR_FAULT:
            protection_clear_latched();
            break;

        case CAN_CTRL_FET_CHG_ON:
            /* 充电 FET 开 — 仅当无过压/过流充电故障 */
            {
                const protection_ctx_t *prot = protection_get_ctx();
                if (prot != NULL && !(prot->active_faults
                    & (FAULT_CELL_OV | FAULT_PACK_OV | FAULT_CHARGE_OC))) {
                    /* 安全：具体 FET 控制脚位由 io_ctrl 实现 */
                }
            }
            break;

        case CAN_CTRL_FET_DSG_ON:
            /* 放电 FET 开 */
            {
                const protection_ctx_t *prot = protection_get_ctx();
                if (prot != NULL && !(prot->active_faults
                    & (FAULT_CELL_UV | FAULT_PACK_UV
                       | FAULT_DISCHARGE_OC | FAULT_SHORT_CIRCUIT))) {
                    /* 安全 */
                }
            }
            break;

        case CAN_CTRL_BALANCE_SET:
            if (msg->dlc >= 3U) {
                uint16_t mask = (uint16_t)(((uint16_t)msg->data[1] << 8U)
                                           | msg->data[2]);
                bq76940_set_balancing(mask);
            }
            break;

        case CAN_CTRL_BALANCE_OFF:
            bq76940_balance_off();
            break;

        case CAN_CTRL_SHUTDOWN:
            bq76940_shutdown();
            break;

        default:
            break;
    }
}

/* ============================================================
 * 参数配置处理
 * ============================================================ */

static void can_cmd_handle_config(const can_msg_t *msg)
{
    /* Byte 0: 参数类型, Byte 1-2: 值 (大端) */
    uint8_t  param = msg->data[0];
    uint16_t value = (uint16_t)(((uint16_t)msg->data[1] << 8U)
                                | msg->data[2]);

    bms_settings_t *s = bms_shared_settings_lock(osWaitForever);
    if (s == NULL) { return; }

    switch (param) {
        case 0x01U: s->cell_ov_mv        = value; break;
        case 0x02U: s->cell_uv_mv        = value; break;
        case 0x03U: s->discharge_oc_ma   = value; break;
        case 0x04U: s->charge_oc_ma      = value; break;
        case 0x05U: s->balance_thresh_mv = value; break;
        case 0x06U: s->balance_min_mv    = value; break;
        default: break;
    }

    bms_shared_settings_unlock();
}

/* ============================================================
 * CAN 上报任务
 * ============================================================ */

void can_tx_task_entry(void *argument)
{
    (void)argument;
    uint32_t loop = 0U;

    for (;;) {
        bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
        if (bms == NULL) { continue; }

        /* 每 100ms: 状态 + 电压 + SOC */
        can_tx_status(bms);
        can_tx_cells(bms);
        can_tx_soc_ocv(bms);

        /* 每 500ms (5 次循环): 故障状态 */
        if ((loop % 5U) == 0U) {
            can_tx_protection(bms);
        }

        bms_shared_data_unlock();

        loop++;
        osDelay(100U);
    }
}

/* ============================================================
 * CAN 上报函数
 * ============================================================ */

void can_tx_status(const bms_shared_t *bms)
{
    can_msg_t msg;
    msg.id   = CAN_TX_BMS_STATUS;
    msg.type = CAN_FRAME_STD;
    msg.fmt  = CAN_FMT_DATA;
    msg.dlc  = 8U;

    uint32_t total_mv = bms->bq_data.cells.total_mv;
    int32_t  current  = bms->bq_data.current.current_ma;

    msg.data[0] = (uint8_t)((total_mv >> 8U) & 0xFFU);
    msg.data[1] = (uint8_t)(total_mv         & 0xFFU);
    msg.data[2] = (uint8_t)((current  >> 8U) & 0xFFU);
    msg.data[3] = (uint8_t)(current          & 0xFFU);
    msg.data[4] = (uint8_t)(bms->soc_permil / 10U);  /* 0-100% */
    msg.data[5] = (uint8_t)(bms->prot_level);
    msg.data[6] = (uint8_t)((bms->prot_ctx.active_faults >> 8U) & 0xFFU);
    msg.data[7] = (uint8_t)(bms->prot_ctx.active_faults        & 0xFFU);

    can_send(&msg);
}

void can_tx_cells(const bms_shared_t *bms)
{
    for (uint8_t grp = 0U; grp < 2U; grp++) {
        can_msg_t msg;
        msg.id   = (grp == 0U) ? CAN_TX_CELL_VOLT1 : CAN_TX_CELL_VOLT2;
        msg.type = CAN_FRAME_STD;
        msg.fmt  = CAN_FMT_DATA;
        msg.dlc  = 8U;

        for (uint8_t j = 0U; j < 4U; j++) {
            uint8_t idx = (uint8_t)(grp * 4U + j);
            uint16_t mv = (idx < BQ76940_CELL_COUNT)
                          ? bms->bq_data.cells.cell_mv[idx] : 0U;
            msg.data[j * 2U]     = (uint8_t)((mv >> 8U) & 0xFFU);
            msg.data[j * 2U + 1U] = (uint8_t)(mv & 0xFFU);
        }
        can_send(&msg);
    }

    /* 第 9 个电芯 + 温度放在同一个帧 */
    can_msg_t msg;
    msg.id   = CAN_TX_CELL_VOLT2 + 1U;  /* 0x112 */
    msg.type = CAN_FRAME_STD;
    msg.fmt  = CAN_FMT_DATA;
    msg.dlc  = 8U;

    uint16_t v9 = (BQ76940_CELL_COUNT > 8U)
                  ? bms->bq_data.cells.cell_mv[8U] : 0U;
    msg.data[0] = (uint8_t)((v9 >> 8U) & 0xFFU);
    msg.data[1] = (uint8_t)(v9          & 0xFFU);
    msg.data[2] = (uint8_t)(bms->bq_data.cells.max_mv / 10U);
    msg.data[3] = (uint8_t)(bms->bq_data.cells.min_mv / 10U);
    msg.data[4] = (uint8_t)(bms->bq_data.cells.diff_mv / 10U);
    msg.data[5] = (uint8_t)(bms->bq_data.temps.ts_mdeg_c[0] / 10U);
    msg.data[6] = (uint8_t)(bms->bq_data.temps.ts_mdeg_c[1] / 10U);
    msg.data[7] = (uint8_t)(bms->bq_data.temps.ts_mdeg_c[2] / 10U);

    can_send(&msg);
}

void can_tx_protection(const bms_shared_t *bms)
{
    can_msg_t msg;
    msg.id   = CAN_TX_PROTECTION;
    msg.type = CAN_FRAME_STD;
    msg.fmt  = CAN_FMT_DATA;
    msg.dlc  = 8U;

    msg.data[0] = (uint8_t)((bms->prot_ctx.active_faults >> 8U) & 0xFFU);
    msg.data[1] = (uint8_t)(bms->prot_ctx.active_faults        & 0xFFU);
    msg.data[2] = (uint8_t)((bms->prot_ctx.latched_faults >> 8U) & 0xFFU);
    msg.data[3] = (uint8_t)(bms->prot_ctx.latched_faults        & 0xFFU);
    msg.data[4] = (uint8_t)bms->prot_level;
    msg.data[5] = (uint8_t)bms->prot_ctx.fet;
    msg.data[6] = 0x00U;
    msg.data[7] = 0x00U;

    can_send(&msg);
}

void can_tx_soc_ocv(const bms_shared_t *bms)
{
    can_msg_t msg;
    msg.id   = CAN_TX_SOC_OCV;
    msg.type = CAN_FRAME_STD;
    msg.fmt  = CAN_FMT_DATA;
    msg.dlc  = 8U;

    uint16_t soc  = bms->soc_permil;
    uint16_t ocv  = bms->ocv_mv;
    int32_t  rem  = bms->remaining_mah;

    msg.data[0] = (uint8_t)((soc >> 8U) & 0xFFU);
    msg.data[1] = (uint8_t)(soc         & 0xFFU);
    msg.data[2] = (uint8_t)((ocv >> 8U) & 0xFFU);
    msg.data[3] = (uint8_t)(ocv         & 0xFFU);
    msg.data[4] = (uint8_t)((rem >> 24U) & 0xFFU);
    msg.data[5] = (uint8_t)((rem >> 16U) & 0xFFU);
    msg.data[6] = (uint8_t)((rem >>  8U) & 0xFFU);
    msg.data[7] = (uint8_t)(rem          & 0xFFU);

    can_send(&msg);
}
