# BmsHostApp 技术架构

> Qt 6 + QML | Hybrid MVVM | PCAN-Basic | 多帧批处理合并

## 架构概览

```
┌──────────────────────────────────────────────────────┐
│                   QML View Layer                      │
│  main.qml → OverviewPage | CellsPage | TrendsPage    │
│             DeviceStatusPage | ControlPanel          │
│             ConfigDialog | CellBar (组件)             │
├──────────────────────────────────────────────────────┤
│                C++ Model / ViewModel                  │
│  BmsDataModel  ·  CellVoltageModel  ·  TemperatureModel │
│  FaultListModel  ·  CommunicationMonitor             │
├──────────────────────────────────────────────────────┤
│                  Protocol Layer                       │
│  BmsProtocolDecoder  ·  BmsDbcHelper  ·  BmsSnapshot │
│  CsvLogger                                           │
├──────────────────────────────────────────────────────┤
│                    CAN Layer                          │
│  CanWorker (PCAN-Basic API)  ·  CanFrame             │
├──────────────────────────────────────────────────────┤
│               PCAN-USB Hardware                       │
│               CAN 500kbps                            │
└──────────────────────────────────────────────────────┘
```

## 分层设计

### Layer 1: CAN 驱动层 (`can/`)

**职责**: 封装 PCAN-Basic API, 提供后台 CAN 收发。

#### CanWorker
- 继承 `QThread`, 独立线程运行
- 使用 PCAN-Basic API 读写 PCAN-USB 设备
- 接收: 阻塞等待 → 封装为 `CanFrame` → 通过信号发送到主线程
- 发送: 通过槽接收 → PCAN-Basic 发送
- 支持设备热插拔检测

#### CanFrame
```cpp
struct CanFrame {
    uint32_t id;       // CAN ID (11-bit 标准帧)
    uint8_t  dlc;      // 数据长度 (0-8)
    uint8_t  data[8];  // 数据字节
    uint64_t timestamp; // 接收时间戳 (μs)
};
```

### Layer 2: 协议解码层 (`protocol/`)

**职责**: 将原始 CAN 帧解码为结构化 BMS 数据。

#### BmsProtocolDecoder

核心设计: **多帧批处理合并** — 解决 PCAN-USB 的 chunk 切分问题。

```
PCAN-USB 数据流:
  固件 CAN 发送: [0x110][0x111][0x120][0x130][0x121]  ← 连续 5 帧
  USB chunk 1:    [0x110]                               ← 非确定性边界
  USB chunk 2:    [0x111, 0x120, 0x130, 0x121]         ← 有时切分

上位机处理:
  CanWorker 每收到一个 chunk → emit framesReady(QVector<CanFrame>)
  BmsDataModel::onBatchReady(batch)
    → m_pendingBatch.append(batch)  ← 累积追加 (非替换!)
    → 30ms 定时器触发
    → decodeFrames(m_pendingBatch)  ← 合并后解码
    → m_pendingBatch.clear()
```

帧到解码器映射:
| CAN ID | 解码方法 | 数据 |
|--------|---------|------|
| 0x110 | `decodeCellVoltage1_4()` | C1-C4 电压 |
| 0x111 | `decodeCellVoltage5_9()` | C5-C9 电压 |
| 0x120 | `decodeBmsStatus()` | 总压/电流/SOC/FET/保护 |
| 0x130 | `decodeSocOcv()` | SOC/OCV 详细 |
| 0x100 | `decodeFault()` | 故障状态 |

容错机制:
- 帧缺失检测: 统计 batch 中每种 ID 的数量, 前 5 次 + 每 50 次告警
- 哨兵保护: `CellVoltageModel` 中 `mV==0 && old!=0` → 保留旧值

#### BmsSnapshot
```cpp
struct BmsSnapshot {
    int16_t cell_mv[9];      // 9 路电芯电压 (mV)
    int32_t total_mv;        // 总压 (mV)
    int16_t current_ma;      // 电流 (mA)
    uint16_t soc_permil;     // SOC (‰)
    uint16_t ocv_mv;         // OCV (mV)
    uint8_t fet_status;      // DSG/CHG FET 状态
    uint16_t fault_bits;     // 故障位
    int16_t temps[3];        // 3 路温度 (×0.1°C)
    uint8_t afe_online;      // AFE 在线标志
};
```

#### CsvLogger
- 将每次 `BmsSnapshot` 追加写入 CSV 文件
- 列: timestamp, C1-C9(mV), total_mV, current_mA, SOC, temps, faults
- 文件按日期命名: `BMS_Log_YYYYMMDD.csv`

### Layer 3: 数据模型层 (`model/`)

**职责**: 管理 UI 绑定的数据, 提供 Q_PROPERTY 接口供 QML 使用。

#### BmsDataModel (核心)
- 继承 `QObject`, 暴露给 QML
- `onBatchReady(QVector<CanFrame>)`: 累积追加 → 30ms 定时器 → 批量解码
- `applySnapshot(BmsSnapshot)`: 将快照分发到各子模型
- 信号: `dataUpdated()` → 触发 QML 刷新

#### CellVoltageModel
- `Q_PROPERTY(QVariantList cells ...)` — 9 路电芯数据
- 每路电芯: `{index, voltage_mV, hasData}`
- 哨兵保护: 新值=0 且旧值有效 → 保留旧值 (防止零覆盖)

