/**
 * @file    can_cmd.c
 * @brief   CAN 指令协议实现 — 纯协议解析 + 周期性上报帧生成
 * @note    can_cmd_dispatch() 返回 can_action_req_t, 零 BSP 调用
 *          can_pub() 生成上报帧, 调用者负责发送
 */

#include "can_cmd.h"
#include "soc_ocv.h"
#include <string.h>

/* ============================================================
 * 配置查询子命令
 * ============================================================ */

#define CAN_QUERY_ALL       0x00U
#define CAN_QUERY_STATUS    0x01U
#define CAN_QUERY_CELLS     0x02U
#define CAN_QUERY_PROTECTION 0x03U
#define CAN_QUERY_SOC       0x04U

/* ============================================================
 * 配置参数类型 (0x202)
 * ============================================================ */

#define CAN_CFG_CELL_OV_MV       0x01U
#define CAN_CFG_CELL_UV_MV       0x02U
#define CAN_CFG_DISCHARGE_OC_MA  0x03U
#define CAN_CFG_CHARGE_OC_MA     0x04U
#define CAN_CFG_BALANCE_THRESH_MV 0x05U
#define CAN_CFG_BALANCE_MIN_MV   0x06U

/* ============================================================
 * 模块内变量
 * ============================================================ */

/* ============================================================
 * 初始化
 * ============================================================ */

void can_cmd_init(void)
{
    /* CAN RX 由 can_drv 内部 ring_buf 处理, 无需额外队列 */
}

/* ============================================================
 * CAN RX 轮询 (由 task_can_rx 调用)
 * ============================================================ */

/**
 * @brief  CAN RX 已由 can_drv 层处理, 此函数保留供兼容
 * @retval 始终返回 1 (空)
 */
uint8_t can_cmd_poll_rx(can_msg_t *msg)
{
    (void)msg;
    return 1U;
}

/* ============================================================
 * 指令分发
 * ============================================================ */

/**
 * @brief  处理查询命令 — 根据 sub_cmd 生成不同响应帧
 */
static void handle_query(uint8_t sub_cmd, bms_shared_t *bms, can_msg_t *resp)
{
    if (bms == NULL || resp == NULL) { return; }

    const bq76940_cell_data_t *cells = &bms->battery_val.cells;
    uint32_t pack_mv = cells->total_mv;
    int32_t  current = bms->battery_val.current.current_ma;

    switch (sub_cmd) {
    case CAN_QUERY_ALL:
    case CAN_QUERY_STATUS:
    default:
        /* BMS_STATUS 帧 (0x100) */
        resp->id  = CAN_TX_BMS_STATUS;
        resp->len = 8U;
        {
            int16_t i_scaled = (int16_t)(current / 10L);
            resp->data[0] = (uint8_t)(bms->soc_permil / 10U);
            resp->data[1] = (uint8_t)((pack_mv >> 8U) & 0xFFU);
            resp->data[2] = (uint8_t)(pack_mv & 0xFFU);
            resp->data[3] = (uint8_t)(((uint16_t)i_scaled >> 8U) & 0xFFU);
            resp->data[4] = (uint8_t)((uint16_t)i_scaled & 0xFFU);
            resp->data[5] = (uint8_t)bms->prot_level;
            resp->data[6] = 0x00U;
            resp->data[7] = 0x00U;
        }
        break;

    case CAN_QUERY_CELLS:
        /* CELL_VOLT_1_4 帧 (0x110) */
        resp->id  = CAN_TX_CELL_VOLT_1_4;
        resp->len = 8U;
        for (uint8_t i = 0U; i < 4U; i++) {
            resp->data[i * 2U]     = (uint8_t)((cells->cell_mv[i] >> 8U) & 0xFFU);
            resp->data[i * 2U + 1U] = (uint8_t)(cells->cell_mv[i] & 0xFFU);
        }
        break;

    case CAN_QUERY_PROTECTION:
        /* 保护状态帧 (0x100) */
        resp->id  = CAN_TX_BMS_STATUS;
        resp->len = 8U;
        resp->data[0] = (uint8_t)bms->prot_level;
        resp->data[1] = (uint8_t)((bms->active_faults >> 8U) & 0xFFU);
        resp->data[2] = (uint8_t)(bms->active_faults & 0xFFU);
        resp->data[3] = (uint8_t)((cells->max_mv >> 8U) & 0xFFU);
        resp->data[4] = (uint8_t)(cells->max_mv & 0xFFU);
        resp->data[5] = (uint8_t)((cells->min_mv >> 8U) & 0xFFU);
        resp->data[6] = (uint8_t)(cells->min_mv & 0xFFU);
        resp->data[7] = 0x00U;
        break;

    case CAN_QUERY_SOC:
        /* SOC_OCV 帧 (0x130) */
        resp->id  = CAN_TX_SOC_OCV;
        resp->len = 8U;
        resp->data[0] = (uint8_t)((bms->soc_permil >> 8U) & 0xFFU);
        resp->data[1] = (uint8_t)(bms->soc_permil & 0xFFU);
        resp->data[2] = (uint8_t)((soc_ocv_get_real_soc() >> 8U) & 0xFFU);
        resp->data[3] = (uint8_t)(soc_ocv_get_real_soc() & 0xFFU);
        {
            int32_t mah_scaled = bms->remaining_mah / 100L;
            resp->data[4] = (uint8_t)(((uint32_t)mah_scaled >> 8U) & 0xFFU);
            resp->data[5] = (uint8_t)((uint32_t)mah_scaled & 0xFFU);
        }
        {
            int32_t q_max_scaled = soc_ocv_get_q_max() / 100L;
            resp->data[6] = (uint8_t)(((uint32_t)q_max_scaled >> 8U) & 0xFFU);
            resp->data[7] = (uint8_t)((uint32_t)q_max_scaled & 0xFFU);
        }
        break;
    }
}

