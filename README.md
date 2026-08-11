# BMS Learning — 电池管理系统全栈项目

> STM32F103 + FreeRTOS + BQ76940 固件 × Qt6/QML 上位机 | 9 串锂电池管理

## 项目概述

本项目是一个完整的 **9 串锂电池管理系统 (BMS)**，包含 **嵌入式固件** 和 **桌面端上位机** 两部分，通过 CAN 总线进行实时数据通信。

### 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    BmsHostApp (Qt6/QML)                     │
│                  Windows 桌面端上位机                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────┐  │
│  │ Overview │  │  Cells   │  │  Trends  │  │ Device Status │  │
│  │  总览页   │  │ 电芯页   │  │ 趋势页   │  │  设备状态页   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────┘  │
│                        │ CAN (PCAN-USB)                     │
├────────────────────────┼────────────────────────────────────┤
│               CAN 500kbps                                   │
├────────────────────────┼────────────────────────────────────┤
│            bms_9s_f103 (STM32F103C8T6)                      │
│              嵌入式 BMS 固件                                 │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  App 层: 保护 · SOC/OCV · CAN指令 · 共享数据 · 任务编排 │   │
│  │  BSP 层: BQ76940 · I2C · CAN · Timer · WDG · LED · IO │   │
│  │  Core 层: HAL · FreeRTOS · CubeMX · 中断              │   │
│  └──────────────────────────────────────────────────────┘   │
│                        │ I2C                                │
│                   ┌────┴────┐                               │
│                   │ BQ76940  │  9 串电芯                     │
│                   │   AFE    │  温度/电流                    │
│                   └─────────┘                               │
└─────────────────────────────────────────────────────────────┘
```

## 技术栈

### 嵌入式固件 (`bms_9s_f103/`)

| 层级 | 技术 | 说明 |
|------|------|------|
| **MCU** | STM32F103C8T6 | Cortex-M3, 72MHz, 64KB Flash, 20KB RAM |
| **AFE** | BQ76940 (TI) | 9–15 串电池前端, I2C + CRC8 |
| **RTOS** | FreeRTOS (CMSIS-RTOS V2) | 6 任务事件驱动架构 |
| **HAL** | STM32CubeF1 | 外设驱动层 |
| **工具链** | CubeMX + CMake + arm-none-eabi-gcc | 构建与配置 |
| **调试** | OpenOCD / J-Link / SWD | 烧录与在线调试 |
| **通信** | CAN 500kbps (唯一对外通道) | 5 帧上行 + 3 帧下行协议 |

### 桌面端上位机 (`BmsHostApp/`)

| 层级 | 技术 | 说明 |
|------|------|------|
| **框架** | Qt 6 + QML | 声明式 UI |
| **架构** | Hybrid MVVM | Model ↔ ViewModel ↔ QML View |
| **CAN 驱动** | PCAN-Basic API | 通过 PCAN-USB 适配器 |
| **解码** | DBC 辅助 + 手动协议解析 | 多帧合并 + 容错 |
| **数据存储** | CSV 日志 | 电压/温度/故障持续记录 |
| **构建** | CMake + MSVC | Windows 桌面应用 |

## 目录结构

```
bms_learning/
├── README.md                    ← 本文件 — 项目总览
├── DEVELOPMENT_LOG.md           ← 开发日志 — 全时间线记录
├── .gitignore
│
├── bms_9s_f103/                 ← 嵌入式 BMS 固件
│   ├── README.md                ← 固件项目说明 + 快速开始
│   ├── DEVELOPMENT_GUIDE.md     ← 完整开发流程指南 (必读)
│   ├── docs/
│   │   └── ARCHITECTURE.md      ← 固件技术架构文档
│   ├── bms_9s_f103/             ← 固件源码
│   │   ├── Core/                ← HAL 骨架 + FreeRTOS + 中断
│   │   ├── BSP/                 ← 板级驱动 (10 模块)
│   │   ├── App/                 ← 应用层 (5 模块, 零 BSP 依赖)
│   │   ├── Drivers/             ← CMSIS + HAL 库
│   │   ├── Middlewares/         ← FreeRTOS 内核
│   │   ├── build/               ← 构建产物
│   │   └── .spec-dev/           ← 设计 Spec + ADR 记录
│   ├── datasheet/               ← 芯片技能 (BQ76940 / STM32F103)
│   └── csv/                     ← CAN 日志 CSV 数据
│
├── BmsHostApp/                  ← Qt 桌面端上位机
│   ├── README.md                ← 上位机项目说明
│   ├── docs/
│   │   └── ARCHITECTURE.md      ← 上位机技术架构文档
│   ├── model/                   ← 数据模型层
│   ├── protocol/                ← CAN 协议解码 + CSV 日志
│   ├── can/                     ← PCAN 工作线程
│   ├── qml/                     ← QML 界面 (4 页面 + 组件)
│   └── build/                   ← 构建产物
│
├── datasheet/                   ← 参考数据手册与视频
└── docs/                        ← 项目级文档
```

## 快速开始

### 固件编译

```bash
cd bms_9s_f103/bms_9s_f103
cmake -B build/Debug -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug -j
```

### 固件烧录

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/Debug/bms_9s_f103.elf verify reset exit"
```

### 上位机编译

```bash
cd BmsHostApp
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## CAN 通信协议

### 上行帧 (MCU → 上位机, 周期性)

| CAN ID | 名称 | 内容 | 周期 |
|--------|------|------|------|
| 0x100 | BMS_FAULT | 故障状态 (紧急, 头插优先) | 事件驱动 |
| 0x110 | CELL_VOLT_1_4 | 电芯电压 C1–C4 | 100ms |
| 0x111 | CELL_VOLT_5_9 | 电芯电压 C5–C9 | 100ms |
| 0x120 | BMS_STATUS | 总压/电流/SOC/FET/故障/保护 | 100ms |
| 0x130 | SOC_OCV | SOC/OCV 详细数据 | 1000ms |

### 下行帧 (上位机 → MCU, 事件驱动)

| CAN ID | 名称 | 用途 |
|--------|------|------|
| 0x200 | QUERY | 查询命令, MCU 立即回传 |
| 0x201 | CONTROL | FET 控制 / 均衡 / 清除故障 |
| 0x202 | CONFIG | 运行时修改保护阈值 |

## 文档索引

| 文档 | 位置 | 内容 |
|------|------|------|
| 开发日志 | [DEVELOPMENT_LOG.md](./DEVELOPMENT_LOG.md) | 完整时间线开发记录 |
| 固件架构 | [bms_9s_f103/docs/ARCHITECTURE.md](./bms_9s_f103/docs/ARCHITECTURE.md) | 4 层架构 + 模块设计 |
| 固件开发指南 | [bms_9s_f103/DEVELOPMENT_GUIDE.md](./bms_9s_f103/DEVELOPMENT_GUIDE.md) | 从零搭建全流程 |
| 上位机架构 | [BmsHostApp/docs/ARCHITECTURE.md](./BmsHostApp/docs/ARCHITECTURE.md) | MVVM + 协议解码 |
| ADR 决策记录 | `bms_9s_f103/bms_9s_f103/.spec-dev/adr/` | 8 个架构决策 |

## 参考项目

基于 TI BQ76940 参考设计: `datasheet/BQ76940发货资料 20220817/`

## 许可证

待定
