/**
 * @file    bq76940.c
 * @brief   BSP 层 BQ76940 电池监视器驱动实现
 * @note    基于软件 I2C + CRC8 通信
 *          - 14 位 ADC 转换: V_cell = ADC_value * 382μV (gain=1)
 *          - 所有写操作必须附加 CRC8
 *          - 多字节读操作需验证 CRC8
 */

#include "bq76940.h"
#include "i2c_sw.h"
#include "io_ctrl.h"
#include "systick.h"

/* ============================================================
 * 默认保护阈值（9 串锂离子电池典型值）
 * ============================================================ */

/** @brief 默认过压阈值: 4.25V */
#define DEFAULT_OV_MV        4250U

/** @brief 默认欠压阈值: 2.80V */
#define DEFAULT_UV_MV        2800U

/** @brief 默认过流放电: 30A */
#define DEFAULT_OCD_MA       30000U

/** @brief 默认短路放电: 60A */
#define DEFAULT_SCD_MA       60000U

/** @brief ADC 满量程 (14-bit) */
#define ADC_MAX              16383U

/* ============================================================
 * 初始化
 * ============================================================ */

bq76940_status_t bq76940_init(const bq76940_protection_cfg_t *cfg)
{
    uint8_t reg_val = 0U;

    /* 1. 探测设备是否存在 */
    if (i2c_sw_probe(BQ76940_I2C_ADDR) != I2C_SW_OK) {
        return BQ76940_I2C_ERROR;
    }

    /* 2. 检查 DEV_XRDY — 等待设备就绪 */
    uint32_t start = bsp_tick_get();
    do {
        if (bq76940_reg_read(BQ76940_REG_SYS_STAT, &reg_val) != BQ76940_OK) {
            return BQ76940_I2C_ERROR;
        }
        if (reg_val & BQ76940_STAT_DEV_XRDY) {
            break;
        }
        bsp_delay_ms(10U);
    } while (!bsp_tick_is_timeout(start, 500U));

    if (!(reg_val & BQ76940_STAT_DEV_XRDY)) {
        return BQ76940_TIMEOUT;
    }

    /* 3. 配置 ADC 使能和库仑计 */
    reg_val = BQ76940_SYS_CTRL1_ADC_EN | BQ76940_SYS_CTRL1_CC_EN;
    if (bq76940_reg_write(BQ76940_REG_SYS_CTRL1, reg_val) != BQ76940_OK) {
        return BQ76940_I2C_ERROR;
    }
    bsp_delay_ms(5U);  /* 等待 ADC 稳定 */

    /* 4. 配置保护阈值 */
    if (cfg != NULL) {
        if (bq76940_set_protection(cfg) != BQ76940_OK) {
            return BQ76940_I2C_ERROR;
        }
    } else {
        /* 使用默认配置 */
        bq76940_protection_cfg_t default_cfg = {
            .ov_mv   = DEFAULT_OV_MV,
            .uv_mv   = DEFAULT_UV_MV,
            .ocd_ma  = DEFAULT_OCD_MA,
            .scd_ma  = DEFAULT_SCD_MA,
            .ov_delay_ms  = 1000U,
            .uv_delay_ms  = 1000U,
            .ocd_delay_ms = 100U,
            .scd_delay_us = 100U,
        };
        if (bq76940_set_protection(&default_cfg) != BQ76940_OK) {
            return BQ76940_I2C_ERROR;
        }
    }

    return BQ76940_OK;
}

/* ============================================================
 * 在线检测
 * ============================================================ */

uint8_t bq76940_is_online(void)
{
    return (i2c_sw_probe(BQ76940_I2C_ADDR) == I2C_SW_OK) ? 1U : 0U;
}

/* ============================================================
 * 关机 / 唤醒
 * ============================================================ */

