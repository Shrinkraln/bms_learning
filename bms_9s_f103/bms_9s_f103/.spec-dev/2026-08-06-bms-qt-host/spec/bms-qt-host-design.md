---
# —— spec-dev 漂移守卫锚点（机器可校验，勿删）——
spec_dev:
  version: 1
  feature: bms-qt-host
  status: draft
  covers: []
  sync_commit: null
---
# BMS 9S QT 上位机 — 设计 spec

## 背景与目标

为 STM32F103 + BQ76940 9 串 BMS 固件创建配套的 QT 桌面端上位机应用程序。上位机通过 PCAN-USB 适配器连接 BMS 的 CAN 总线（500kbps），接收周期性遥测数据并可视化显示，同时支持发送控制指令、参数配置和数据查询。

**成功标准**:
- 实时接收并解码全部 7 种上行 CAN 帧（0x100/0x101/0x110/0x111/0x120/0x121/0x130），刷新率 100ms
- 3 页面可视化：总览（仪表盘+FET+温度）、电芯（电压柱状图+温度条）、曲线（滚动趋势图）
- 全部 8 种下行 CAN 指令可发送（查询/控制/配置），含安全条件校验
- CSV 数据日志（含温度、FET 状态），DBC 文件可选加载
- 信号超时检测（>500ms 无帧 → "已断开"状态）
- 编译 0 错误 0 警告，Qt 6.5+ / CMake / MSVC 或 MinGW

## 非目标

- 移动端（Android/iOS）部署
- 多 BMS 设备同时连接（仅支持单设备）
- CAN 总线回放/模拟器
- 远程 Web 访问
- 固件 OTA 升级
- BMS 阈值回读显示（固件 CONFIG 为单向写入，无读回通道。上位机对话框初始值为空，用户填入后发送）

## 术语表

| 术语 | 定义 | Avoid |
|------|------|-------|
| Snapshot | 一次完整 BMS 数据采样，包含电芯电压/温度/电流/SOC/保护状态/FET/均衡 | frame batch、data packet |
| permil (‰) | SOC 内部精度单位，0-1000 对应 0%-100% | per mille |
| FET | 场效应管，BMS 通过 CHG FET / DSG FET 控制充/放电回路开关 | MOSFET、开关管 |
| 上行帧 | BMS MCU → 上位机的 CAN 帧（遥测数据）| uplink、telemetry |
| 下行帧 | 上位机 → BMS MCU 的 CAN 帧（指令）| downlink、command |
| 故障恢复帧 | level=0、faults=0 的 0x101 帧，表示所有故障已清除 | 故障清除帧、clear frame |

## 影响面

本需求为**新建独立项目**。为支持上位机完整功能，**BMS 固件需小幅修改 CAN 协议**（约 30 行，详见附录 C）：

| 模块 | 影响 | 说明 |
|------|------|------|
| BMS 固件 can_cmd.c | 修改 | can_pub() 新增 0x121 温度帧；0x120 data[5] 编码 FET+均衡状态位 |
| BMS 固件 bms_app.c | 修改 | 0x101 故障帧 layout 改为 uint16 电压编码 + 发送恢复帧 |
| BMS 固件 task_plan.md | 修改 | CAN 协议节同步更新 |
| can/ | 新建 | CAN Worker 线程 + PCAN 驱动封装 |
| protocol/ | 新建 | 协议解码/编码 + CSV 日志 + DBC 解析 |
| model/ | 新建 | 中央数据模型 + 3 个子 ListModel |
| qml/ | 新建 | 3 页面 + 控制面板 + 配置对话框 + 自定义组件 |

---

## 架构与组件设计

### 总体架构

```
CAN Worker Thread (独立 QThread)
  QCanBusDevice("peakcan", "usb0", 500kbps)
  → framesReceived → readAllFrames()
  → emit batchReady(QVector<CanFrame>)
    ↓ Qt::QueuedConnection
Main Thread:
  BmsProtocolDecoder::decodeFrames(batch)
  → BmsSnapshot 结构体 (含温度、FET、均衡标志)
  → BmsDataModel::applySnapshot(snapshot)
    → Q_PROPERTY NOTIFY (仅变更时发射)
    → CellVoltageModel::dataChanged (仅变更 cell)
    → TemperatureModel::dataChanged (仅变更 sensor)
    → CsvLogger::appendRow (条件写入)
  → QML Views (属性绑定自动刷新)
```

**批处理语义**：多个 `batchReady` 批次到达时，取最后一个（latest-wins），丢弃中间批次——避免积压。

### 组件职责

**CanWorker (can/CanWorker.h/cpp)**:
- 拥有 `QCanBusDevice` 实例，全部 CAN 操作在此线程
- `start(plugin, interface, bitrate)` → 打开设备，连接 `framesReceived` 信号
- `stop()` → 断开设备
- `sendFrame(CanFrame)` slot → `writeFrame()`
- 信号: `batchReady(QVector<CanFrame>)`, `connectionStatusChanged(bool)`, `errorOccurred(QString)`
- 500ms 定时器检测 `lastFrameTimestamp`，超时 emit `connectionStatusChanged(false)`