#### TemperatureModel
- 管理 3 路温度传感器数据

#### FaultListModel
- 继承 `QAbstractListModel`
- 故障位解码为可读文本列表 (OV/UV/OC/OT/SCD/OCD/COMM_LOSS...)

#### CommunicationMonitor
- CAN 帧接收速率统计
- 通信超时检测 (连续无帧 → 离线告警)
- PCAN-USB 设备状态监控

### Layer 4: QML View Layer (`qml/`)

**职责**: 声明式 UI, 数据绑定, 用户交互。

#### 页面结构

```
main.qml (ApplicationWindow)
├── OverviewPage.qml       ← 总览: SOC 圆环 + 总压/电流 + 最高/最低电芯
├── CellsPage.qml           ← 电芯详情: 9 个 CellBar 柱状图
├── TrendsPage.qml          ← 趋势图: 电压/电流/温度时间序列
├── DeviceStatusPage.qml    ← 设备状态: FET/保护/故障标志
├── ControlPanel.qml        ← 控制: FET 开关/清除故障/参数配置
└── ConfigDialog.qml        ← CAN 接口 + 波特率配置
```

#### CellBar 组件 (关键修复)

锚点定位布局 (消除跳动闪烁):
```qml
Item {
    // 电芯编号 — 固定在底部 (下方对齐)
    Text {
        id: cellLabel
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
    }

    // 柱状条 — 底部固定, 向上增长
    Rectangle {
        id: bar
        anchors.bottom: cellLabel.top
        anchors.bottomMargin: 4
        // 仅过渡高度, 不动画 y
        Behavior on height {
            SmoothedAnimation { duration: 150; velocity: 80 }
        }
    }

    // 状态标志 — 固定在顶部
    Text {
        id: statusFlag
        anchors.top: parent.top
    }
}
```

#### 数据绑定模式

QML 属性绑定到 C++ Model 的 Q_PROPERTY:
```
CellVoltageModel.cells (QVariantList)
  → CellsPage.qml: Repeater { model: cellModel.cells }
    → CellBar.qml: voltageText = model.voltage_mV
```

## 数据流

```
PCAN-USB Hardware
  │ (CAN frames, 500kbps)
  ▼
CanWorker (background thread)
  │ signals: framesReady(QVector<CanFrame>)
  ▼
BmsDataModel::onBatchReady()
  │ m_pendingBatch.append(batch)           ← 累积追加
  │ 30ms QTimer → decodeFrames()           ← 合并解码
  ▼
BmsProtocolDecoder
  │ 帧分发: 0x110→cellVoltage, 0x120→status...
  ▼
BmsSnapshot
  │
  ├─► CellVoltageModel  ─► CellsPage.qml
  ├─► TemperatureModel  ─► OverviewPage / TrendsPage
  ├─► FaultListModel    ─► DeviceStatusPage
  └─► CsvLogger         ─► disk (BMS_Log_*.csv)
```

## 关键技术决策

### 多帧批处理合并 (vs 逐帧处理)

**问题**: PCAN-USB 将固件的连续 5 帧 CAN 消息切分为非确定性 USB chunks, 单 chunk 可能不包含完整的 9 路电压 (0x110 + 0x111 + 0x120)。

**方案**: 30ms 累积窗口 + append 合并:
```cpp
void BmsDataModel::onBatchReady(const QVector<CanFrame> &batch) {
    m_pendingBatch.append(batch);  // 累积 (非替换!)
    if (m_pendingBatch.size() > 50) {
        m_pendingBatch = m_pendingBatch.last(50);  // 上限保护
    }
    // 30ms 定时器到期 → decodeFrames(m_pendingBatch) → clear()
}
```

### CellBar 锚点布局 (vs Column 布局)

**问题**: Column 布局下电压值变化导致柱状条高度变化 → y 位置随之变化 → 视觉跳动闪烁。

**方案**: 锚点定位:
- 柱状条 `anchors.bottom` 固定到电芯编号顶部 → 底部固定
- 仅动画过渡高度 (`SmoothedAnimation`) → 无 y 动画 → 无跳动

### 哨兵保护 (vs 严格覆盖)

**问题**: 即使合并后, 某帧仍可能偶发缺失 → 有效电压被 0 覆盖 → 显示 "---"。

**方案**: CellVoltageModel 更新时检查:
```cpp
void CellVoltageModel::updateCell(int index, int16_t mV) {
    auto &c = m_cells[index];
    if (mV == 0 && c.mV != 0) return;  // 新值无效, 保留旧值
    c.mV = mV;
    c.hasData = (mV > 0);
    emit dataChanged(...);
}
```

## 已知问题

| 问题 | 状态 | 备注 |
|------|------|------|
| OverviewPage 电芯摘要硬编码 "C?" | 低优先级 | `OverviewPage.qml:87-89` |
| 0x110 帧偶发缺失 | 已防御 | 三层修复 (主 + 哨兵 + 固件可选) |

## 编译与部署

### 编译

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### 运行依赖

- `BmsHostApp.exe`
- PCAN-Basic DLL (PEAK-System 驱动安装时自动注册)
- Qt 6 运行时 DLL
- MSVC 2022 运行时

### 配置文件

- CAN 接口选择: 运行时通过 ConfigDialog 配置
- 日志路径: 可执行文件同目录 `logs/`
