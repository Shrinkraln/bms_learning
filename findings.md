# Findings & Decisions

## Requirements
- 使用 CubeMX + CMake + VSCode 工具链
- 主控: STM32F103C8T6 (64KB Flash, 20KB RAM, LQFP48)
- AFE: BQ76940 (TI电池前端，支持9-15串)
- 9串电池配置
- 引脚配置和控制逻辑与参考项目保持一致
- 参考项目路径: `datasheet/BQ76940发货资料 20220817/3.程序/1.BQ76940（程序加CAN20210915）改温度探头/BMS_s940/`

## Research Findings

### 1. 参考项目引脚映射表（完整）

**GPIOA:**
| 引脚 | 宏定义 | 方向 | 功能描述 |
|------|--------|------|---------|
| PA0 | MCU_KZ_QB_POWER | OUT | 控制QB电源开关 |
| PA1 | MCU_D_POWER | OUT | 控制D电源开关 |
| PA8 | MCU_WAKE_BQ | OUT | BQ76940唤醒信号 |
| PA9 | USART1_TX | AF | USART1发送（QT上位机）|
| PA10 | USART1_RX | AF | USART1接收（QT上位机）|
| PA12 | MCU_KZ_P2 | OUT | 控制P2开关 |

**GPIOB:**
| 引脚 | 宏定义 | 方向 | 功能描述 |
|------|--------|------|---------|
| PB0 | LED_D4? | OUT | LED指示灯 |
| PB1 | STA_WAKE | OUT | 唤醒状态指示 |
| PB2 | MCU_BQ1_ART | OUT | BQ76940 ALERT信号控制 |
| PB3 | PB3 | OUT | 通用IO（JTAG禁用后可用）|
| PB4 | PB4 | OUT | 通用IO（JTAG禁用后可用）|
| PB5 | PB5 | OUT | 通用IO |
| PB6 | I2C1_SCL(硬件) | - | 原为硬件I2C，但代码用软件I2C |
| PB7 | I2C1_SDA(硬件) | - | 同上 |
| PB8 | I2C1_SCL(软件) | OD | 软件模拟I2C时钟 → BQ76940 |
| PB9 | I2C1_SDA(软件) | OD | 软件模拟I2C数据 → BQ76940 |
| PB10 | I2C2_SCL/USART3_TX | - | 可能用于I2C2 |
| PB11 | I2C2_SDA/USART3_RX | - | 可能用于I2C2 |
| PB12 | MCU_IN_STA1 | OUT | 输入状态指示1 |
| PB13 | SPI2_SCK | AF | SPI Flash时钟 |
| PB14 | SPI2_MISO | AF | SPI Flash MISO |
| PB15 | SPI2_MOSI | AF | SPI Flash MOSI |

**GPIOC:**
| 引脚 | 宏定义 | 方向 | 功能描述 |
|------|--------|------|---------|
| PC13 | LED1 | OUT | LED指示灯 |

**其他关键配置:**
- CAN: PB8=CAN_RX, PB9=CAN_TX? 需要通过CAN驱动确认
  - 实际: CAN使用 `CAN_Mode_Init`，需要查 can.c 确定引脚
- JTAG禁用 → SWD模式 (PA13=SWDIO, PA14=SWCLK)
- USART2: 需要查usart2.c确定引脚（用于HMI串口屏）
- 软件I2C地址: BQ76940 = 0x08 (7位地址 << 1 = 0x10)

### 2. BQ76940 关键寄存器地址
| 寄存器 | 地址 | 描述 |
|--------|------|------|
| SYS_STAT | 0x00 | 系统状态 |
| CELLBAL1 | 0x01 | 电池均衡1 |
| CELLBAL2 | 0x02 | 电池均衡2 |
| CELLBAL3 | 0x03 | 电池均衡3 |
| SYS_CTRL1 | 0x04 | 系统控制1 |
| SYS_CTRL2 | 0x05 | 系统控制2(DSG/CHG FET) |
| PROTECT1 | 0x06 | 保护1(SCD) |
| PROTECT2 | 0x07 | 保护2(OCD) |
| PROTECT3 | 0x08 | 保护3 |
| OV_TRIP | 0x09 | 过压阈值 |
| UV_TRIP | 0x0A | 欠压阈值 |
| CC_CFG | 0x0B | 库仑计配置 |
| VC1_HI/LO | 0x0C/0x0D | 第1节电压 |
| VC2_HI/LO | 0x0E/0x0F | 第2节电压 |
| VC5_HI/LO | 0x14/0x15 | 第5节电压 |
| VC6_HI/LO | 0x16/0x17 | 第6节电压 |
| VC7_HI/LO | 0x18/0x19 | 第7节电压 |
| VC10_HI/LO | 0x1E/0x1F | 第10节电压 |
| VC11_HI/LO | 0x20/0x21 | 第11节电压 |
| VC12_HI/LO | 0x22/0x23 | 第12节电压 |
| VC15_HI/LO | 0x28/0x29 | 第15节电压 |
| BAT_HI/LO | 0x2A/0x2B | 总电压 |
| TS1_HI/LO | 0x2C/0x2D | 温度传感器1 |
| CC_HI/LO | 0x32/0x33 | 库仑计电流 |
| ADCGAIN1 | 0x50 | ADC增益1 |
| ADCOFFSET | 0x51 | ADC偏移 |
| ADCGAIN2 | 0x59 | ADC增益2 |