**BmsProtocolDecoder (protocol/BmsProtocolDecoder.h/cpp)**:
- 纯函数风格，零 QObject 依赖（可单元测试）
- `decodeFrames(batch) → BmsSnapshot`: 按 CAN ID 分发解码，**每帧先检查 DLC ≥ 预期最小长度（见附录 A），DLC 不足则跳过并日志计数**
- `encodeCommand(BmsCommand) → CanFrame`: 下行指令编码
- 内置手写解码器（主路径）；DBC 解码器可选（`BmsDbcHelper`）
- 解码规则与 BMS 固件完全一致（见附录 A，含固件修改后的新协议）

**BmsDbcHelper (protocol/BmsDbcHelper.h/cpp)**:
- `loadDbc(path) → bool`: 加载 DBC 文件
- `decodeFrame(frame) → QMap<QString, QVariant>`: DBC 解码单帧
- 使用 `QCanDbcFileParser` + `QCanFrameProcessor`（Qt 6.5+ 实验性 API）
- 加载失败回退到内置解码器

**CsvLogger (protocol/CsvLogger.h/cpp)**:
- `start(filePath, intervalMs)`: 打开 CSV 文件，写表头
- `appendRow(snapshot)`: **每收到一次 snapshot 决定是否写入**——interval 模式: 距上次写入 ≥ intervalMs 时写入；故障触发: activeFaults 变化时立即写入（不受间隔限制）→ 写入后重置 interval 计时器
- `stop()`: flush + close
- 列: timestamp(ISO 8601 UTC), pack_voltage_mv, pack_current_ma, soc_permil, prot_level, active_faults, cell_1_mv..cell_9_mv, ts1_0p1c, ts2_0p1c, ts3_0p1c, fet_chg, fet_dsg, balancing

**BmsDataModel (model/BmsDataModel.h/cpp)**:
- 中央数据模型，QObject 单例，注册到 QML context
- Q_PROPERTY 标量（13 个，全部 NOTIFY）:
  - `packVoltage` (quint32, mV), `packCurrent` (qint32, mA)
  - `socPermil` (quint16, ‰), `realSocPermil` (quint16, ‰)
  - `remainingMah` (qint32), `qMaxMah` (qint32)
  - `protLevel` (quint8), `activeFaults` (quint16)
  - `cellMin`/`cellMax`/`cellDiff` (quint16, mV)
  - `fetCharge` (bool), `fetDischarge` (bool), `balancing` (bool)
  - `connectionStatus` (enum: CONNECTED/DISCONNECTED)
- Q_PROPERTY 列表模型:
  - `cellVoltageModel` (CellVoltageModel*), `temperatureModel` (TemperatureModel*), `faultListModel` (FaultListModel*)
- Q_INVOKABLE:
  - `sendQuery(quint8 subCmd)`, `sendControl(quint8 cmd, quint16 mask)`, `sendConfig(quint8 param, quint16 value)`
  - `saveCsvLog(url)`, `loadDbcFile(url)`
- 30ms QTimer → `applySnapshot(snapshot)`: 批量比较新旧值，仅变更时 `setProperty` + emit NOTIFY

**CellVoltageModel (QAbstractListModel, 9 rows)**:
- Roles: `VoltageRole` (quint16 mV), `CellIndexRole` (int, 1-based), `IsMaxRole`/`IsMinRole` (bool), `ColorRole` (QColor)

**TemperatureModel (QAbstractListModel, 3 rows)**:
- Roles: `TempRole` (qreal °C), `SensorIndexRole` (int, 1-based), `ColorRole` (QColor)

**FaultListModel (QAbstractListModel, dynamic)**:
- Roles: `FaultNameRole` (QString), `FaultBitRole` (int, bit position), `SeverityRole` (enum: WARNING/ALERT/FAULT, 按故障位映射见附录 A)

**QML Views (qml/)**:
- `main.qml`: ApplicationWindow + TabBar 导航 (总览/电芯/曲线) + 底部状态栏(连接状态/最后更新时间)
- `OverviewPage.qml`: SOC GaugeArc + 总压/电流数值 + FET 状态指示灯(绿色●/灰色○) + 均衡状态 + FaultRibbon + 最高温度/最低温度摘要
- `CellsPage.qml`: 9 个 CellBar 柱状图(Repeater+CellVoltageModel) + 下方 3 路温度条(水平) + 均衡勾选(CheckBox per cell, 存储在 CellsPage 内部 ListModel，切换页面保持)
- `TrendsPage.qml`: QtCharts LineSeries × 4，TabBar 切换(总压/电流/SOC/温度)，300s 环形缓冲区，30fps 刷新，缩放平移
- `ControlPanel.qml`: 侧边抽屉 — FET 开关按钮(无故障且已连接时启用) + 均衡开启/关闭 + 清除故障 + 关机(确认对话框)
- `ConfigDialog.qml`: 6 参数输入(OV/UV/DSG_OC/CHG_OC/BAL_THRESH/BAL_MIN)，**初始值为空**，用户填入后 [发送配置] 发送 CAN 0x202 帧。输入校验: 范围提示见各参数定义
- `GaugeArc.qml`: Canvas 绘制 270° 圆弧仪表，颜色分段: [0-200]‰ 红色, [201-500]‰ 橙色, [501-1000]‰ 绿色，动画过渡
- `CellBar.qml`: 单节电压柱 Rectangle，高度映射 2800-4300mV→0-100%，颜色: >4200 红, 3600-4200 绿, 3000-3600 黄, <3000 蓝
- `FaultRibbon.qml`: 故障信息条，activeFaults 非零时红色+故障名列表，零时绿色"✓ 无活跃故障"

