/**
 * @file    bq76940.h
 * @brief   BSP 层 BQ76940 电池监视器驱动
 * @note    适用于 BQ769x0 系列 (BQ76920/30/40)，9 串电池配置
 *          - 通信: 软件 I2C + CRC8 校验
 *          - 支持: 电池电压、温度、电流、保护状态、均衡控制
 *          - 参考: TI BQ76940 datasheet (SLUSC25B)
 *
 * 使用前需:
 *   1. 初始化 i2c_sw_init()
 *   2. 调用 bq76940_init() 配置芯片
 *   3. 周期性调用 bq76940_read_all() 更新数据
 */

#ifndef __BSP_BQ76940_H
#define __BSP_BQ76940_H

#include "stm32f1xx_hal.h"

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

/** @brief 14 位 ADC 的分辨率 (μV/LSB, gain=1) */
#define BQ76940_ADC_UV_PER_LSB  382U

/* ============================================================
 * 寄存器地址定义
 * ============================================================ */

/** @brief 系统寄存器 */
#define BQ76940_REG_SYS_STAT     0x00U   /**< 系统状态                     */
#define BQ76940_REG_CC_CFG       0x0BU   /**< 库仑计配置                   */
#define BQ76940_REG_SYS_CTRL1    0x04U   /**< 系统控制 1                   */
#define BQ76940_REG_SYS_CTRL2    0x05U   /**< 系统控制 2                   */

/** @brief 均衡寄存器 */
#define BQ76940_REG_CELLBAL1     0x06U   /**< 电芯均衡 1 (VC1-VC5)         */
#define BQ76940_REG_CELLBAL2     0x07U   /**< 电芯均衡 2 (VC6-VC10)        */

/** @brief 保护状态 */
#define BQ76940_REG_PROTECT1     0x6AU   /**< 保护状态 1 (SCD/OCD)         */
#define BQ76940_REG_PROTECT2     0x6BU   /**< 保护状态 2 (OV/UV)           */

/** @brief 保护阈值 */
#define BQ76940_REG_OV_TRIP      0x08U   /**< 过压触发阈值 (14-bit)        */
#define BQ76940_REG_UV_TRIP      0x0AU   /**< 欠压触发阈值 (14-bit)        */
#define BQ76940_REG_OCD_TRIP     0x0CU   /**< 过流放电触发阈值              */
#define BQ76940_REG_SCD_TRIP     0x0EU   /**< 短路放电触发阈值              */

/** @brief 电芯电压寄存器基地址 */
#define BQ76940_REG_VC1_LO       0x20U   /**< VC1 电压低字节                */

/** @brief 总电压 */
#define BQ76940_REG_BAT_LO       0x34U   /**< 总电压低字节 (BAT)           */

/** @brief 温度传感器 */
#define BQ76940_REG_TS1_LO       0x36U   /**< TS1 温度低字节               */

/** @brief 库仑计 */
#define BQ76940_REG_CC_LO        0x3CU   /**< 库仑计低字节                 */

/** @brief ADC 增益 / 偏移 */
#define BQ76940_REG_ADCGAIN1     0x50U   /**< ADC 增益 1                   */
#define BQ76940_REG_ADCOFFSET    0x52U   /**< ADC 偏移                     */

/* ============================================================
 * 系统控制位定义
 * ============================================================ */

#define BQ76940_SYS_CTRL1_ADC_EN     (1U << 0U)  /**< ADC 使能      */
#define BQ76940_SYS_CTRL1_CC_EN      (1U << 1U)  /**< 库仑计使能    */
#define BQ76940_SYS_CTRL1_CC_ONESHOT (1U << 3U)  /**< 库仑计单次    */

#define BQ76940_SYS_CTRL2_CC_RST     (1U << 0U)  /**< 库仑计复位    */

/** @brief 系统状态位 */
#define BQ76940_STAT_OV      (1U << 0U)  /**< 过压              */
#define BQ76940_STAT_UV      (1U << 1U)  /**< 欠压              */
#define BQ76940_STAT_OCD     (1U << 2U)  /**< 过流放电          */
#define BQ76940_STAT_SCD     (1U << 3U)  /**< 短路放电          */
#define BQ76940_STAT_DEV_XRDY (1U << 7U) /**< 设备就绪          */

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

/** @brief 电芯电压数据 */
typedef struct {
    uint16_t cell_mv[BQ76940_CELL_COUNT];  /**< 各电芯电压 (mV)             */
    uint32_t total_mv;                      /**< 总电压 (mV)                 */
    uint16_t max_mv;                        /**< 最高单芯电压 (mV)           */
    uint16_t min_mv;                        /**< 最低单芯电压 (mV)           */
    uint16_t diff_mv;                       /**< 电芯压差 (mV)               */
    uint8_t  valid;                         /**< 数据有效标志                */
} bq76940_cell_data_t;

/** @brief 温度数据 */
typedef struct {
    uint16_t ts_mdeg_c[BQ76940_TS_COUNT];   /**< 各通道温度 (0.1°C)         */
    uint8_t  valid;                         /**< 数据有效标志                */
} bq76940_temp_data_t;

/** @brief 电流/库仑计数据 */
typedef struct {
    int32_t  current_ma;                    /**< 电流 (mA), 充电+/放电-     */
    uint32_t coulomb_mah;                   /**< 累计电量 (mAh)             */
    uint8_t  valid;                         /**< 数据有效标志                */
} bq76940_current_data_t;

