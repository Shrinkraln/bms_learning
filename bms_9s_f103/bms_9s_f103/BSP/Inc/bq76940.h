/**
 * @file    bq76940.h
 * @brief   BSP 层 BQ76940 电池监视器驱动
 * @note    适用于 BQ769x0 系列 (BQ76920/30/40)，9 串电池配置
 *          - 通信: 软件 I2C + CRC8 校验
 *          - 支持: 电池电压、NTC 温度、电流、保护状态、均衡控制、FET 控制
 *          - ADC 校准: 读取出厂 GAIN/OFFSET，电压 = (GAIN×ADC)/1000 + OFFSET
 *          - 温度: NTC 103AT 分压→电阻→查表插值 (33 点, -40~120°C)
 *          - 参考: TI BQ76940 datasheet (SLUSC25B)
 */

#ifndef BSP_BQ76940_H
#define BSP_BQ76940_H

#include "stm32f1xx_hal.h"
#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 芯片配置
 * ============================================================ */

/** @brief BQ76940 7 位 I2C 地址 */
#define BQ76940_I2C_ADDR        0x08U

/** @brief 电芯数量 (9 串) */
#define BQ76940_CELL_COUNT      9U

/** @brief 温度传感器数量 */
#define BQ76940_TS_COUNT        3U

/** @brief ADC 参考电压 (mV) */
#define BQ76940_VREF_MV         3300U

/** @brief 默认 ADC 增益 (μV/LSB)，校准失败时回退 */
#define BQ76940_ADC_UV_PER_LSB  382U

/** @brief ADC 满量程 (14-bit) */
#define BQ76940_ADC_MAX         16383U

/** @brief 默认检流电阻 (mΩ) */
#define BQ76940_DEFAULT_R_SENSE_MOHM  1U

/* ============================================================
 * 寄存器地址定义
 * ============================================================ */

#define BQ76940_REG_SYS_STAT     0x00U   /**< 系统状态                     */
#define BQ76940_REG_CELLBAL1     0x06U   /**< 电芯均衡 1 (VC1-VC5)         */
#define BQ76940_REG_CELLBAL2     0x07U   /**< 电芯均衡 2 (VC6-VC10)        */
#define BQ76940_REG_SYS_CTRL1    0x04U   /**< 系统控制 1                   */
#define BQ76940_REG_SYS_CTRL2    0x05U   /**< 系统控制 2                   */
#define BQ76940_REG_CC_CFG       0x0BU   /**< 库仑计配置                   */

#define BQ76940_REG_OV_TRIP      0x08U   /**< 过压触发阈值 (14-bit)        */
#define BQ76940_REG_UV_TRIP      0x0AU   /**< 欠压触发阈值 (14-bit)        */
#define BQ76940_REG_OCD_TRIP     0x0CU   /**< 过流放电触发阈值              */
#define BQ76940_REG_SCD_TRIP     0x0EU   /**< 短路放电触发阈值              */

#define BQ76940_REG_OV_DELAY     0x09U   /**< 过压延时                      */
#define BQ76940_REG_UV_DELAY     0x0BU   /**< 欠压延时 (注: 与 UV_TRIP+1 同地址) */
#define BQ76940_REG_OCD_DELAY    0x0DU   /**< 过流放电延时                  */
#define BQ76940_REG_SCD_DELAY    0x0FU   /**< 短路放电延时                  */

#define BQ76940_REG_VC1_LO       0x20U   /**< VC1 电压低字节                */
#define BQ76940_REG_BAT_LO       0x34U   /**< 总电压低字节 (BAT)           */
#define BQ76940_REG_TS1_LO       0x36U   /**< TS1 温度低字节               */
#define BQ76940_REG_CC_LO        0x3CU   /**< 库仑计低字节                 */

#define BQ76940_REG_ADCGAIN1     0x50U   /**< ADC 增益 1                   */
#define BQ76940_REG_ADCGAIN2     0x59U   /**< ADC 增益 2 (仅 GAIN bit)     */
#define BQ76940_REG_ADCOFFSET    0x51U   /**< ADC 偏移                     */

#define BQ76940_REG_PROTECT1     0x6AU   /**< 保护状态 1 (SCD/OCD)         */
#define BQ76940_REG_PROTECT2     0x6BU   /**< 保护状态 2 (OV/UV)           */

/* ============================================================
 * 系统控制位定义
 * ============================================================ */

#define BQ76940_SYS_CTRL1_ADC_EN     (1U << 0U)  /**< ADC 使能              */
#define BQ76940_SYS_CTRL1_CC_EN      (1U << 1U)  /**< 库仑计使能            */
#define BQ76940_SYS_CTRL1_TEMP_SEL   (1U << 3U)  /**< 温度传感器选择 (1=外部NTC) */
#define BQ76940_SYS_CTRL1_ADC_EN2    (1U << 4U)  /**< ADC 使能 2            */
#define BQ76940_SYS_CTRL1_CC_ONESHOT (1U << 6U)  /**< 库仑计单次            */

#define BQ76940_SYS_CTRL2_CHG_FET    (1U << 0U)  /**< 充电 FET 控制          */
#define BQ76940_SYS_CTRL2_DSG_FET    (1U << 1U)  /**< 放电 FET 控制          */

/** @brief 系统状态位 */
#define BQ76940_STAT_OV      (1U << 0U)
#define BQ76940_STAT_UV      (1U << 1U)
#define BQ76940_STAT_OCD     (1U << 2U)
#define BQ76940_STAT_SCD     (1U << 3U)
#define BQ76940_STAT_DEV_XRDY (1U << 7U)

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief BQ76940 操作状态 */
typedef enum {
    BQ76940_OK           = 0x00U,
    BQ76940_ERROR        = 0x01U,
    BQ76940_I2C_ERROR    = 0x02U,
    BQ76940_CRC_ERROR    = 0x03U,
    BQ76940_TIMEOUT      = 0x04U,
    BQ76940_NOT_READY    = 0x05U,
} bq76940_status_t;

