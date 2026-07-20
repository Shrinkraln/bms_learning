/**
 * @file    i2c_sw.c
 * @brief   BSP 层软件 I2C + CRC8 实现
 * @note    用 GPIO 位带操作模拟 I2C 总线时序
 *          - 不使用硬件 I2C 外设，完全由软件控制
 *          - 所有延时基于 bsp_delay_us()（DWT 周期计数器）
 */

#include "i2c_sw.h"
#include "systick.h"

/* ============================================================
 * 低层 GPIO 操作宏
 *
 * 软件 I2C 关键技巧：
 * SCL/SDA 始终配置为开漏输出（不切换模式）。
 * "读取" SDA 时：先写 ODR=1（释放总线），然后读 IDR。
 * 这是标准做法，避免了运行时重新配置 GPIO 模式的开销。
 * ============================================================ */

/* SCL 控制 — 始终输出 */
#define SCL_H()   (I2C_SW_SCL_PORT->BSRR = I2C_SW_SCL_PIN)
#define SCL_L()   (I2C_SW_SCL_PORT->BRR  = I2C_SW_SCL_PIN)

/* SDA 控制 — 开漏：写 1 释放总线（ODR 高 → 由外部上拉），写 0 拉低 */
#define SDA_H()   (I2C_SW_SDA_PORT->BSRR = I2C_SW_SDA_PIN)
#define SDA_L()   (I2C_SW_SDA_PORT->BRR  = I2C_SW_SDA_PIN)

/* SDA 读取 — 读取输入数据寄存器（开漏输出下始终有效） */
#define SDA_READ() ((I2C_SW_SDA_PORT->IDR & I2C_SW_SDA_PIN) ? 1U : 0U)

/* ============================================================
 * 初始化
 * ============================================================ */

void i2c_sw_init(void)
{
    /* 确保 tick 系统已初始化（DWT 用于微秒延时） */
    bsp_tick_init();

    /* 配置 SCL/SDA 为开漏输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = I2C_SW_SCL_PIN | I2C_SW_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_SW_SCL_PORT, &GPIO_InitStruct);

    /* 释放总线（高电平） */
    SCL_H();
    SDA_H();
    bsp_delay_us(I2C_SW_DELAY_US);
}

/* ============================================================
 * 低层时序操作
 * ============================================================ */

static void i2c_start(void)
{
    SDA_H();
    SCL_H();
    bsp_delay_us(I2C_SW_DELAY_US);
    SDA_L();
    bsp_delay_us(I2C_SW_DELAY_US);
    SCL_L();
}

static void i2c_stop(void)
{
    SDA_L();
    SCL_H();
    bsp_delay_us(I2C_SW_DELAY_US);
    SDA_H();
    bsp_delay_us(I2C_SW_DELAY_US);
}

/**
 * @brief  发送一个字节，返回是否收到 ACK
 * @param  byte  要发送的字节
 * @retval 0 = ACK, 1 = NACK
 */
static uint8_t i2c_send_byte(uint8_t byte)
{
    /* 发送 8 位，MSB 先出 */
    for (uint8_t i = 0U; i < 8U; i++) {
        if (byte & 0x80U) {
            SDA_H();
        } else {
            SDA_L();
        }
        bsp_delay_us(I2C_SW_DELAY_US);
        SCL_H();
        bsp_delay_us(I2C_SW_DELAY_US);
        SCL_L();
        byte <<= 1U;
    }

    /* 释放 SDA，读取 ACK */
    SDA_H();  /* 释放 SDA（开漏），准备读取 ACK 位 */
    SCL_H();
    bsp_delay_us(I2C_SW_DELAY_US);
    uint8_t ack = (SDA_READ() == GPIO_PIN_SET) ? 1U : 0U;
    SCL_L();

    return ack;
}

/**
 * @brief  接收一个字节
 * @param  ack  0 = 主机发送 ACK, 1 = 主机发送 NACK
 * @retval 接收到的字节
 */
static uint8_t i2c_recv_byte(uint8_t ack)
{
    uint8_t byte = 0U;

    SDA_H();  /* 释放 SDA（开漏），准备读取数据位 */

    for (uint8_t i = 0U; i < 8U; i++) {
        byte <<= 1U;
        SCL_H();
        bsp_delay_us(I2C_SW_DELAY_US);
        if (SDA_READ() == GPIO_PIN_SET) {
            byte |= 1U;
        }
        SCL_L();
        bsp_delay_us(I2C_SW_DELAY_US);
    }

    /* 发送 ACK / NACK */
    if (ack) {
        SDA_H();  /* NACK */
    } else {
        SDA_L();  /* ACK  */
    }
    SCL_H();
    bsp_delay_us(I2C_SW_DELAY_US);
    SCL_L();

    return byte;
}

/* ============================================================
 * 公共 API — 探测
 * ============================================================ */