bq76940_status_t bq76940_shutdown(void)
{
    /* 将 SYS_CTRL1 bit 4 (SHUTDOWN) 置 1 */
    uint8_t ctrl1 = 0U;

    i2c_sw_status_t ret = i2c_sw_read_crc(BQ76940_I2C_ADDR,
                                           BQ76940_REG_SYS_CTRL1, &ctrl1);
    if (ret != I2C_SW_OK) {
        return BQ76940_I2C_ERROR;
    }

    ctrl1 |= (1U << 4U);  /* SHUTDOWN bit */

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_SYS_CTRL1, ctrl1);
    if (ret != I2C_SW_OK) {
        return BQ76940_I2C_ERROR;
    }

    return BQ76940_OK;
}

void bq76940_wakeup(void)
{
    /* 通过 WAKE_BQ 引脚发出唤醒脉冲 */
    io_ctrl_set(IO_ID_WAKE_BQ, IO_LEVEL_HIGH);
    bsp_delay_us(500U);
    io_ctrl_set(IO_ID_WAKE_BQ, IO_LEVEL_LOW);
}

/* ============================================================
 * 单次全量采样
 * ============================================================ */

bq76940_status_t bq76940_read_all(bq76940_data_t *data)
{
    if (data == NULL) {
        return BQ76940_ERROR;
    }

    bq76940_status_t ret;

    /* 记录时间戳 */
    data->timestamp_ms = bsp_tick_get();

    /* 读取电芯电压 */
    ret = bq76940_read_cells(&data->cells);
    if (ret != BQ76940_OK) {
        return ret;
    }

    /* 读取温度 */
    ret = bq76940_read_temps(&data->temps);
    if (ret != BQ76940_OK) {
        return ret;
    }

    /* 读取电流 */
    ret = bq76940_read_current(&data->current);
    if (ret != BQ76940_OK) {
        return ret;
    }

    /* 读取故障状态 */
    ret = bq76940_read_faults(&data->faults);
    if (ret != BQ76940_OK) {
        return ret;
    }

    return BQ76940_OK;
}

/* ============================================================
 * 电芯电压
 * ============================================================ */

bq76940_status_t bq76940_read_cells(bq76940_cell_data_t *cells)
{
    if (cells == NULL) {
        return BQ76940_ERROR;
    }

    /* 批量读取电芯电压寄存器: VC1_LO(0x20) ~ VC9_HI (共 9*2=18 字节) */
    uint8_t buf[18];
    i2c_sw_status_t ret = i2c_sw_read_buf(BQ76940_I2C_ADDR,
                                           BQ76940_REG_VC1_LO,
                                           buf, 18U);
    if (ret != I2C_SW_OK) {
        return BQ76940_I2C_ERROR;
    }

    /* 解析 14 位 ADC 值 → mV */
    cells->max_mv = 0U;
    cells->min_mv = 0xFFFFU;
    cells->total_mv = 0U;

    for (uint8_t i = 0U; i < BQ76940_CELL_COUNT; i++) {
        uint8_t lo = buf[i * 2U];
        uint8_t hi = buf[i * 2U + 1U];

        /* 14-bit: hi[5:0] | lo[7:0] */
        uint16_t adc = (uint16_t)(((uint16_t)(hi & 0x3FU) << 8U) | lo);
        uint16_t mv  = (uint16_t)(((uint32_t)adc * BQ76940_ADC_UV_PER_LSB) / 1000U);

        cells->cell_mv[i] = mv;
        cells->total_mv += mv;

        if (mv > cells->max_mv) { cells->max_mv = mv; }
        if (mv < cells->min_mv) { cells->min_mv = mv; }
    }

    cells->diff_mv = (uint16_t)(cells->max_mv - cells->min_mv);
    cells->valid = 1U;

    return BQ76940_OK;
}

/* ============================================================
 * 温度传感器
 * ============================================================ */