/**
 * @brief  处理控制命令 → 返回 can_action_req_t
 */
static can_action_req_t handle_control(const can_msg_t *msg, uint16_t active_faults)
{
    can_action_req_t req = { CAN_ACTION_NONE, 0U };
    if (msg == NULL || msg->len < 1U) { return req; }

    uint8_t cmd = msg->data[0];

    switch (cmd) {
    case CAN_CTRL_CLEAR_FAULT:
        req.action = CAN_ACTION_CLEAR_FAULT;
        break;
    case CAN_CTRL_FET_CHG_ON:
        /* 安全条件: 无活跃故障时才允许开启 */
        if (active_faults == 0x0000U) {
            req.action = CAN_ACTION_FET_CHG_ON;
        }
        break;
    case CAN_CTRL_FET_CHG_OFF:
        req.action = CAN_ACTION_FET_CHG_OFF;
        break;
    case CAN_CTRL_FET_DSG_ON:
        if (active_faults == 0x0000U) {
            req.action = CAN_ACTION_FET_DSG_ON;
        }
        break;
    case CAN_CTRL_FET_DSG_OFF:
        req.action = CAN_ACTION_FET_DSG_OFF;
        break;
    case CAN_CTRL_BALANCE_SET:
        req.action = CAN_ACTION_BALANCE_SET;
        if (msg->len >= 3U) {
            req.balance_mask = (uint16_t)(((uint16_t)msg->data[1] << 8U) | msg->data[2]);
        }
        break;
    case CAN_CTRL_BALANCE_OFF:
        req.action = CAN_ACTION_BALANCE_OFF;
        break;
    case CAN_CTRL_SHUTDOWN:
        req.action = CAN_ACTION_SHUTDOWN;
        break;
    default:
        break;
    }

    return req;
}

/**
 * @brief  处理配置命令 → 更新 settings
 */
static void handle_config(const can_msg_t *msg)
{
    if (msg == NULL || msg->len < 3U) { return; }

    uint8_t  param = msg->data[0];
    uint16_t value = (uint16_t)(((uint16_t)msg->data[1] << 8U) | msg->data[2]);

    bms_settings_t *settings = bms_shared_settings_lock(osWaitForever);
    if (settings == NULL) { return; }

    switch (param) {
    case CAN_CFG_CELL_OV_MV:       settings->cell_ov_mv       = value; break;
    case CAN_CFG_CELL_UV_MV:       settings->cell_uv_mv       = value; break;
    case CAN_CFG_DISCHARGE_OC_MA:  settings->discharge_oc_ma  = value; break;
    case CAN_CFG_CHARGE_OC_MA:     settings->charge_oc_ma     = value; break;
    case CAN_CFG_BALANCE_THRESH_MV: settings->balance_thresh_mv = value; break;
    case CAN_CFG_BALANCE_MIN_MV:   settings->balance_min_mv   = value; break;
    default: break;
    }

    bms_shared_settings_unlock();
}

can_action_req_t can_cmd_dispatch(const can_msg_t *msg, uint16_t active_faults,
                                   can_msg_t *resp_frame)
{
    can_action_req_t req = { CAN_ACTION_NONE, 0U };

    if (msg == NULL) { return req; }

    switch (msg->id) {
    case CAN_RX_QUERY: {
        /* 查询: 生成响应帧写入 resp_frame (非 NULL 时) */
        if (resp_frame != NULL) {
            bms_shared_t *bms = bms_shared_data_lock(10U);
            if (bms != NULL) {
                handle_query(msg->data[0], bms, resp_frame);
                bms_shared_data_unlock();
            }
        }
        break;
    }
    case CAN_RX_CONTROL:
        req = handle_control(msg, active_faults);
        break;
    case CAN_RX_CONFIG:
        handle_config(msg);
        break;
    default:
        break;
    }

    return req;
}

/* ============================================================
 * 周期性 CAN 上报帧生成
 * ============================================================ */

