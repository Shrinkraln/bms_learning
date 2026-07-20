/**
 * @file    data_report.c
 * @brief   BMS 数据上报模块实现
 */

#include "data_report.h"
#include <stdio.h>

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief 串口格式化缓冲区 */
static char serial_buf[256];

/* ============================================================
 * 初始化
 * ============================================================ */

void data_report_init(void)
{
    /* CAN 和 USART 在 BSP 层已初始化，此处无需额外操作 */
}

/* ============================================================
 * 上报入口
 * ============================================================ */

void data_report_all(const bq76940_data_t *data,
                      const protection_ctx_t *prot)
{
    data_report_can(data, prot);
    data_report_serial(data, prot);
}

/* ============================================================
 * CAN 上报
 * ============================================================ */

void data_report_can(const bq76940_data_t *data,
                      const protection_ctx_t *prot)
{
    if (data == NULL || prot == NULL) { return; }
    can_msg_t msg;

    /* ---- 0x100: 电池状态 ---- */
    msg.id   = CAN_ID_BMS_STATUS;
    msg.type = CAN_FRAME_STD;
    msg.fmt  = CAN_FMT_DATA;
    msg.dlc  = 8U;

    /* Byte 0-1: 总电压 (mV), 大端 */
    uint32_t total_mv = data->cells.total_mv;
    msg.data[0] = (uint8_t)((total_mv >> 8U)  & 0xFFU);
    msg.data[1] = (uint8_t)(total_mv          & 0xFFU);

    /* Byte 2-3: 电流 (mA), 有符号，大端 */
    int32_t current_ma = data->current.current_ma;
    msg.data[2] = (uint8_t)((current_ma >> 8U) & 0xFFU);
    msg.data[3] = (uint8_t)(current_ma         & 0xFFU);

    /* Byte 4: 最高单芯电压 (×10 mV→V/10) */
    msg.data[4] = (uint8_t)((data->cells.max_mv * 10U / 4250U) & 0xFFU);

    /* Byte 5: 最低单芯电压 (×10) */
    msg.data[5] = (uint8_t)((data->cells.min_mv * 10U / 4250U) & 0xFFU);

    /* Byte 6: FET 状态 + 保护等级 */
    msg.data[6] = (uint8_t)((prot->fet << 4U) | prot->level);

    /* Byte 7: 最高温度 (0.1°C, 简化为 °C) */
    msg.data[7] = (uint8_t)(data->temps.ts_mdeg_c[0] / 10U);

    can_send(&msg);

    /* ---- 0x110-0x112: 电芯电压 ---- */
    for (uint8_t grp = 0U; grp < 3U; grp++) {
        msg.id   = (uint32_t)(CAN_ID_CELL_VOLT1 + grp);
        msg.type = CAN_FRAME_STD;
        msg.fmt  = CAN_FMT_DATA;
        msg.dlc  = 8U;

        for (uint8_t j = 0U; j < 4U; j++) {
            uint8_t cell_idx = (uint8_t)(grp * 4U + j);
            if (cell_idx < BQ76940_CELL_COUNT) {
                /* 每电芯 2 字节 (mV), 总 8 字节 */
                uint16_t mv = data->cells.cell_mv[cell_idx];
                msg.data[j * 2U]     = (uint8_t)((mv >> 8U) & 0xFFU);
                msg.data[j * 2U + 1U] = (uint8_t)(mv & 0xFFU);
            } else {
                msg.data[j * 2U]     = 0x00U;
                msg.data[j * 2U + 1U] = 0x00U;
            }
        }
        can_send(&msg);
    }

    /* ---- 0x120: 保护状态 + 故障码 ---- */
    msg.id   = CAN_ID_PROTECTION;
    msg.type = CAN_FRAME_STD;
    msg.fmt  = CAN_FMT_DATA;
    msg.dlc  = 6U;

    /* Byte 0-1: 活跃故障码 */
    msg.data[0] = (uint8_t)((prot->active_faults >> 8U) & 0xFFU);
    msg.data[1] = (uint8_t)(prot->active_faults         & 0xFFU);

    /* Byte 2-3: 锁存故障码 */
    msg.data[2] = (uint8_t)((prot->latched_faults >> 8U) & 0xFFU);
    msg.data[3] = (uint8_t)(prot->latched_faults         & 0xFFU);

    /* Byte 4: 电芯压差 / 10 mV */
    msg.data[4] = (uint8_t)((data->cells.diff_mv / 10U) & 0xFFU);

    /* Byte 5: 平衡状态 */
    msg.data[5] = (uint8_t)(bq76940_get_balancing() & 0xFFU);

    can_send(&msg);
}

/* ============================================================
 * 串口上报（调试）
 * ============================================================ */

void data_report_serial(const bq76940_data_t *data,
                         const protection_ctx_t *prot)
{
    if (data == NULL || prot == NULL) { return; }

    /* 电芯电压 */
    usart_send_string(USART_ID_1, "[BMS] C:");
    for (uint8_t i = 0U; i < BQ76940_CELL_COUNT; i++) {
        sprintf(serial_buf, " %u.%03u",
                data->cells.cell_mv[i] / 1000U,
                data->cells.cell_mv[i] % 1000U);
        usart_send_string(USART_ID_1, serial_buf);
    }

    /* 总压 */
    sprintf(serial_buf, " | TOTAL:%lu.%03uV",
            data->cells.total_mv / 1000U,
            (unsigned int)(data->cells.total_mv % 1000U));
    usart_send_string(USART_ID_1, serial_buf);

    /* 电流 */
    sprintf(serial_buf, " | I:%ld.%03ldA",
            data->current.current_ma / 1000L,
            (data->current.current_ma % 1000L) >= 0
                ? (data->current.current_ma % 1000L)
                : -(data->current.current_ma % 1000L));
    usart_send_string(USART_ID_1, serial_buf);

    /* 故障码 */
    if (prot->active_faults != 0x0000U) {
        usart_send_string(USART_ID_1, " | FAULT:");
        for (uint8_t i = 0U; i < 12U; i++) {
            if (prot->active_faults & (1U << i)) {
                usart_send_string(USART_ID_1,
                                  protection_fault_str((fault_code_t)(1U << i)));
                usart_send_string(USART_ID_1, " ");
            }
        }
    } else {
        usart_send_string(USART_ID_1, " | OK");
    }

    usart_send_string(USART_ID_1, "\r\n");
}
