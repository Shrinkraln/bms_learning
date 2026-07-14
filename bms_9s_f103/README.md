# BMS 9S F103 — 电池管理系统

> STM32F103C8T6 + BQ76940 | 9串锂电池 | CubeMX + CMake + VSCode

## 项目概述

基于 TI BQ76940 电池前端 AFE 芯片，使用 STM32F103C8T6 作为主控的 9串电池管理系统（BMS）固件。

### 功能特性

- 🔋 **9串电压采集** — 通过 I2C 读取 BQ76940 14位 ADC
- 🌡️ **温度监控** — NTC 热敏电阻温度采样
- ⚡ **电流检测** — 库仑计数器电流测量
- 📊 **SOC 估算** — 基于电压的 SOC 查表
- 🛡️ **多层保护** — 过压/欠压/过流/短路/过温（硬件 + 软件）
- 📡 **多通道通信** — USART1(QT上位机) + USART2(HMI串口屏) + CAN
- 💾 **参数存储** — Flash 模拟 EEPROM

### 硬件

| 功能 | 芯片/模块 |
|------|----------|
| 主控 | STM32F103C8T6 (64KB Flash, 20KB RAM) |
| AFE | BQ76940 (9-15串电池前端) |
| 通信 | CAN 收发器 + USB-TTL |
| 调试 | SWD (ST-Link / J-Link) |

## 快速开始

### 前提条件

- ARM GCC Toolchain (`arm-none-eabi-gcc`)
- CMake ≥ 3.16
- OpenOCD (烧录/调试)
- STM32CubeF1 固件包

### 步骤

```bash
# 1. 下载 CMSIS/HAL 库
bash scripts/setup_cmsis_hal.sh

# 2. 配置并构建
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# 3. 烧录
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program build/BMS_9S_F103.elf verify reset exit"
```

### VSCode 开发

1. 安装推荐插件（打开项目时会提示）
2. `Ctrl+Shift+B` → 构建
3. `F5` → 调试（Cortex-Debug）

## 项目结构

```
bms_9s_f103/
├── DEVELOPMENT_GUIDE.md   ← 详细开发指导文档（必读！）
├── CMakeLists.txt          ← CMake 构建脚本
├── STM32F103C8Tx_FLASH.ld ← GCC 链接脚本
├── Core/                   ← HAL 配置 + 中断 + main
├── BSP/                    ← 板级驱动 (I2C, CAN, UART, GPIO...)
├── App/                    ← 应用层 (保护, 上报, 调度)
├── Drivers/                ← CMSIS + HAL (需下载)
├── scripts/                ← 辅助脚本
└── .vscode/                ← VSCode 配置
```

## 文档

- **[DEVELOPMENT_GUIDE.md](./DEVELOPMENT_GUIDE.md)** — 完整的开发指导框架，包含：
  - CubeMX 引脚/外设/时钟配置步骤
  - CMake 工程搭建
  - 代码架构与分层设计
  - 每个 BSP 驱动的编写流程
  - BQ76940 驱动详细实现指南
  - 应用层编写模板
  - StdPeriph → HAL 转换对照表
  - 编译/烧录/验证清单

## 参考项目

基于 `datasheet/BQ76940发货资料 20220817/3.程序/1.BQ76940（程序加CAN20210915）改温度探头/BMS_s940/`

引脚配置和控制逻辑与原项目保持一致。

## 许可证

待定