uint8_t can_pub(const bms_shared_t *bms, can_msg_t *frames)
{
    if (bms == NULL || frames == NULL) { return 0U; }

    const bq76940_cell_data_t *cells = &bms->battery_val.cells;
    int32_t current_ma = bms->battery_val.current.current_ma;

    /* ---- Frame 0: 0x110 CELL_VOLT_1_4 (8 bytes, big-endian) ---- */
    frames[0].id  = CAN_TX_CELL_VOLT_1_4;
    frames[0].len = 8U;
    for (uint8_t i = 0U; i < 4U; i++) {
        frames[0].data[i * 2U]     = (uint8_t)((cells->cell_mv[i] >> 8U) & 0xFFU);
        frames[0].data[i * 2U + 1U] = (uint8_t)(cells->cell_mv[i] & 0xFFU);
    }

    /* ---- Frame 1: 0x111 CELL_VOLT_5_8 (8 bytes, big-endian) ---- */
    frames[1].id  = CAN_TX_CELL_VOLT_5_9;
    frames[1].len = 8U;
    for (uint8_t i = 4U; i < 8U; i++) {
        uint8_t idx = i - 4U;
        frames[1].data[idx * 2U]     = (uint8_t)((cells->cell_mv[i] >> 8U) & 0xFFU);
        frames[1].data[idx * 2U + 1U] = (uint8_t)(cells->cell_mv[i] & 0xFFU);
    }

    /* ---- Frame 2: 0x120 BMS_STATUS (8 bytes, big-endian) ---- */
    frames[2].id  = CAN_TX_STATUS;
    frames[2].len = 8U;

    uint32_t pack_mv = cells->total_mv;
    int16_t  current_scaled = (int16_t)(current_ma / 10L);

    frames[2].data[0] = (uint8_t)(bms->soc_permil / 10U);
    frames[2].data[1] = (uint8_t)((pack_mv >> 8U) & 0xFFU);
    frames[2].data[2] = (uint8_t)(pack_mv & 0xFFU);
    frames[2].data[3] = (uint8_t)(((uint16_t)current_scaled >> 8U) & 0xFFU);
    frames[2].data[4] = (uint8_t)((uint16_t)current_scaled & 0xFFU);
    /* ctrl byte: [1:0]=prot_level, [2]=CHG_FET, [3]=DSG_FET, [4]=BALANCING */
    {
        uint8_t ctrl = (uint8_t)(bms->prot_level & 0x03U);
        /* FET/均衡状态从全局读取 (task_protect/bms_app 维护) */
        extern uint8_t g_fet_chg_on;    /* 定义在 bms_app.c */
        extern uint8_t g_fet_dsg_on;
        extern uint8_t g_balancing_active;
        if (g_fet_chg_on)  { ctrl |= (1U << 2U); }
        if (g_fet_dsg_on)  { ctrl |= (1U << 3U); }
        if (g_balancing_active) { ctrl |= (1U << 4U); }
        frames[2].data[5] = ctrl;
    }
    /* Cell 9 (index 8) in spare bytes, big-endian */
    frames[2].data[6] = (uint8_t)((cells->cell_mv[8] >> 8U) & 0xFFU);
    frames[2].data[7] = (uint8_t)(cells->cell_mv[8] & 0xFFU);

    /* ---- Frame 3: 0x130 SOC_OCV (8 bytes, big-endian) ---- */
    frames[3].id  = CAN_TX_SOC_OCV;
    frames[3].len = 8U;

    frames[3].data[0] = (uint8_t)((bms->soc_permil >> 8U) & 0xFFU);
    frames[3].data[1] = (uint8_t)(bms->soc_permil & 0xFFU);
    frames[3].data[2] = (uint8_t)((soc_ocv_get_real_soc() >> 8U) & 0xFFU);
    frames[3].data[3] = (uint8_t)(soc_ocv_get_real_soc() & 0xFFU);

    int32_t mah_scaled = bms->remaining_mah / 100L;
    frames[3].data[4] = (uint8_t)(((uint32_t)mah_scaled >> 8U) & 0xFFU);
    frames[3].data[5] = (uint8_t)((uint32_t)mah_scaled & 0xFFU);

    int32_t q_max_scaled = soc_ocv_get_q_max() / 100L;
    frames[3].data[6] = (uint8_t)(((uint32_t)q_max_scaled >> 8U) & 0xFFU);
    frames[3].data[7] = (uint8_t)((uint32_t)q_max_scaled & 0xFFU);

    /* ---- Frame 4: 0x121 TEMPERATURE (8 bytes, big-endian int16 ×3 + pad) ---- */
    frames[4].id  = CAN_TX_TEMPERATURE;
    frames[4].len = 8U;
    {
        const bq76940_temp_data_t *temps = &bms->battery_val.temps;
        for (uint8_t i = 0U; i < 3U; i++) {
            int16_t t = temps->ts_mdeg_c[i];
            frames[4].data[i * 2U]     = (uint8_t)(((uint16_t)t >> 8U) & 0xFFU);
            frames[4].data[i * 2U + 1U] = (uint8_t)((uint16_t)t & 0xFFU);
        }
        frames[4].data[6] = 0x00U;
        frames[4].data[7] = 0x00U;
    }

    return 5U;
}