### 3. BQ76940 初始化配置序列
参考代码的初始化寄存器值：
```
SYS_STAT  = 0xFF   // 清除所有状态
CELLBAL1  = 0x00   // 关闭均衡
CELLBAL2  = 0x00
CELLBAL3  = 0x00
SYS_CTRL1 = 0x18   // 启用ADC，关闭SHIP模式
SYS_CTRL2 = 0x43   // DSG ON, CHG ON
PROTECT1  = 0x00
PROTECT2  = 0x00
PROTECT3  = 0x00
OV_TRIP   = 0x00   // 稍后由OV_UV_1_PROTECT()设置
UV_TRIP   = 0x00
CC_CFG    = 0x19   // 库仑计配置
```

### 4. 保护参数
| 保护类型 | 阈值 | 恢复值 | 说明 |
|---------|------|--------|------|
| 过压(OV) | 4400mV | 4300mV | BQ76940硬件保护 |
| 欠压(UV) | 2400mV | 2500mV | BQ76940硬件保护 |
| 软件OV | 4200mV | 4100mV | MCU关闭CHG FET |
| 软件UV | 2800mV | 2800mV | MCU关闭DSG FET |
| 过流(OC) | 2000mA | 2000mA | MCU关闭FET |
| 过温(OT) | Flash存储值 | Flash存储值 | MCU关闭FET |
| SCD | 66A/400us | - | BQ76940硬件保护 |
| OCD | 100A/1280ms | - | BQ76940硬件保护 |

### 5. 电压计算公式
```
ADC_offset = 从ADCOFFSET寄存器读取
ADC_GAIN = ((gain[0]&0x0c)<<1) + ((gain[1]&0xe0)>>5)
GAIN = 365 + ADC_GAIN  (单位: uV/LSB)
电压 = (ADC_raw * GAIN) / 1000 + ADC_offset  (单位: mV)
```

### 6. SOC 估算方法
简单的查表法（基于总电压）：
- >4100mV×9: 100%
- 4050-4100mV×9: 90%
- ... (以50mV为步进递减)
- 3100-3200mV×9: 5%
注意：SOC判断逻辑中有一个bug，第一条件 `>4100*9` 在第二个elseif中会被跳过（因为第二个条件是 `>4100*9 && <4150*9`，第一个条件 `>4100*9` 包含了第二个）。

### 7. 通信协议
- **USART1 (115200bps)**: 
  - 接收QT上位机指令: 帧头 `01 02 55` / `01 03 55` 等
  - 发送数据帧: `AA 01 ...`（BMS数据）
- **USART2 (9600bps)**: 
  - 发送HMI串口屏命令: DCV16()显示电压等
- **CAN (500Kbps)**:
  - 7帧CAN消息，ID 0x0001-0x0007
  - 每帧8字节

### 8. I2C CRC 协议
BQ76940使用带CRC的I2C通信：
- 写操作帧格式: `[设备地址(写)] [寄存器地址] [数据] [CRC8]`
- 读操作: 标准I2C读，无需CRC
- CRC8多项式: x^8 + x^2 + x + 1 (key=0x07)

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| 使用 HAL 库重写 BSP 层 | CubeMX 生成 HAL 代码，与StdPeriph库API不同但功能相同 |
| 软件 I2C 保持 GPIO 模拟 | BQ76940 CRC 协议需要灵活控制 I2C 时序 |
| 保持原有9节采集索引 | 硬件连接决定（VC1/2/5/6/7/10/11/12/15对应物理9串） |
| 所有引脚/控制逻辑与原项目一致 | 用户明确要求 |
| CAN使用PB8/PB9? 待can.c确认 | 需要确认CAN引脚映射 |
| CMakeLists 手动编写 | CubeMX生成的Makefile不够灵活 |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| 参考项目使用StdPeriph库，需迁移到HAL | 逐层重写BSP驱动，保持接口不变 |
| BQ76930.c文件名误导(实为BQ76940) | 新项目重命名为bq76940.c/h |

## Resources
- 参考项目路径: `D:\coding_codes\bms_learning\datasheet\BQ76940发货资料 20220817\3.程序\1.BQ76940（程序加CAN20210915）改温度探头\BMS_s940\`
- BQ76940 Datasheet: `D:\coding_codes\bms_learning\datasheet\`
- CubeMX 安装路径: (需确认)
- ARM GCC Toolchain: (需确认)

## Visual/Browser Findings
- 参考项目是 Keil MDK-ARM 工程 (BMS_DEMO.uvprojx)
- 使用 startup_stm32f10x_md.s (中等密度启动文件)
- 编译产物包含 .axf (ELF) 和 .hex 文件
- 链接配置文件在 config/ 目录下 (.icf 文件)
