# BmsHostApp — BMS 桌面端上位机

> Qt 6 + QML | Hybrid MVVM | PCAN-USB | CAN 协议解码

## 项目概述

基于 Qt 6 的 Windows 桌面端 BMS 上位机，通过 PCAN-USB 适配器与 BMS 固件通信，实时显示电芯电压、温度、SOC、故障状态等数据，支持 CSV 日志记录。

## 功能特性

- 🔋 **电芯监控** — 9 路电芯电压实时柱状图, 锚点布局无闪烁
- 🌡️ **温度显示** — 多路温度传感器 + 趋势图
- 📊 **SOC/OCV** — 荷电状态 + 开路电压可视化
- ⚠️ **故障告警** — 保护状态实时显示
- 📡 **CAN 通信** — PCAN-USB 500kbps 收发
- 💾 **CSV 日志** — 电压/温度/故障持续记录
- 🎛️ **设备控制** — FET 控制 / 清除故障 / 参数配置

## 快速开始

### 前提条件

- Windows 10/11
- Qt 6.x + MSVC 2022
- CMake ≥ 3.16
- PCAN-Basic API (PEAK-System)
- PCAN-USB 硬件适配器

### 编译

```bash
cd BmsHostApp
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### 运行

```bash
./build/Release/BmsHostApp.exe
```

## 项目结构

```
BmsHostApp/
├── README.md
├── docs/
│   └── ARCHITECTURE.md       ← 技术架构文档
├── CMakeLists.txt            ← CMake 构建脚本
├── main.cpp                  ← 应用入口
├── can/                      ← PCAN 驱动层
│   ├── CanWorker.h/cpp       ← 后台收发线程
│   └── CanFrame.h            ← CAN 帧数据结构
├── protocol/                 ← 协议解码层
│   ├── BmsProtocolDecoder.h/cpp ← 多帧合并 + 解码
│   ├── BmsDbcHelper.h/cpp    ← DBC 辅助
│   ├── BmsSnapshot.h         ← 数据快照
│   └── CsvLogger.h/cpp       ← CSV 日志
├── model/                    ← 数据模型层
│   ├── BmsDataModel.h/cpp    ← 核心数据模型
│   ├── CellVoltageModel.h/cpp ← 电芯电压模型
│   ├── TemperatureModel.h/cpp ← 温度模型
│   ├── FaultListModel.h/cpp  ← 故障列表模型
│   └── CommunicationMonitor.h/cpp ← 通信监控
└── qml/                      ← QML 界面层
    ├── main.qml              ← 主窗口
    ├── OverviewPage.qml      ← 总览页
    ├── CellsPage.qml         ← 电芯详情页
    ├── TrendsPage.qml        ← 趋势图页
    ├── DeviceStatusPage.qml  ← 设备状态页
    ├── ControlPanel.qml      ← 控制面板
    ├── ConfigDialog.qml      ← 配置对话框
    └── components/
        └── CellBar.qml       ← 电芯柱状图组件
```

## 技术架构

详见 [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)

## 依赖

- Qt 6 (Core, Qml, Quick, Widgets)
- PCAN-Basic API (PEAK-System)
- MSVC 2022 (C++17)

## 许可证

待定
