/**
 * @file    bq76940.c
 * @brief   BSP 层 BQ76940 电池监视器驱动实现
 * @note    基于软件 I2C + CRC8 通信
 *          - ADC 校准: 读取出厂 GAIN/OFFSET 寄存器
 *          - 电压: V_mV = (GAIN × ADC_raw) / 1000 + OFFSET_mV
 *          - 电流: I_mA = CC_raw × 844 / (R_sense_mΩ × 100)
 *          - 温度: NTC 103AT 分压→电阻→33 点查表插值
 *          - FET 控制: 通过 SYS_CTRL2 CHG/DSG 位
 */

#include "bq76940.h"
#include "i2c_sw.h"
#include "systick.h"

/* ============================================================
 * 默认配置
 * ============================================================ */

#define DEFAULT_OV_MV          4250U
#define DEFAULT_UV_MV          2800U
#define DEFAULT_DISCHARGE_OC_MA  30000U
#define DEFAULT_CHARGE_OC_MA     15000U

/* OCD/SCD 延时默认值 (单位: μs; BQ76940 延时寄存器为 8-bit, 步长因寄存器而异) */
#define DEFAULT_OCD_DELAY_US    8000U   /* 8ms */
#define DEFAULT_SCD_DELAY_US     100U   /* 100μs */

/* ============================================================
 * 模块静态变量
 * ============================================================ */

static bq76940_calib_t g_calib = {
    .gain_uv_per_lsb = BQ76940_ADC_UV_PER_LSB,  /* 默认 382 */
    .offset_mv       = 0,
};
static uint16_t g_r_sense_mohm = BQ76940_DEFAULT_R_SENSE_MOHM;

/* ============================================================
 * NTC 103AT 温度查表 (33 点, -40~120°C, 5°C 步长)
 *
 * R(T) = 10000 × exp(3435 × (1/(T+273.15) - 1/298.15))
 * 表按电阻降序排列（从低温→高温）, 用于二分查找
 * ============================================================ */

typedef struct {
    uint32_t r_ohm;        /**< NTC 电阻 (Ω)    */
    int16_t  temp_mdeg;    /**< 温度 (0.1°C)    */
} ntc_point_t;

static const ntc_point_t ntc_table[] = {
    {248300U,  -400},  /* -40.0°C */
    {182200U,  -350},  /* -35.0°C */
    {135500U,  -300},  /* -30.0°C */
    {102000U,  -250},  /* -25.0°C */
    { 77400U,  -200},  /* -20.0°C */
    { 59600U,  -150},  /* -15.0°C */
    { 46300U,  -100},  /* -10.0°C */
    { 36300U,   -50},  /*  -5.0°C */
    { 28700U,     0},  /*   0.0°C */
    { 22900U,    50},  /*   5.0°C */
    { 18400U,   100},  /*  10.0°C */
    { 14900U,   150},  /*  15.0°C */
    { 12170U,   200},  /*  20.0°C */
    { 10000U,   250},  /*  25.0°C */
    {  8250U,   300},  /*  30.0°C */
    {  6880U,   350},  /*  35.0°C */
    {  5750U,   400},  /*  40.0°C */
    {  4850U,   450},  /*  45.0°C */
    {  4110U,   500},  /*  50.0°C */
    {  3490U,   550},  /*  55.0°C */
    {  2990U,   600},  /*  60.0°C */
    {  2560U,   650},  /*  65.0°C */
    {  2210U,   700},  /*  70.0°C */
    {  1910U,   750},  /*  75.0°C */
    {  1660U,   800},  /*  80.0°C */
    {  1450U,   850},  /*  85.0°C */
    {  1270U,   900},  /*  90.0°C */
    {  1120U,   950},  /*  95.0°C */
    {   990U,  1000},  /* 100.0°C */
    {   870U,  1050},  /* 105.0°C */
    {   780U,  1100},  /* 110.0°C */
    {   690U,  1150},  /* 115.0°C */
    {   620U,  1200},  /* 120.0°C */
};