---

## 行为需求

### Requirement: CAN 连接管理
系统 SHALL 通过 PCAN-USB 适配器连接 BMS CAN 总线，支持连接/断开/重连。

#### Scenario: 正常连接
GIVEN PCAN-USB 已插入且驱动已安装
WHEN 用户启动应用程序
THEN 系统加载 QSettings 中保存的 plugin/interface/bitrate（首次使用默认 peakcan/usb0/500000）
AND 自动连接 CAN 设备
AND 状态栏显示 "● 已连接" + 最后更新时间
AND 开始接收 CAN 帧

#### Scenario: 适配器未插入
GIVEN PCAN-USB 未插入或驱动未安装
WHEN 用户启动应用程序
THEN 系统弹出错误对话框，列出 `QCanBus::availableDevices()` 返回的可用设备列表
AND 状态栏显示 "✗ 未连接"
AND 提供 [重试] 按钮

#### Scenario: 运行时断开
GIVEN CAN 连接正常
WHEN PCAN-USB 被拔出或总线错误发生或 500ms 无帧超时
THEN 状态栏显示 "⚠ 已断开"
AND 所有数据字段显示 "---"
AND connectionStatus = DISCONNECTED
AND 每 2 秒自动尝试重连，最多 10 次（之后停止，需用户手动 [重试]）
AND FET 控制按钮禁用

#### Scenario: BMS 主动关机后
GIVEN 连接正常，用户发送了 SHUTDOWN 指令
WHEN 500ms 后无 CAN 帧到达
THEN 状态栏显示 "⏻ BMS 已关机"（区别于意外断开）
AND **不**自动重连（等待用户手动操作）

### Requirement: 上行数据接收与解码
系统 SHALL 接收 BMS 周期上报的 CAN 帧并解码为物理值。

#### Scenario: 正常周期数据
GIVEN CAN 连接正常，BMS 以 100ms 周期发送 5 帧（0x110/0x111/0x120/0x121/0x130）
WHEN 系统接收到一批 CAN 帧
THEN 解码电芯电压为 mV 值（uint16 big-endian × 9，来自 0x110/0x111/0x120[6:7]）
AND 解码总压为 mV（uint16 big-endian，来自 0x120）
AND 解码电流为 mA（int16 big-endian，来自 0x120，×10 还原）
AND 解码 SOC 为 permil（uint16 big-endian，来自 0x130）
AND 解码 3 路温度为 0.1°C（int16 big-endian ×3，来自 0x121）
AND 解码 FET 状态（0x120 data[5] 高位）
AND 解码均衡状态（0x120 data[5] bit2）
AND 在 30ms 内批量更新 UI 数据模型

#### Scenario: 故障事件帧 — 故障发生
GIVEN BMS 发生故障
WHEN 系统接收到 0x101 故障帧且 level > 0
THEN 解码保护等级 data[0]（0=NONE/1=WARNING/2=ALERT/3=FAULT）
AND 解码 12-bit 故障掩码 data[1:2]（uint16 big-endian）
AND 解码最高电芯电压 = `(data[3]<<8)|data[4]` mV
AND 解码最低电芯电压 = `(data[5]<<8)|data[6]` mV
AND 解码最高温度 = `(data[7] - 40)` °C（1°C 分辨率）
AND FaultRibbon 切换为红色并列出活跃故障名
AND CSV 立即追加一行

#### Scenario: 故障恢复帧
GIVEN BMS 所有故障已清除
WHEN 系统接收到 0x101 帧且 data[0]=0x00, data[1:2]=0x0000
THEN activeFaults 更新为 0x0000
AND protLevel 更新为 NONE
AND FaultRibbon 切换为绿色 "✓ 无活跃故障"
AND FET 控制按钮恢复可用
AND CSV 立即追加一行

#### Scenario: 帧 DLC 不足
GIVEN 接收到有效 CAN 帧但 DLC 小于预期最小长度（见附录 A）
WHEN 系统解码该帧
THEN 跳过该帧不解码
AND 日志记录 "DLC too short: id=0xNNN, got=X, expected≥Y"（含时间戳）
AND 不影响其他帧的解码