i2c_sw_status_t i2c_sw_probe(uint8_t dev_addr)
{
    i2c_start();
    uint8_t ack = i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U);  /* W */
    i2c_stop();

    return (ack == 0U) ? I2C_SW_OK : I2C_SW_NACK;
}

/* ============================================================
 * 公共 API — 单字节读写
 * ============================================================ */

i2c_sw_status_t i2c_sw_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data)
{
    i2c_start();

    /* 发送设备地址 (W) */
    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 发送寄存器地址 */
    if (i2c_send_byte(reg)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 发送数据 */
    if (i2c_send_byte(data)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    i2c_stop();
    return I2C_SW_OK;
}

i2c_sw_status_t i2c_sw_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data)
{
    if (data == NULL) {
        return I2C_SW_ERROR;
    }

    i2c_start();

    /* 发送设备地址 (W) — 写寄存器地址 */
    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    if (i2c_send_byte(reg)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 重复起始 + 设备地址 (R) */
    i2c_start();
    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 1U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 读取数据 + NACK */
    *data = i2c_recv_byte(1U);

    i2c_stop();
    return I2C_SW_OK;
}

/* ============================================================
 * 公共 API — 批量读写
 * ============================================================ */

i2c_sw_status_t i2c_sw_write_buf(uint8_t dev_addr, uint8_t reg,
                                  const uint8_t *buf, uint8_t len)
{
    if (buf == NULL || len == 0U) {
        return I2C_SW_ERROR;
    }

    i2c_start();

    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    if (i2c_send_byte(reg)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    for (uint8_t i = 0U; i < len; i++) {
        if (i2c_send_byte(buf[i])) {
            i2c_stop();
            return I2C_SW_NACK;
        }
    }

    i2c_stop();
    return I2C_SW_OK;
}

i2c_sw_status_t i2c_sw_read_buf(uint8_t dev_addr, uint8_t reg,
                                 uint8_t *buf, uint8_t len)
{
    if (buf == NULL || len == 0U) {
        return I2C_SW_ERROR;
    }

    i2c_start();

    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    if (i2c_send_byte(reg)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    i2c_start();
    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 1U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 连续读取，最后一个字节前发 ACK，最后一字节发 NACK */
    for (uint8_t i = 0U; i < len; i++) {
        uint8_t ack = (i == (len - 1U)) ? 1U : 0U;
        buf[i] = i2c_recv_byte(ack);
    }

    i2c_stop();
    return I2C_SW_OK;
}

/* ============================================================
 * CRC8 — 多项式 0x07 (x^8 + x^2 + x + 1)
 * ============================================================ */

uint8_t i2c_sw_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00U;

    for (uint8_t i = 0U; i < len; i++) {
        crc ^= data[i];

        for (uint8_t j = 0U; j < 8U; j++) {
            if (crc & 0x80U) {
                crc = (uint8_t)(crc << 1U) ^ 0x07U;
            } else {
                crc <<= 1U;
            }
        }
    }

    return crc;
}

/* ============================================================
 * 带 CRC8 的读写（BQ76940 专用）
 * ============================================================ */

i2c_sw_status_t i2c_sw_write_crc(uint8_t dev_addr, uint8_t reg, uint8_t data)
{
    /* 计算 CRC8: 覆盖 {REG, DATA} */
    uint8_t crc_buf[2] = { reg, data };
    uint8_t crc = i2c_sw_crc8(crc_buf, 2U);

    /* 发送: REG + DATA + CRC */
    i2c_start();

    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    if (i2c_send_byte(reg))   { i2c_stop(); return I2C_SW_NACK; }
    if (i2c_send_byte(data))  { i2c_stop(); return I2C_SW_NACK; }
    if (i2c_send_byte(crc))   { i2c_stop(); return I2C_SW_NACK; }

    i2c_stop();
    return I2C_SW_OK;
}

i2c_sw_status_t i2c_sw_read_crc(uint8_t dev_addr, uint8_t reg, uint8_t *data)
{
    if (data == NULL) {
        return I2C_SW_ERROR;
    }

    i2c_start();

    /* 写寄存器地址 */
    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 0U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }
    if (i2c_send_byte(reg)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 重复起始 + 读 */
    i2c_start();
    if (i2c_send_byte((uint8_t)(dev_addr << 1U) | 1U)) {
        i2c_stop();
        return I2C_SW_NACK;
    }

    /* 读取 DATA + CRC（共 2 字节）*/
    uint8_t rx_data = i2c_recv_byte(0U);   /* DATA, ACK  */
    uint8_t rx_crc  = i2c_recv_byte(1U);   /* CRC,  NACK */

    i2c_stop();

    /* 验证 CRC8: 覆盖 {DATA} */
    uint8_t calc_crc = i2c_sw_crc8(&rx_data, 1U);
    if (calc_crc != rx_crc) {
        return I2C_SW_CRC_ERROR;
    }

    *data = rx_data;
    return I2C_SW_OK;
}
