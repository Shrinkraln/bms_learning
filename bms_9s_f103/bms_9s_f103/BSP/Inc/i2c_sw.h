/**
 * @file    i2c_sw.h
 * @brief   BSP 层软件 I2C 驱动 + CRC8 校验
 * @note    用 GPIO 模拟 I2C 时序，用于 BQ76940 等需要 CRC8 的 I2C 从设备
 *          - 支持标准模式 (~100kHz)
 *          - 内置 CRC8 计算（多项式 0x07，兼容 BQ76940）
 *          - SCL/SDA 引脚通过宏配置，可在头文件中修改
 *          - 所有函数为阻塞式，适用于 FreeRTOS 任务上下文
 */

#ifndef __BSP_I2C_SW_H
#define __BSP_I2C_SW_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 引脚配置 (可根据实际硬件修改)
 * ============================================================ */

/** @brief 软件 I2C SCL 引脚 — PB10 (与 HW I2C1 PB8/PB9 错开) */
#define I2C_SW_SCL_PORT      GPIOB
#define I2C_SW_SCL_PIN       GPIO_PIN_10

/** @brief 软件 I2C SDA 引脚 — PB11 */
#define I2C_SW_SDA_PORT      GPIOB
#define I2C_SW_SDA_PIN       GPIO_PIN_11

/** @brief I2C 时序延时参数（72MHz 系统时钟下） */
#define I2C_SW_DELAY_US      5U    /**< 半周期延时 → ~100kHz SCL */

/* ============================================================
 * 类型定义
 * ============================================================ */

/** @brief I2C 操作状态 */
typedef enum {
    I2C_SW_OK           = 0x00U,
    I2C_SW_ERROR        = 0x01U,
    I2C_SW_TIMEOUT      = 0x02U,
    I2C_SW_NACK         = 0x03U,  /**< 从机未应答 */
    I2C_SW_BUSY         = 0x04U,
    I2C_SW_CRC_ERROR    = 0x05U,  /**< CRC 校验失败 */
} i2c_sw_status_t;

/* ============================================================
 * API — 初始化和基本操作
 * ============================================================ */

/**
 * @brief  初始化软件 I2C 引脚
 * @note   配置 SCL/SDA 为开漏输出，释放总线（高电平）
 */
void i2c_sw_init(void);

/**
 * @brief  扫描 I2C 总线，检查指定地址是否有设备应答
 * @param  dev_addr  7 位设备地址（不含 R/W 位）
 * @retval I2C_SW_OK = 设备存在, I2C_SW_NACK = 无应答
 */
i2c_sw_status_t i2c_sw_probe(uint8_t dev_addr);

/**
 * @brief  向从设备写入单字节寄存器
 * @param  dev_addr  7 位设备地址
 * @param  reg       寄存器地址
 * @param  data      要写入的数据
 * @retval 操作状态
 */
i2c_sw_status_t i2c_sw_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data);

/**
 * @brief  从从设备读取单字节寄存器
 * @param  dev_addr  7 位设备地址
 * @param  reg       寄存器地址
 * @param  data      读取数据输出缓冲区
 * @retval 操作状态
 */
i2c_sw_status_t i2c_sw_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data);

/**
 * @brief  批量写入寄存器
 * @param  dev_addr  7 位设备地址
 * @param  reg       起始寄存器地址
 * @param  buf       数据缓冲区
 * @param  len       数据长度（字节）
 * @retval 操作状态
 */
i2c_sw_status_t i2c_sw_write_buf(uint8_t dev_addr, uint8_t reg,
                                  const uint8_t *buf, uint8_t len);

/**
 * @brief  批量读取寄存器
 * @param  dev_addr  7 位设备地址
 * @param  reg       起始寄存器地址
 * @param  buf       数据缓冲区
 * @param  len       读取长度（字节）
 * @retval 操作状态
 */
i2c_sw_status_t i2c_sw_read_buf(uint8_t dev_addr, uint8_t reg,
                                 uint8_t *buf, uint8_t len);

/* ============================================================
 * API — CRC8 校验 (BQ76940 兼容)
 * ============================================================ */

/**
 * @brief  计算 CRC8
 * @note   多项式 0x07 (x^8 + x^2 + x + 1)，初始值 0x00
 *         兼容 BQ76940 / BQ769x0 系列的 CRC8 算法
 * @param  data  数据缓冲区指针
 * @param  len   数据长度
 * @retval CRC8 校验值（1 字节）
 */
uint8_t i2c_sw_crc8(const uint8_t *data, uint8_t len);

/**
 * @brief  带 CRC8 的写入操作（BQ76940 格式）
 * @note   数据格式: [DEV_ADDR<<1|W] [REG] [DATA...] [CRC8]
 *         CRC8 覆盖 REG + DATA
 * @param  dev_addr  7 位设备地址
 * @param  reg       寄存器地址
 * @param  data      数据字节
 * @retval 操作状态（含 CRC 校验）
 */
i2c_sw_status_t i2c_sw_write_crc(uint8_t dev_addr, uint8_t reg, uint8_t data);

/**
 * @brief  带 CRC8 的读取操作（BQ76940 格式）
 * @note   读取格式: [REG] 后跟 [DATA...] [CRC8]
 *         CRC8 覆盖 DATA
 * @param  dev_addr  7 位设备地址
 * @param  reg       寄存器地址
 * @param  data      读取数据输出
 * @retval 操作状态（自动验证 CRC8）
 */
i2c_sw_status_t i2c_sw_read_crc(uint8_t dev_addr, uint8_t reg, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_I2C_SW_H */