#define NTC_TABLE_SIZE  (sizeof(ntc_table) / sizeof(ntc_table[0]))

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief  NTC 电阻值 → 温度 (二分查找 + 线性插值)
 * @param  r_ntc_ohm  NTC 电阻 (Ω)
 * @retval 温度 (0.1°C), clamp 到 [-400, 1200]
 */
static int16_t ntc_resistance_to_temp(uint32_t r_ntc_ohm)
{
    /* 边界检查 */
    if (r_ntc_ohm >= ntc_table[0].r_ohm) {
        return ntc_table[0].temp_mdeg;       /* 低温 clamp */
    }
    if (r_ntc_ohm <= ntc_table[NTC_TABLE_SIZE - 1U].r_ohm) {
        return ntc_table[NTC_TABLE_SIZE - 1U].temp_mdeg;  /* 高温 clamp */
    }

    /* 二分查找 (表按电阻降序排列) */
    uint8_t lo = 0U;
    uint8_t hi = (uint8_t)(NTC_TABLE_SIZE - 1U);

    while (lo < hi) {
        uint8_t mid = (uint8_t)((lo + hi) / 2U);
        if (ntc_table[mid].r_ohm > r_ntc_ohm) {
            lo = (uint8_t)(mid + 1U);
        } else {
            hi = mid;
        }
    }

    /* hi 指向第一个 r_ohm <= r_ntc_ohm 的点 (即电阻更大的那个点) */
    if (hi == 0U) {
        return ntc_table[0].temp_mdeg;
    }

    /* 线性插值: 在 point[hi-1] (较高电阻/较低温度) 和 point[hi] (较低电阻/较高温度) 之间 */
    const ntc_point_t *p_high = &ntc_table[hi - 1U];  /* R 更大 */
    const ntc_point_t *p_low  = &ntc_table[hi];       /* R 更小 */

    /* 分母: p_high->r_ohm > p_low->r_ohm 因为降序 */
    int32_t delta_r   = (int32_t)(p_high->r_ohm - p_low->r_ohm);
    int32_t delta_t   = (int32_t)(p_low->temp_mdeg - p_high->temp_mdeg);
    int32_t r_diff    = (int32_t)(r_ntc_ohm - p_low->r_ohm);

    /* temp = p_low->temp_mdeg - (r_diff × delta_t) / delta_r */
    int32_t temp = (int32_t)p_low->temp_mdeg - (r_diff * delta_t) / delta_r;

    /* clamp */
    if (temp < -400) { temp = -400; }
    if (temp > 1200) { temp = 1200; }

    return (int16_t)temp;
}

/**
 * @brief  单通道 ADC → NTC 温度转换
 * @param  adc_raw  14-bit ADC 值
 * @retval 温度 (0.1°C)
 */
static int16_t adc_to_ntc_temp(uint16_t adc_raw)
{
    int32_t gain = (int32_t)g_calib.gain_uv_per_lsb;

    /* Step 1: ADC → 电压 (mV) */
    uint32_t v_ts_mv = ((uint32_t)adc_raw * (uint32_t)gain) / 1000U;

    /* Step 2: 电压 → NTC 电阻 — V_TS = VREF × R_NTC / (R_pullup + R_NTC)
     * R_NTC = R_pullup × V_TS / (VREF - V_TS)
     * R_pullup = 10kΩ (BQ76940 内部), VREF = 3300mV */
    if (v_ts_mv >= BQ76940_VREF_MV) {
        return 1200;  /* NTC 短路 → 最高温 */
    }
    if (v_ts_mv == 0U) {
        return -400;  /* NTC 开路 → 最低温 */
    }

    uint32_t r_ntc_ohm = (10000UL * v_ts_mv) / (BQ76940_VREF_MV - v_ts_mv);

    /* Step 3: 电阻 → 温度 (查表插值) */
    return ntc_resistance_to_temp(r_ntc_ohm);
}