bq76940_status_t bq76940_read_temps(bq76940_temp_data_t *temps)
{
    if (temps == NULL) {
        return BQ76940_ERROR;
    }

    for (uint8_t i = 0U; i < BQ76940_TS_COUNT; i++) {
        uint8_t reg = (uint8_t)(BQ76940_REG_TS1_LO + (i * 2U));
        uint8_t buf[2];

        i2c_sw_status_t ret = i2c_sw_read_buf(BQ76940_I2C_ADDR, reg, buf, 2U);
        if (ret != I2C_SW_OK) {
            temps->valid = 0U;
            return BQ76940_I2C_ERROR;
        }

        uint16_t adc = (uint16_t)(((uint16_t)(buf[1] & 0x3FU) << 8U) | buf[0]);

        /* 温度计算（简化公式，假设 NTC 103AT 型热敏电阻，10kΩ @ 25°C）
         * 实际产品应使用查找表或 Steinhart-Hart 方程
         * 这里返回原始 ADC 值的线性近似，供上层做精确转换 */
        int32_t temp_mdeg = (int32_t)(((uint32_t)adc * 100U) - 25000L);
        temps->ts_mdeg_c[i] = (uint16_t)((temp_mdeg < 0) ? 0U : (uint16_t)temp_mdeg);
    }

    temps->valid = 1U;
    return BQ76940_OK;
}

/* ============================================================
 * 电流 / 库仑计
 * ============================================================ */

bq76940_status_t bq76940_read_current(bq76940_current_data_t *current)
{
    if (current == NULL) {
        return BQ76940_ERROR;
    }

    /* 读取库仑计 CC_LO/HI (0x3C-0x3D) */
    uint8_t buf[2];
    i2c_sw_status_t ret = i2c_sw_read_buf(BQ76940_I2C_ADDR,
                                           BQ76940_REG_CC_LO, buf, 2U);
    if (ret != I2C_SW_OK) {
        current->valid = 0U;
        return BQ76940_I2C_ERROR;
    }

    /* CC 为 16 位有符号值，单位: 取决于外部检流电阻配置
     * 典型值: 1 LSB = 8.44 μV (增益配置) / R_sense
     * 假设 1mΩ 检流电阻: I(mA) = CC * 8.44μV / 1mΩ = CC * 8.44
     * 这里返回原始 CC 值，由上层做单位转换 */
    int16_t cc_raw = (int16_t)(((uint16_t)buf[1] << 8U) | buf[0]);

    /* 简化: 假设 1mΩ 检流电阻，1 LSB ≈ 8.44 mV → 约 8.44 mA */
    current->current_ma  = (int32_t)cc_raw * 844L / 100L;
    current->coulomb_mah = 0U;  /* 累计值由上层积分计算 */
    current->valid = 1U;

    return BQ76940_OK;
}

/* ============================================================
 * 保护故障
 * ============================================================ */

bq76940_status_t bq76940_read_faults(bq76940_fault_t *faults)
{
    if (faults == NULL) {
        return BQ76940_ERROR;
    }

    uint8_t prot1 = 0U, prot2 = 0U, sys_stat = 0U;

    if (bq76940_reg_read(BQ76940_REG_PROTECT1, &prot1) != BQ76940_OK) {
        return BQ76940_I2C_ERROR;
    }
    if (bq76940_reg_read(BQ76940_REG_PROTECT2, &prot2) != BQ76940_OK) {
        return BQ76940_I2C_ERROR;
    }
    if (bq76940_reg_read(BQ76940_REG_SYS_STAT, &sys_stat) != BQ76940_OK) {
        return BQ76940_I2C_ERROR;
    }

    /* PROTECT1: [7]SCD [4]OCD_C [3]OCD_D */
    faults->scd  = (prot1 >> 7U) & 1U;
    faults->oc_c = (prot1 >> 4U) & 1U;
    faults->ocd  = (prot1 >> 3U) & 1U;

    /* PROTECT2: [7]OV [6]UV [4]OT [3]UT */
    faults->ov = (prot2 >> 7U) & 1U;
    faults->uv = (prot2 >> 6U) & 1U;
    faults->ot = (prot2 >> 4U) & 1U;
    faults->ut = (prot2 >> 3U) & 1U;

    /* SYS_STAT */
    faults->dev_xrdy = (sys_stat >> 7U) & 1U;

    return BQ76940_OK;
}

