# BMS 9S F103 — 电池管理系统固件

> STM32F103C8T6 + FreeRTOS + BQ76940 | 9串锂电池 | CubeMX + CMake + VSCode

## 项目概述

基于 TI BQ76940 电池前端 AFE 芯片，使用 STM32F103C8T6 作为主控的 9 串电池管理系统（BMS）固件。采用 **4 层严格分层架构** (Hardware → Core → BSP → App) 和 **6 任务事件驱动 FreeRTOS 设计**。

### 功能特性

- 🔋 **9串电压采集** — I2C 读取 BQ76940 14-bit ADC
- 🌡️ **温度监控** — NTC 热敏电阻温度采样 (3 路)
- ⚡ **电流检测** — 库仑计数器电流测量
- 📊 **SOC 估算** — 增强型安时积分 + 31 点 OCV 查表 (真实电芯数据)
- 🛡️ **12 种故障检测** — 3 级保护 (WARNING → ALERT → FAULT)
- 📡 **CAN 通信** — 5 帧上行 + 3 帧下行协议 (500kbps)
- 🔄 **FreeRTOS** — 6 任务事件驱动, 故障响应 < 10μs
- 💾 **参数存储** — Flash 模拟 EEPROM

### 硬件

| 功能 | 芯片/模块 | 关键参数 |
|------|----------|---------|
| 主控 | STM32F103C8T6 | Cortex-M3, 72MHz, 64KB Flash, 20KB RAM |
| AFE | BQ76940 (TI) | 9–15串, I2C 100kHz, 地址 0x08 |
| 通信 | CAN 收发器 | PA11(RX)/PA12(TX), 500kbps |
| 调试 | SWD | PA13(SWDIO)/PA14(SWCLK) |
| 时钟 | HSE 8MHz + PLL×9 | SYSCLK 72MHz, APB1 36MHz, APB2 72MHz |

## 项目结构

```
bms_9s_f103/
├── README.md                    ← 本文件
├── DEVELOPMENT_GUIDE.md         ← 完整开发流程指南 (从零搭建)
├── docs/
│   └── ARCHITECTURE.md          ← 技术架构文档 (4层 + 6任务)
├── bms_9s_f103/                 ← 固件源码
│   ├── bms_9s_f103.ioc          ← CubeMX 硬件配置入口
│   ├── CMakeLists.txt           ← CMake 构建 (分层 OBJECT 库)
│   ├── STM32F103XX_FLASH.ld     ← GCC 链接脚本 (.noinit 段)
│   ├── Core/                    ← HAL 骨架 + FreeRTOS + 中断
│   │   ├── Inc/                 ← main.h, FreeRTOSConfig.h, stm32f1xx_hal_conf.h
│   │   └── Src/                 ← main.c, freertos.c, stm32f1xx_it.c, stm32f1xx_hal_msp.c
│   ├── BSP/                     ← 板级驱动 (9 模块 + ring_buf + bsp_common)
│   │   ├── Inc/                 ← bq76940.h, i2c_sw.h, can_drv.h, timer.h, ...
│   │   └── Src/                 ← bq76940.c, i2c_sw.c, can_drv.c, ...
│   ├── App/                     ← 应用层 (5 模块, 零 BSP 依赖)
│   │   ├── Inc/                 ← protection.h, soc_ocv.h, can_cmd.h, ...
│   │   └── Src/                 ← protection.c, soc_ocv.c, can_cmd.c, bms_app.c, ...
│   ├── Drivers/                 ← CMSIS + STM32F1xx HAL 库
│   ├── Middlewares/             ← FreeRTOS 内核 (CMSIS-RTOS V2)
│   ├── build/Debug/             ← 构建产物 + .elf/.hex
│   ├── cmake/                   ← CMake 工具链模块 (stm32cubemx)
│   └── .spec-dev/               ← 设计 Spec + ADR 记录
├── datasheet/                   ← BQ76940 / STM32F103 数据手册技能
└── csv/                         ← CAN 日志 CSV 数据
```

## 快速开始

### 前提条件

- ARM GCC Toolchain (`arm-none-eabi-gcc`)
- CMake ≥ 3.16
- OpenOCD (烧录/调试) 或 J-Link
- STM32CubeMX ≥ 6.0 (硬件配置修改)

### 编译

```bash
cd bms_9s_f103
cmake -B build/Debug -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug -j
```

### 烧录 (ST-Link)

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/Debug/bms_9s_f103.elf verify reset exit"
```

### 烧录 (J-Link)

```bash
openocd -f interface/jlink.cfg -f target/stm32f1x.cfg \
  -c "program build/Debug/bms_9s_f103.elf verify reset exit"
```

### VSCode 开发

1. 安装推荐插件: C/C++, CMake Tools, Cortex-Debug, ARM
2. `Ctrl+Shift+B` → 构建
3. `F5` → 调试 (Cortex-Debug + OpenOCD)

## 文档索引

| 文档 | 内容 |
|------|------|
| [ARCHITECTURE.md](./docs/ARCHITECTURE.md) | **技术架构** — 4 层分层 + 模块设计 + 数据流 + 内存预算 |
| [DEVELOPMENT_GUIDE.md](./DEVELOPMENT_GUIDE.md) | **开发指南** — 18 步从零搭建全流程 + FreeRTOS 集成 |
| [../DEVELOPMENT_LOG.md](../DEVELOPMENT_LOG.md) | **开发日志** — 全时间线记录 (根目录) |
| `.spec-dev/adr/` | **ADR** — 8 个架构决策记录 |
| `.spec-dev/*/spec/` | **设计 Spec** — 13 个功能设计文档 |

## 当前状态

| 指标 | 数值 |
|------|------|
| FLASH | 56.2 KB / 64 KB (87.8%) |
| RAM | 19.6 KB / 20 KB (95.8%) |
| 编译 | 0 错误 0 警告 |
| 系统稳定性 | ✅ 无 IWDG 复位 |
| 电压采集 | ✅ 9 路全部正常 |
| CAN 通信 | ✅ 上位机 CSV 正常 |
| 温度 | ⚠️ 读数 = 0 (待排查) |
| FET/CC | ⚠️ DEVICE_XREADY 硬件锁存 (待硬件排查) |

## CubeMX 修改流程

1. 在 CubeMX 中打开 `bms_9s_f103/bms_9s_f103.ioc`
2. 修改引脚/外设/时钟/FreeRTOS 配置
3. Regenerate Code → 仅覆盖 Core 层非 USER CODE 区域
4. 手动合并 USER CODE 区域冲突
5. 重新编译验证

## 参考项目

基于 TI BQ76940 参考设计: `../datasheet/BQ76940发货资料 20220817/3.程序/1.BQ76940（程序加CAN20210915）改温度探头/BMS_s940/`

## 许可证

待定