#### Scenario: 帧乱序/丢帧
GIVEN CAN 总线上帧到达顺序不一致或个别帧丢失
WHEN 系统接收到不完整的帧批次
THEN 各帧按 CAN ID 独立解码，不依赖帧间顺序
AND 缺失帧的字段保持上一次有效值
AND 缺帧超过 1 秒 → cellDiff/cellMax/cellMin 标记为基于过期数据
AND 下一批帧到达时自动恢复

#### Scenario: 损坏帧
GIVEN CAN 总线上存在 CRC 错误或格式损坏的帧
WHEN 系统通过 `QCanBusFrame::isValid()` 检测到无效帧
THEN 跳过该帧并增加内部计数器
AND 不影响后续有效帧的处理

### Requirement: 数据可视化 — 总览页
系统 SHALL 在总览页以仪表盘形式展示 BMS 关键参数。

#### Scenario: SOC 仪表显示
GIVEN BMS 上报 SOC = 780‰ (78%)
WHEN 总览页可见
THEN GaugeArc 圆弧填充至 78% 位置，颜色为绿色（>50%）
AND 圆弧中央显示 "78%"
AND 动画过渡时长 500ms

#### Scenario: 数值显示
GIVEN BMS 上报 packVoltage=36720mV, packCurrent=-2350mA, remainingMah=18400
WHEN 总览页可见
THEN 总压显示 "36.72 V"
AND 电流显示 "-2.35 A (放电)"（负值=放电，正值=充电）
AND 剩余容量显示 "18.40 Ah"

#### Scenario: FET 与均衡状态指示
GIVEN 0x120 data[5] = 0x07 (prot=NONE, CHG=ON, DSG=ON, BAL=OFF)
WHEN 总览页可见
THEN FET 指示灯: CHG=绿色● DSG=绿色●
AND 均衡指示灯: ⚖ 灰色○（未均衡）
AND 文字标注 "充电: 开启 | 放电: 开启"

#### Scenario: 均衡状态指示
GIVEN 0x120 data[5] = 0x04 (BAL=ON)
WHEN 总览页可见
THEN 均衡指示灯: ⚖ 橙色●（均衡中）

#### Scenario: 故障状态 — 无故障
GIVEN activeFaults == 0x0000
WHEN 总览页可见
THEN FaultRibbon 背景色为绿色
AND 显示 "✓ 无活跃故障"

#### Scenario: 故障状态 — 有故障
GIVEN activeFaults == 0x0005 (CELL_OV | PACK_OV)
WHEN 总览页可见
THEN FaultRibbon 背景色为红色
AND 显示 "⚠ 2 个活跃故障: 单芯过压, 总压过压"

### Requirement: 数据可视化 — 电芯页
系统 SHALL 在电芯页以柱状图展示 9 节电芯电压和温度。

#### Scenario: 电芯电压柱状图
GIVEN 9 节电芯电压在 4065-4082mV 范围内
WHEN 电芯页可见
THEN 9 个 CellBar 垂直柱在 2.8-4.3V 坐标系内正确排列
AND 最高电芯 C3 标记 * 号并高亮
AND 最低电芯 C7 标记 _ 号并高亮
AND 颜色: C3(4082) 绿色, C7(4065) 绿色
AND 显示 "压差: 17 mV"

#### Scenario: 过压电芯告警
GIVEN C3 电压 = 4300mV (> 4200mV 阈值)
WHEN 电芯页可见
THEN C3 柱状图颜色为红色
AND 电芯编号旁显示 ⚠ 图标

#### Scenario: 温度显示
GIVEN 0x121 帧: TS1=285 (28.5°C), TS2=312 (31.2°C), TS3=298 (29.8°C)
WHEN 电芯页可见
THEN 温度值显示为 "TS1: 28.5°C  TS2: 31.2°C  TS3: 29.8°C"
AND TS2(31.2°C) 颜色为黄色（30-50°C 区间），其余为绿色（<30°C）

#### Scenario: 均衡勾选
GIVEN 电芯页可见
WHEN 用户勾选 C2 和 C5 的 CheckBox
THEN CheckBox 选中状态存储在 CellsPage 内部
AND 切换页面后勾选状态保持
AND 点击 ControlPanel [开启均衡] 时读取当前勾选状态构造平衡掩码

### Requirement: 数据可视化 — 曲线页
系统 SHALL 在曲线页滚动显示历史趋势。

#### Scenario: 实时趋势滚动
GIVEN 曲线页可见，300s 滚动窗口开启
WHEN BMS 持续上报数据
THEN 总压/电流/SOC/温度四条 LineSeries 持续更新
AND 旧数据点（> 300s）自动 `removePoints(0, n)`
AND 30fps QTimer 驱动 replot（非逐帧触发）
AND 数据追加使用批量 `QList<QPointF>` 而非逐点 append

#### Scenario: Tab 切换曲线
GIVEN 曲线页可见
WHEN 用户点击 "电流" Tab
THEN 显示电流趋势 LineSeries，隐藏其他三条
AND Y 轴自动调整范围
AND 隐藏的系列数据继续在后台累积（切换回来时显示完整历史）