/* ============================================================
 * 保护配置
 * ============================================================ */

bq76940_status_t bq76940_set_protection(const bq76940_protection_cfg_t *cfg)
{
    if (cfg == NULL) {
        return BQ76940_ERROR;
    }

    /* 设置过压阈值: OV_TRIP = (V_ov / 382μV) */
    if (cfg->ov_mv > 0U) {
        uint16_t ov_adc = (uint16_t)((uint32_t)cfg->ov_mv * 1000U
                                     / BQ76940_ADC_UV_PER_LSB);
        if (ov_adc > ADC_MAX) { ov_adc = ADC_MAX; }

        uint8_t ov_lo = (uint8_t)(ov_adc & 0xFFU);
        uint8_t ov_hi = (uint8_t)((ov_adc >> 8U) & 0xFFU);

        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OV_TRIP,     ov_lo);
        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OV_TRIP + 1U, ov_hi);
    }

    /* 设置欠压阈值 */
    if (cfg->uv_mv > 0U) {
        uint16_t uv_adc = (uint16_t)((uint32_t)cfg->uv_mv * 1000U
                                     / BQ76940_ADC_UV_PER_LSB);
        if (uv_adc > ADC_MAX) { uv_adc = ADC_MAX; }

        uint8_t uv_lo = (uint8_t)(uv_adc & 0xFFU);
        uint8_t uv_hi = (uint8_t)((uv_adc >> 8U) & 0xFFU);

        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_UV_TRIP,     uv_lo);
        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_UV_TRIP + 1U, uv_hi);
    }

    return BQ76940_OK;
}

bq76940_status_t bq76940_clear_faults(void)
{
    /* 向 SYS_STAT 写入 0xFF 清除所有状态位 */
    return bq76940_reg_write(BQ76940_REG_SYS_STAT, 0xFFU);
}

/* ============================================================
 * 电池均衡
 * ============================================================ */

bq76940_status_t bq76940_set_balancing(uint16_t balance_mask)
{
    /* CELLBAL1: VC1-VC5 (bit[4:0]) */
    uint8_t bal1 = (uint8_t)(balance_mask & 0x1FU);
    /* CELLBAL2: VC6-VC9 (bit[3:0]) */
    uint8_t bal2 = (uint8_t)((balance_mask >> 5U) & 0x0FU);

    i2c_sw_status_t ret;

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL1, bal1);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL2, bal2);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    return BQ76940_OK;
}

uint16_t bq76940_get_balancing(void)
{
    uint8_t bal1 = 0U, bal2 = 0U;

    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL1, &bal1);
    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL2, &bal2);

    return (uint16_t)(((uint16_t)bal2 << 5U) | (bal1 & 0x1FU));
}

bq76940_status_t bq76940_balance_off(void)
{
    return bq76940_set_balancing(0x0000U);
}

/* ============================================================
 * 低层寄存器访问
 * ============================================================ */

bq76940_status_t bq76940_reg_write(uint8_t reg, uint8_t data)
{
    i2c_sw_status_t ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, reg, data);

    switch (ret) {
        case I2C_SW_OK:       return BQ76940_OK;
        case I2C_SW_NACK:     return BQ76940_I2C_ERROR;
        case I2C_SW_CRC_ERROR: return BQ76940_CRC_ERROR;
        default:              return BQ76940_ERROR;
    }
}

bq76940_status_t bq76940_reg_read(uint8_t reg, uint8_t *data)
{
    i2c_sw_status_t ret = i2c_sw_read_crc(BQ76940_I2C_ADDR, reg, data);

    switch (ret) {
        case I2C_SW_OK:       return BQ76940_OK;
        case I2C_SW_NACK:     return BQ76940_I2C_ERROR;
        case I2C_SW_CRC_ERROR: return BQ76940_CRC_ERROR;
        default:              return BQ76940_ERROR;
    }
}