/** @brief ADC 校准数据（芯片出厂编程） */
typedef struct {
    uint16_t gain_uv_per_lsb;    /**< μV/LSB, 365-396                     */
    int16_t  offset_mv;          /**< mV, 2's complement                  */
} bq76940_calib_t;

/** @brief 初始化配置（合并保护阈值 + 检流电阻） */
typedef struct {
    uint16_t r_sense_mohm;       /**< 检流电阻 (mΩ), 0=默认 1mΩ          */
    uint16_t cell_ov_mv;         /**< 过压阈值 (mV), 0=默认 4250          */
    uint16_t cell_uv_mv;         /**< 欠压阈值 (mV), 0=默认 2800          */
    uint16_t discharge_oc_ma;    /**< 放电过流 (mA), 0=默认 30000         */
    uint16_t charge_oc_ma;       /**< 充电过流 (mA), 0=默认 15000         */
} bq76940_cfg_t;

/** @brief 电芯电压数据 */
typedef struct {
    uint16_t cell_mv[BQ76940_CELL_COUNT];  /**< 各电芯电压 (mV)           */
    uint32_t total_mv;                      /**< 总电压 (mV)               */
    uint16_t max_mv;                        /**< 最高单芯电压 (mV)         */
    uint16_t min_mv;                        /**< 最低单芯电压 (mV)         */
    uint16_t diff_mv;                       /**< 电芯压差 (mV)             */
    uint8_t  valid;                         /**< 数据有效标志              */
} bq76940_cell_data_t;

/** @brief 温度数据（支持负温） */
typedef struct {
    int16_t  ts_mdeg_c[BQ76940_TS_COUNT];   /**< 各通道温度 (0.1°C)       */
    uint8_t  valid;                         /**< 数据有效标志              */
} bq76940_temp_data_t;

/** @brief 电流/库仑计数据 */
typedef struct {
    int32_t  current_ma;                    /**< 电流 (mA), 充电+/放电-   */
    uint32_t coulomb_mah;                   /**< 累计电量 (mAh)           */
    uint8_t  valid;                         /**< 数据有效标志              */
} bq76940_current_data_t;

/** @brief 保护状态 */
typedef struct {
    uint8_t ov     : 1;   /**< 过压                                      */
    uint8_t uv     : 1;   /**< 欠压                                      */
    uint8_t ocd    : 1;   /**< 过流放电                                  */
    uint8_t scd    : 1;   /**< 短路放电                                  */
    uint8_t oc_c   : 1;   /**< 过流充电                                  */
    uint8_t ot     : 1;   /**< 过温                                      */
    uint8_t ut     : 1;   /**< 欠温                                      */
    uint8_t dev_xrdy : 1; /**< 设备就绪                                  */
} bq76940_fault_t;

/** @brief 完整 BMS 数据快照 */
typedef struct {
    bq76940_cell_data_t    cells;           /**< 电芯电压                  */
    bq76940_temp_data_t    temps;           /**< 温度数据                  */
    bq76940_current_data_t current;         /**< 电流数据                  */
    bq76940_fault_t        faults;          /**< 保护故障状态              */
    uint32_t               timestamp_ms;    /**< 采样时间戳                */
} bq76940_data_t;

/* ============================================================
 * API — 初始化和系统控制
 * ============================================================ */

bq76940_status_t bq76940_init(const bq76940_cfg_t *cfg);
uint8_t bq76940_is_online(void);
bq76940_status_t bq76940_shutdown(void);

/**
 * @brief  获取芯片出厂校准值
 * @retval 校准数据指针 (NULL = 尚未初始化)
 */
const bq76940_calib_t *bq76940_get_calib(void);

/**
 * @brief  写 SYS_CTRL2 寄存器（FET 控制）
 * @param  mask   位掩码 (BQ76940_SYS_CTRL2_CHG_FET | BQ76940_SYS_CTRL2_DSG_FET)
 * @param  value  位值 (1=开启, 0=关闭)
 * @retval 操作状态
 */
bq76940_status_t bq76940_write_sys_ctrl2(uint8_t mask, uint8_t value);

/* ============================================================
 * API — 数据采集
 * ============================================================ */

bq76940_status_t bq76940_read_all(bq76940_data_t *data);
bq76940_status_t bq76940_read_cells(bq76940_cell_data_t *cells);
bq76940_status_t bq76940_read_temps(bq76940_temp_data_t *temps);
bq76940_status_t bq76940_read_current(bq76940_current_data_t *current);
bq76940_status_t bq76940_read_faults(bq76940_fault_t *faults);

/* ============================================================
 * API — 保护配置
 * ============================================================ */

bq76940_status_t bq76940_set_protection(const bq76940_cfg_t *cfg);
bq76940_status_t bq76940_clear_faults(void);

/* ============================================================
 * API — 电池均衡
 * ============================================================ */

bq76940_status_t bq76940_set_balancing(uint16_t balance_mask);
uint16_t bq76940_get_balancing(void);
bq76940_status_t bq76940_balance_off(void);

/* ============================================================
 * API — 低层寄存器访问（调试用）
 * ============================================================ */

bq76940_status_t bq76940_reg_write(uint8_t reg, uint8_t data);
bq76940_status_t bq76940_reg_read(uint8_t reg, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BQ76940_H */