#### Scenario: 交互操作
GIVEN 曲线页可见
WHEN 用户鼠标滚轮缩放
THEN X 轴时间范围缩放
AND 用户拖拽鼠标平移时间轴
AND [▶ 暂停] 按钮暂停实时滚动，[▶ 继续] 恢复

#### Scenario: 导出 CSV
GIVEN 曲线页可见且数据日志已启用
WHEN 用户点击 [📁 导出CSV]
THEN 弹出保存文件对话框
AND 将当前趋势缓冲区数据导出为 CSV（列: timestamp_ms, pack_voltage_mv, pack_current_ma, soc_permil, max_temp_0p1c）

### Requirement: 下行控制指令
系统 SHALL 支持发送全部 8 种控制指令到 BMS，每种指令均有独立场景。

#### Scenario: FET CHG_ON — 安全条件满足
GIVEN activeFaults == 0x0000 且 connectionStatus == CONNECTED
WHEN 用户点击 [充电 ON]
THEN 系统发送 CAN ID 0x201, data[0]=0x10, data[1..7]=0x00
AND 状态反馈 "充电 FET 已开启"

#### Scenario: FET CHG_OFF
GIVEN connectionStatus == CONNECTED
WHEN 用户点击 [充电 OFF]
THEN 系统发送 CAN ID 0x201, data[0]=0x11, data[1..7]=0x00

#### Scenario: FET DSG_ON — 安全条件满足
GIVEN activeFaults == 0x0000 且 connectionStatus == CONNECTED
WHEN 用户点击 [放电 ON]
THEN 系统发送 CAN ID 0x201, data[0]=0x20, data[1..7]=0x00

#### Scenario: FET DSG_OFF
GIVEN connectionStatus == CONNECTED
WHEN 用户点击 [放电 OFF]
THEN 系统发送 CAN ID 0x201, data[0]=0x21, data[1..7]=0x00

#### Scenario: FET 控制 — 有故障时拒绝
GIVEN 存在活跃故障（activeFaults != 0x0000）或未连接
WHEN 用户尝试点击 [充电 ON] 或 [放电 ON]
THEN 按钮呈灰色禁用状态
AND Tooltip 显示 "存在活跃故障，无法开启 FET" 或 "CAN 未连接"

#### Scenario: 均衡控制
GIVEN 电芯页 C2 和 C5 被勾选，connectionStatus == CONNECTED
WHEN 用户点击 [开启均衡]
THEN 系统发送 CAN ID 0x201, data[0]=0x30, data[1]=0x00, data[2]=0x12 (bit1|bit4, 电芯索引: C1=bit0..C9=bit8)
AND 状态反馈 "均衡指令已发送 (掩码: C2, C5)"

#### Scenario: 均衡关闭
GIVEN connectionStatus == CONNECTED
WHEN 用户点击 [关闭均衡]
THEN 系统发送 CAN ID 0x201, data[0]=0x31, data[1..7]=0x00

#### Scenario: 清除故障
GIVEN connectionStatus == CONNECTED
WHEN 用户点击 [清除故障]
THEN 系统发送 CAN ID 0x201, data[0]=0x01, data[1..7]=0x00
AND 状态反馈 "故障清除指令已发送"

#### Scenario: 关机 — 确认对话框
GIVEN 用户点击 [关机]
WHEN 弹出确认对话框 "确认向 BMS 发送关机指令？此操作将关闭电池输出。"
AND 用户点击 [确认]
THEN 系统发送 CAN ID 0x201, data[0]=0xFF, data[1..7]=0x00
AND 显示 "关机指令已发送"
AND connectionStatus 预期变为 "⏻ BMS 已关机"（500ms 后无帧）

#### Scenario: 关机 — 取消
GIVEN 关机确认对话框可见
WHEN 用户点击 [取消]
THEN 不发送任何 CAN 帧
AND 对话框关闭

### Requirement: 参数配置
系统 SHALL 支持在线修改 BMS 保护阈值。

#### Scenario: 修改保护阈值
GIVEN 配置对话框可见，connectionStatus == CONNECTED
WHEN 用户输入 "单芯过压 (mV)" = 4200 并点击 [发送配置]
THEN 系统发送 CAN ID 0x202, data[0]=0x01, data[1]=0x10, data[2]=0x68 (4200 big-endian), data[3..7]=0x00
AND 显示 "配置已发送"

#### Scenario: 输入校验
GIVEN 配置对话框可见
WHEN 用户输入的值超出有效范围（OV: 3000-5000, UV: 2000-3500, DSG_OC: 1000-100000, CHG_OC: 1000-50000, BAL_THRESH: 10-500, BAL_MIN: 2500-4000 mV）
THEN 输入框边框变红
AND [发送配置] 按钮禁用
AND Tooltip 显示具体有效范围

### Requirement: 数据查询
系统 SHALL 支持主动查询 BMS 指定数据，查询后 **等待 500ms 超时**。

#### Scenario: 查询超时
GIVEN CAN 连接正常
WHEN 用户点击 [刷新全部数据] (发送 0x200 data[0]=0x00)
AND 500ms 内未收到响应帧
THEN 显示 "查询超时"