/** @brief 保护状态 */
typedef struct {
    uint8_t ov     : 1;   /**< 过压 (Over-Voltage)                        */
    uint8_t uv     : 1;   /**< 欠压 (Under-Voltage)                       */
    uint8_t ocd    : 1;   /**< 过流放电 (Over-Current Discharge)          */
    uint8_t scd    : 1;   /**< 短路放电 (Short-Circuit Discharge)         */
    uint8_t oc_c   : 1;   /**< 过流充电 (Over-Current Charge)             */
    uint8_t ot     : 1;   /**< 过温 (Over-Temperature)                    */
    uint8_t ut     : 1;   /**< 欠温 (Under-Temperature)                   */
    uint8_t dev_xrdy : 1; /**< 设备就绪                                    */
} bq76940_fault_t;

/** @brief 保护阈值配置 */
typedef struct {
    uint16_t ov_mv;        /**< 过压阈值 (mV), 0 = 使用默认               */
    uint16_t uv_mv;        /**< 欠压阈值 (mV), 0 = 使用默认               */
    uint16_t ocd_ma;       /**< 过流放电阈值 (mA), 0 = 使用默认           */
    uint16_t scd_ma;       /**< 短路放电阈值 (mA), 0 = 使用默认           */
    uint16_t ov_delay_ms;  /**< 过压延时 (ms)                             */
    uint16_t uv_delay_ms;  /**< 欠压延时 (ms)                             */
    uint16_t ocd_delay_ms; /**< 过流放电延时 (ms)                         */
    uint16_t scd_delay_us; /**< 短路放电延时 (μs)                         */
} bq76940_protection_cfg_t;

/** @brief 完整 BMS 数据快照 */
typedef struct {
    bq76940_cell_data_t    cells;
    bq76940_temp_data_t    temps;
    bq76940_current_data_t current;
    bq76940_fault_t        faults;
    uint32_t               timestamp_ms;  /**< 采样时间戳 */
} bq76940_data_t;

/* ============================================================
 * API — 初始化和系统控制
 * ============================================================ */

/**
 * @brief  初始化 BQ76940
 * @note   配置 ADC、库仑计、默认保护阈值
 *         等待 DEV_XRDY 标志就绪
 * @param  cfg  保护配置指针，NULL = 使用默认值
 * @retval 操作状态
 */
bq76940_status_t bq76940_init(const bq76940_protection_cfg_t *cfg);

/**
 * @brief  检查 BQ76940 是否在线
 * @retval 1 = 在线, 0 = 不在线
 */
uint8_t bq76940_is_online(void);

/**
 * @brief  将 BQ76940 进入关机模式
 * @retval 操作状态
 */
bq76940_status_t bq76940_shutdown(void);

/**
 * @brief  唤醒 BQ76940（硬件脉冲唤醒引脚）
 * @note   需要 MCU_WAKE_BQ 引脚已配置
 */
void bq76940_wakeup(void);

/* ============================================================
 * API — 数据采集
 * ============================================================ */

/**
 * @brief  一次性读取所有数据（电压、温度、电流、故障）
 * @note   典型的单次采样 ~10ms（9 电芯 + 3 温度 + 电流 + 状态）
 * @param  data  数据输出结构指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_read_all(bq76940_data_t *data);

/**
 * @brief  读取电芯电压
 * @param  cells  电压数据输出指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_read_cells(bq76940_cell_data_t *cells);

/**
 * @brief  读取温度传感器
 * @param  temps  温度数据输出指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_read_temps(bq76940_temp_data_t *temps);

/**
 * @brief  读取电流和库仑计
 * @param  current  电流数据输出指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_read_current(bq76940_current_data_t *current);

/**
 * @brief  读取保护故障状态
 * @param  faults  故障状态输出指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_read_faults(bq76940_fault_t *faults);

/* ============================================================
 * API — 保护配置
 * ============================================================ */

/**
 * @brief  设置保护阈值
 * @param  cfg  保护配置指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_set_protection(const bq76940_protection_cfg_t *cfg);

/**
 * @brief  清除所有保护锁存
 * @retval 操作状态
 */
bq76940_status_t bq76940_clear_faults(void);

/* ============================================================
 * API — 电池均衡
 * ============================================================ */

/**
 * @brief  设置电芯均衡状态
 * @param  balance_mask  均衡位掩码 [bit0=VC1, bit8=VC9]
 *                       1 = 开启均衡, 0 = 关闭均衡
 * @retval 操作状态
 */
bq76940_status_t bq76940_set_balancing(uint16_t balance_mask);

/**
 * @brief  读取当前均衡状态
 * @retval 均衡位掩码
 */
uint16_t bq76940_get_balancing(void);

/**
 * @brief  关闭所有均衡
 * @retval 操作状态
 */
bq76940_status_t bq76940_balance_off(void);

/* ============================================================
 * API — 低层寄存器访问（调试用）
 * ============================================================ */

/**
 * @brief  直接写入寄存器（带 CRC8）
 * @param  reg   寄存器地址
 * @param  data  数据值
 * @retval 操作状态
 */
bq76940_status_t bq76940_reg_write(uint8_t reg, uint8_t data);

/**
 * @brief  直接读取寄存器（带 CRC8 验证）
 * @param  reg   寄存器地址
 * @param  data  数据输出指针
 * @retval 操作状态
 */
bq76940_status_t bq76940_reg_read(uint8_t reg, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BQ76940_H */