/* ============================================================
 * 初始化
 * ============================================================ */

bq76940_status_t bq76940_init(const bq76940_cfg_t *cfg)
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

    /* 3. 读取 ADC 校准值 (ADCGAIN1, ADCGAIN2, ADCOFFSET) */
    uint8_t adcgain1 = 0U, adcgain2 = 0U, adcoffset = 0U;

    if (bq76940_reg_read(BQ76940_REG_ADCGAIN1, &adcgain1) == BQ76940_OK
     && bq76940_reg_read(BQ76940_REG_ADCGAIN2, &adcgain2) == BQ76940_OK) {
        /* GAIN = 365 + ADCGAIN<4:0>
         * ADCGAIN1[4:0] 为低 5 位 */
        uint8_t adcgain = adcgain1 & 0x1FU;
        g_calib.gain_uv_per_lsb = (uint16_t)(365U + (uint16_t)adcgain);
    } else {
        /* 回退默认值 */
        g_calib.gain_uv_per_lsb = BQ76940_ADC_UV_PER_LSB;
    }

    if (bq76940_reg_read(BQ76940_REG_ADCOFFSET, &adcoffset) == BQ76940_OK) {
        /* OFFSET 为 2's complement int8, 单位 mV */
        g_calib.offset_mv = (int16_t)((int8_t)adcoffset);
    } else {
        g_calib.offset_mv = 0;
    }

    /* 4. 存储检流电阻值 */
    if (cfg != NULL && cfg->r_sense_mohm > 0U) {
        g_r_sense_mohm = cfg->r_sense_mohm;
    } else {
        g_r_sense_mohm = BQ76940_DEFAULT_R_SENSE_MOHM;
    }

    /* 5. 配置 SYS_CTRL1: ADC_EN + CC_EN + TEMP_SEL=1 (外部 NTC) */
    reg_val = BQ76940_SYS_CTRL1_ADC_EN
            | BQ76940_SYS_CTRL1_CC_EN
            | BQ76940_SYS_CTRL1_TEMP_SEL;
    if (bq76940_reg_write(BQ76940_REG_SYS_CTRL1, reg_val) != BQ76940_OK) {
        return BQ76940_I2C_ERROR;
    }
    bsp_delay_ms(5U);  /* 等待 ADC 稳定 */

    /* 6. 配置保护阈值 */
    if (cfg != NULL) {
        if (bq76940_set_protection(cfg) != BQ76940_OK) {
            return BQ76940_I2C_ERROR;
        }
    } else {
        bq76940_cfg_t default_cfg = {
            .r_sense_mohm    = BQ76940_DEFAULT_R_SENSE_MOHM,
            .cell_ov_mv      = DEFAULT_OV_MV,
            .cell_uv_mv      = DEFAULT_UV_MV,
            .discharge_oc_ma = DEFAULT_DISCHARGE_OC_MA,
            .charge_oc_ma    = DEFAULT_CHARGE_OC_MA,
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
 * 校准数据访问
 * ============================================================ */

const bq76940_calib_t *bq76940_get_calib(void)
{
    return &g_calib;
}

/* ============================================================
 * 关机
 * ============================================================ */

bq76940_status_t bq76940_shutdown(void)
{
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

/* ============================================================
 * FET 控制 (SYS_CTRL2)
 * ============================================================ */

bq76940_status_t bq76940_write_sys_ctrl2(uint8_t mask, uint8_t value)
{
    uint8_t ctrl2 = 0U;

    /* 读取当前值 */
    i2c_sw_status_t ret = i2c_sw_read_crc(BQ76940_I2C_ADDR,
                                           BQ76940_REG_SYS_CTRL2, &ctrl2);
    if (ret != I2C_SW_OK) {
        return BQ76940_I2C_ERROR;
    }

    /* 修改指定位 */
    ctrl2 = (ctrl2 & ~mask) | (value & mask);

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_SYS_CTRL2, ctrl2);
    if (ret != I2C_SW_OK) {
        return BQ76940_I2C_ERROR;
    }

    return BQ76940_OK;
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
 * 电芯电压 (校准后)
 * ============================================================ */

bq76940_status_t bq76940_read_cells(bq76940_cell_data_t *cells)
{
    if (cells == NULL) {
        return BQ76940_ERROR;
    }

    int32_t gain = (int32_t)g_calib.gain_uv_per_lsb;
    int16_t offset = g_calib.offset_mv;

    /* 批量读取电芯电压寄存器: VC1_LO(0x20) ~ VC9_HI (共 9×2=18 字节) */
    uint8_t buf[18];
    i2c_sw_status_t ret = i2c_sw_read_buf(BQ76940_I2C_ADDR,
                                           BQ76940_REG_VC1_LO,
                                           buf, 18U);
    if (ret != I2C_SW_OK) {
        cells->valid = 0U;
        return BQ76940_I2C_ERROR;
    }

    /* 解析 14 位 ADC 值 → mV (使用校准值) */
    cells->max_mv = 0U;
    cells->min_mv = 0xFFFFU;
    cells->total_mv = 0U;

    for (uint8_t i = 0U; i < BQ76940_CELL_COUNT; i++) {
        uint8_t lo = buf[i * 2U];
        uint8_t hi = buf[i * 2U + 1U];

        /* 14-bit: hi[5:0] | lo[7:0] */
        uint16_t adc = (uint16_t)(((uint16_t)(hi & 0x3FU) << 8U) | lo);
        if (adc > BQ76940_ADC_MAX) { adc = BQ76940_ADC_MAX; }

        /* V_mV = (GAIN × ADC) / 1000 + OFFSET */
        int32_t mv = (gain * (int32_t)adc) / 1000L + (int32_t)offset;
        if (mv < 0) { mv = 0; }

        cells->cell_mv[i] = (uint16_t)mv;
        cells->total_mv += (uint32_t)mv;

        if ((uint16_t)mv > cells->max_mv) { cells->max_mv = (uint16_t)mv; }
        if ((uint16_t)mv < cells->min_mv) { cells->min_mv = (uint16_t)mv; }
    }

    cells->diff_mv = (uint16_t)(cells->max_mv - cells->min_mv);
    cells->valid = 1U;

    return BQ76940_OK;
}

/* ============================================================
 * 温度传感器 (NTC 分压 → 查表插值)
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

        /* ADC → NTC 温度 (0.1°C) */
        temps->ts_mdeg_c[i] = adc_to_ntc_temp(adc);
    }

    temps->valid = 1U;
    return BQ76940_OK;
}

/* ============================================================
 * 电流 / 库仑计 (可配置检流电阻)
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

    /* CC 为 16 位有符号值 */
    int16_t cc_raw = (int16_t)(((uint16_t)buf[1] << 8U) | buf[0]);

    /* I_mA = CC_raw × 844 / (R_sense_mΩ × 100)
     * 注意: 844 = 8.44μV × 100, 避免浮点 */
    int32_t denom = (int32_t)((uint32_t)g_r_sense_mohm * 100U);
    if (denom == 0) { denom = 100; }  /* 安全: 默认 1mΩ */

    current->current_ma  = (int32_t)cc_raw * 844L / denom;
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
 * 保护配置 (OV/UV/OCD/SCD + 延时寄存器)
 * ============================================================ */

bq76940_status_t bq76940_set_protection(const bq76940_cfg_t *cfg)
{
    if (cfg == NULL) {
        return BQ76940_ERROR;
    }

    int32_t gain = (int32_t)g_calib.gain_uv_per_lsb;

    /* 过压阈值: OV_TRIP = V_ov(mV) × 1000 / GAIN */
    if (cfg->cell_ov_mv > 0U) {
        uint16_t ov_adc = (uint16_t)((uint32_t)cfg->cell_ov_mv * 1000U
                                     / (uint32_t)gain);
        if (ov_adc > BQ76940_ADC_MAX) { ov_adc = BQ76940_ADC_MAX; }

        uint8_t ov_lo = (uint8_t)(ov_adc & 0xFFU);
        uint8_t ov_hi = (uint8_t)((ov_adc >> 8U) & 0xFFU);

        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OV_TRIP,     ov_lo);
        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OV_TRIP + 1U, ov_hi);
    }

    /* 欠压阈值 */
    if (cfg->cell_uv_mv > 0U) {
        uint16_t uv_adc = (uint16_t)((uint32_t)cfg->cell_uv_mv * 1000U
                                     / (uint32_t)gain);
        if (uv_adc > BQ76940_ADC_MAX) { uv_adc = BQ76940_ADC_MAX; }

        uint8_t uv_lo = (uint8_t)(uv_adc & 0xFFU);
        uint8_t uv_hi = (uint8_t)((uv_adc >> 8U) & 0xFFU);

        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_UV_TRIP,     uv_lo);
        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_UV_TRIP + 1U, uv_hi);
    }

    /* 过流放电阈值: OCD_TRIP = I_ocd(mA) × R_sense(mΩ) / 8.44μV
     * 简化为: OCD_raw = I_ocd × R_sense / 844 × 100 */
    if (cfg->discharge_oc_ma > 0U && g_r_sense_mohm > 0U) {
        uint16_t ocd_raw = (uint16_t)((uint32_t)cfg->discharge_oc_ma
                                      * (uint32_t)g_r_sense_mohm
                                      * 100U / 844U);

        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OCD_TRIP,
                         (uint8_t)(ocd_raw & 0xFFU));
        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OCD_TRIP + 1U,
                         (uint8_t)((ocd_raw >> 8U) & 0xFFU));
    }

    /* 短路放电阈值 */
    if (cfg->charge_oc_ma > 0U) {
        /* 短路电流通常远大于 OCD, 使用粗略估算 */
        uint16_t scd_raw = 0x0010U;  /* 默认最小值 */
        (void)cfg->charge_oc_ma;     /* 使用字段保持接口兼容 */

        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_SCD_TRIP,
                         (uint8_t)(scd_raw & 0xFFU));
        i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_SCD_TRIP + 1U,
                         (uint8_t)((scd_raw >> 8U) & 0xFFU));
    }

    /* 延时寄存器 (写默认值) */
    i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OV_DELAY,  0x80U);  /* ~1s */
    i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_UV_DELAY,  0x80U);  /* ~1s */
    i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_OCD_DELAY, 0x08U);  /* ~8ms */
    i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_SCD_DELAY, 0x01U);  /* ~100μs */

    return BQ76940_OK;
}

bq76940_status_t bq76940_clear_faults(void)
{
    /* 向 SYS_STAT 写入 0xFF 清除所有状态位 */
    return bq76940_reg_write(BQ76940_REG_SYS_STAT, 0xFFU);
}

/* ============================================================
 * 电池均衡 (地址修正: CELLBAL1=0x06, CELLBAL2=0x07)
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
        case I2C_SW_OK:        return BQ76940_OK;
        case I2C_SW_NACK:      return BQ76940_I2C_ERROR;
        case I2C_SW_CRC_ERROR: return BQ76940_CRC_ERROR;
        default:               return BQ76940_ERROR;
    }
}

bq76940_status_t bq76940_reg_read(uint8_t reg, uint8_t *data)
{
    i2c_sw_status_t ret = i2c_sw_read_crc(BQ76940_I2C_ADDR, reg, data);

    switch (ret) {
        case I2C_SW_OK:        return BQ76940_OK;
        case I2C_SW_NACK:      return BQ76940_I2C_ERROR;
        case I2C_SW_CRC_ERROR: return BQ76940_CRC_ERROR;
        default:               return BQ76940_ERROR;
    }
}