#### Scenario: 查询全部数据
GIVEN CAN 连接正常
WHEN 用户点击 [刷新全部数据]
AND 500ms 内收到 0x100 STATUS 布局响应
THEN 系统解码 SOC%/总压/电流/保护等级并更新 UI

#### Scenario: 查询电芯电压
GIVEN CAN 连接正常
WHEN 用户选择查询 "电芯电压" (发送 0x200 data[0]=0x02)
AND 500ms 内收到 0x110 帧
THEN 系统解码 C1-C4 电压并更新对应 CellBar（注: 仅前 4 节，C5-C9 不在此查询响应中）

### Requirement: 数据日志
系统 SHALL 支持将 BMS 数据记录为 CSV 文件。

#### Scenario: 定时日志
GIVEN CSV 日志已启用，interval=1000ms
WHEN BMS 数据持续到达
THEN 距上次写入 ≥1000ms 时追加一行
AND 每行包含: ISO 8601 UTC 时间戳 + pack_voltage_mv + pack_current_ma + soc_permil + prot_level + active_faults + cell_1_mv..cell_9_mv + ts1_0p1c + ts2_0p1c + ts3_0p1c + fet_chg(0/1) + fet_dsg(0/1) + balancing(0/1)

#### Scenario: 故障触发日志
GIVEN CSV 日志已启用
WHEN activeFaults 发生变化（0→非0 或 非0→0 或 掩码变化）
THEN 立即追加一行（不受 interval 限制）
AND 写入后重置 interval 计时器

#### Scenario: 日志文件轮转
GIVEN CSV 日志已启用
WHEN 文件大小超过 10MB
THEN 关闭当前文件 → 重命名为 `bms_log_YYYYMMDDTHHMMSS.csv` → 以相同路径创建新文件
AND 保留最近 10 个轮转文件，删除更旧的

### Requirement: 配置持久化
系统 SHALL 在配置变更时立即保存，启动时恢复。

#### Scenario: 窗口布局恢复
GIVEN 用户上次使用后退出了应用
WHEN 用户再次启动应用
THEN 窗口恢复到上次的位置和大小
AND TabBar 恢复到上次的页面

#### Scenario: CAN 配置恢复
GIVEN 用户上次保存了 CAN 接口参数
WHEN 用户再次启动应用
THEN 自动加载上次的 plugin/interface/bitrate 配置
AND QSettings 中的保存值优先于硬编码默认值

### Requirement: 国际化
系统 SHALL 支持中文界面，代码预留 i18n 能力。

#### Scenario: 中文显示
GIVEN 应用启动
WHEN 加载默认翻译
THEN 全部 UI 文本（菜单/标签/按钮/提示/故障名）显示为中文
AND 故障名显示为中文（如 "单芯过压" 而非 "CELL_OV"）

#### Scenario: i18n 预留
GIVEN 全部用户可见字符串使用 `tr()` 包裹
WHEN 需要添加英文支持
THEN 通过 `lupdate` 提取 → 翻译 → `lrelease` 生成 .qm → 加载即可
AND 无需修改任何源代码

---

## 技术决策

| 决策 | 理由 |
|------|------|
| Qt 6.5+ LTS + CMake | Qt6 是当前 LTS，DBC 解码内置（QCanDbcFileParser），CMake 是 Qt6 唯一支持的构建系统 |
| PCAN-USB + peakcan 插件 | Qt 官方插件零代码接入，Windows 驱动成熟，用户指定 |
| QML 纯声明式 UI | 用户指定。仪表盘效果最佳，GPU 渲染动画流畅 |
| Qt Charts QML 曲线 | 官方 QML 集成，OpenGL 加速，用户指定 |
| 混合分层 MVVM（Q_PROPERTY + QAbstractListModel）| 标量绑定简洁（属性），列表稳扎（Model/View），30ms 批量快照避免信号洪峰。详见 ADR-0008 |
| Worker Thread CAN 隔离 | QCanBusDevice 非线程安全，必须单线程操作；跨线程 QueuedConnection 标准模式 |
| 内置手写解码器为主，DBC 可选 | DBC API 实验性（Qt 6.5），手写解码器零依赖兜底 |
| QSettings 即时保存（非退出时）| 避免崩溃丢失配置；窗口几何等高频变化项 500ms 防抖保存 |
| 全功能一期交付 | 用户明确指定。模块化设计使各模块可独立开发测试 |
| 固件 CAN 协议小幅扩展 | 用户选择方向 A。上位机需要温度/FET/均衡状态数据来满足全功能 spec |

## 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| QML 高频更新造成 UI 卡顿 | 中 | 30ms 批量快照 + 仅变更时 NOTIFY + Behavior 动画吸收差值 |
| Qt Charts QML 长时运行内存增长 | 中 | 环形缓冲区 3000 点/系列上限 + 定期 `removePoints(0, n)` |
| Qt Charts GPLv3 商业化受限 | 低 | 学习项目无影响；后续可替换 Qt Graphs (LGPL) 或 Canvas |
| QCanDbcFileParser 实验性 API 变更 | 低 | 内置解码器兜底，DBC 仅作可选增强 |
| PCAN 驱动/DLL 缺失 | 低 | 启动时 `availableDevices()` 检查 + 友好错误提示 |
| QML 学习曲线 | 低 | 自定义组件(仪表/柱状图)参考开源实现(Serial Studio, AGL cluster demo) |
| 固件协议修改引入 bug | 低 | 固件修改约 30 行，仅涉及 CAN 帧打包；上位机解码器单元测试全覆盖 |

---

## 附录 A: CAN 协议完整定义（含固件修改后）

### 上行帧 (BMS → 上位机)

| CAN ID | 名称 | 周期 | DLC≥ | Data[0] | Data[1] | Data[2] | Data[3] | Data[4] | Data[5] | Data[6] | Data[7] |
|--------|------|------|------|---------|---------|---------|---------|---------|---------|---------|---------|
| 0x100 | BMS_STATUS (query resp) | 查询响应 | 8 | SOC% | PackV_H | PackV_L | I_H | I_L | prot_lvl | 0 | 0 |
| 0x100 | BMS_PROTECTION (query resp) | 查询响应 | 8 | prot_lvl | faults_H | faults_L | max_mV_H | max_mV_L | min_mV_H | min_mV_L | 0 |
| 0x101 | BMS_FAULT | 事件驱动 | 8 | level | faults_H | faults_L | max_mV_H | max_mV_L | min_mV_H | min_mV_L | maxT+40 |
| 0x110 | CELL_VOLT_1_4 | 100ms | 8 | C1_H | C1_L | C2_H | C2_L | C3_H | C3_L | C4_H | C4_L |
| 0x111 | CELL_VOLT_5_8 | 100ms | 8 | C5_H | C5_L | C6_H | C6_L | C7_H | C7_L | C8_H | C8_L |
| 0x120 | BMS_STATUS | 100ms | 8 | SOC% | PackV_H | PackV_L | I_H | I_L | ctrl | C9_H | C9_L |
| 0x121 | TEMPERATURE | 100ms | 6 | TS1_H | TS1_L | TS2_H | TS2_L | TS3_H | TS3_L | 0 | 0 |
| 0x130 | SOC_OCV | 100ms | 8 | SOC_H | SOC_L | rSOC_H | rSOC_L | rmAh_H | rmAh_L | qMax_H | qMax_L |

**解码规则**:
- 电压: `(byte_H << 8) | byte_L` → mV
- 电流: `int16_t` 有符号（big-endian），值 × 10 → mA（正=充电，负=放电）
- SOC: `(byte_H << 8) | byte_L` → permil (‰)，÷10 = %
- SOC% 字段 (data[0] in 0x100/0x120): 已经是百分比 (0-100)
- 温度 (0x121): `int16_t` big-endian → 0.1°C（÷10 = °C）。颜色: <0°C 蓝色, 0-30°C 绿色, 30-50°C 黄色, >50°C 红色
- 故障帧温度 (0x101 data[7]): `data[7] - 40` → °C（1°C 分辨率）
- 剩余容量: `(byte_H << 8) | byte_L` × 100 → mAh
- Q_max: `(byte_H << 8) | byte_L` × 100 → mAh

**0x120 ctrl 字节 (data[5]) 位定义**:

| Bit | 名称 | 说明 |
|-----|------|------|
| [1:0] | prot_level | 保护等级 (0=NONE, 1=WARNING, 2=ALERT, 3=FAULT) |
| [2] | fet_chg | 充电 FET (1=开启, 0=关闭) |
| [3] | fet_dsg | 放电 FET (1=开启, 0=关闭) |
| [4] | balancing | 均衡 (1=均衡中, 0=未均衡) |
| [7:5] | reserved | 保留 (0) |

**0x100 双布局**: QUERY sub_cmd=0x00/0x01 → 返回 STATUS 布局（第一行）；sub_cmd=0x03 → 返回 PROTECTION 布局（第二行）。上位机解码时根据自己发出的 sub_cmd 选择布局。

**0x101 双语义**: data[0]>0 + data[1:2]!=0 → 故障发生帧；data[0]=0 + data[1:2]=0 → 故障恢复帧（所有故障已清除）。

### 下行帧 (上位机 → BMS)

| CAN ID | 名称 | Data[0] | Data[1] | Data[2] | Data[3..7] |
|--------|------|---------|---------|---------|------------|
| 0x200 | QUERY | sub_cmd | 0 | 0 | 0 |
| 0x201 | CONTROL | cmd | mask_H | mask_L | 0 |
| 0x202 | CONFIG | param | val_H | val_L | 0 |

**子命令/参数定义**:
- QUERY sub_cmd: 0x00=ALL(→0x100 STATUS), 0x01=STATUS(→0x100 STATUS), 0x02=CELLS(→0x110 C1-C4 only), 0x03=PROTECTION(→0x100 PROTECTION), 0x04=SOC(→0x130)
- CONTROL cmd: 0x01=CLEAR_FAULT, 0x10=CHG_ON, 0x11=CHG_OFF, 0x20=DSG_ON, 0x21=DSG_OFF, 0x30=BAL_SET, 0x31=BAL_OFF, 0xFF=SHUTDOWN
- BAL_SET 均衡掩码: C1=bit0, C2=bit1, ..., C9=bit8（data[1]=高字节, data[2]=低字节）
- CONFIG param: 0x01=cell_ov_mV, 0x02=cell_uv_mV, 0x03=discharge_oc_mA, 0x04=charge_oc_mA, 0x05=balance_thresh_mV, 0x06=balance_min_mV

**配置参数有效范围**:

| Param | 名称 | 范围 | 单位 |
|-------|------|------|------|
| 0x01 | cell_ov_mV | 3000-5000 | mV |
| 0x02 | cell_uv_mV | 2000-3500 | mV |
| 0x03 | discharge_oc_mA | 1000-100000 | mA |
| 0x04 | charge_oc_mA | 1000-50000 | mA |
| 0x05 | balance_thresh_mV | 10-500 | mV |
| 0x06 | balance_min_mV | 2500-4000 | mV |

### 故障位定义 (12-bit) 与严重等级

| Bit | 名称 | 严重等级 | Bit | 名称 | 严重等级 |
|-----|------|---------|-----|------|---------|
| 0 | 单芯过压 (CELL_OV) | FAULT | 6 | 短路 (SHORT_CIRCUIT) | FAULT |
| 1 | 单芯欠压 (CELL_UV) | FAULT | 7 | 过温 (OVER_TEMP) | ALERT |
| 2 | 总压过压 (PACK_OV) | FAULT | 8 | 欠温 (UNDER_TEMP) | ALERT |
| 3 | 总压欠压 (PACK_UV) | FAULT | 9 | 压差过大 (CELL_IMBALANCE) | WARNING |
| 4 | 放电过流 (DISCHARGE_OC) | FAULT | 10 | 通信丢失 (COMM_LOSS) | FAULT |
| 5 | 充电过流 (CHARGE_OC) | FAULT | 11 | 看门狗 (WATCHDOG) | FAULT |

---

## 附录 B: Q_PROPERTY 列表

```cpp
// BmsDataModel — 暴露给 QML 的属性
Q_PROPERTY(quint32 packVoltage     READ packVoltage     NOTIFY packVoltageChanged)
Q_PROPERTY(qint32  packCurrent     READ packCurrent     NOTIFY packCurrentChanged)
Q_PROPERTY(quint16 socPermil       READ socPermil       NOTIFY socPermilChanged)
Q_PROPERTY(quint16 realSocPermil   READ realSocPermil   NOTIFY realSocPermilChanged)
Q_PROPERTY(qint32  remainingMah    READ remainingMah    NOTIFY remainingMahChanged)
Q_PROPERTY(qint32  qMaxMah         READ qMaxMah         NOTIFY qMaxMahChanged)
Q_PROPERTY(quint8  protLevel       READ protLevel       NOTIFY protLevelChanged)
Q_PROPERTY(quint16 activeFaults    READ activeFaults    NOTIFY activeFaultsChanged)
Q_PROPERTY(quint16 cellMin         READ cellMin         NOTIFY cellMinChanged)
Q_PROPERTY(quint16 cellMax         READ cellMax         NOTIFY cellMaxChanged)
Q_PROPERTY(quint16 cellDiff        READ cellDiff        NOTIFY cellDiffChanged)
Q_PROPERTY(bool    fetCharge       READ fetCharge       NOTIFY fetChargeChanged)
Q_PROPERTY(bool    fetDischarge    READ fetDischarge    NOTIFY fetDischargeChanged)
Q_PROPERTY(bool    balancing       READ balancing       NOTIFY balancingChanged)
Q_PROPERTY(int     connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
Q_PROPERTY(QObject* cellVoltageModel READ cellVoltageModel CONSTANT)
Q_PROPERTY(QObject* temperatureModel READ temperatureModel CONSTANT)
Q_PROPERTY(QObject* faultListModel   READ faultListModel   CONSTANT)
```

---

## 附录 C: BMS 固件修改清单

固件需要以下修改以支持上位机完整功能：

| 文件 | 修改 | 行数估计 |
|------|------|---------|
| `App/Src/can_cmd.c` | `can_pub()`: 新增 0x121 帧（3 路温度 int16 ×3），0x120 data[5] 改为 ctrl 字节编码 | ~15 行 |
| `App/Src/bms_app.c` | `task_protect_entry()`: 0x101 故障帧 layout 修改（max_mV/min_mV 改为 uint16 big-endian，移除电流字段）| ~10 行 |
| `App/Inc/can_cmd.h` | 新增 `CAN_TX_TEMPERATURE 0x121U` 宏定义 | 1 行 |
| `task_plan.md` | CAN 协议节同步更新 | ~5 行 |

固件修改不改变任何 App 层算法或 BSP 层行为——**仅影响 CAN 帧打包格式**。修改后需编译验证（0 错误 0 警告）。
