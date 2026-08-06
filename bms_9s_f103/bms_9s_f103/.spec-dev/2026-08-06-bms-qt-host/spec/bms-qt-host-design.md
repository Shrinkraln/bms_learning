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
- 实时接收并解码全部 6 种上行 CAN 帧（0x100/0x101/0x110/0x111/0x120/0x130），刷新率 100ms
- 3 页面可视化：总览（仪表盘）、电芯（电压柱状图+温度）、曲线（滚动趋势图）
- 全部 8 种下行 CAN 指令可发送（查询/控制/配置），含安全条件校验
- CSV 数据日志，DBC 文件可选加载
- 信号超时检测（>500ms 无帧 → "已断开"状态）
- 编译 0 错误 0 警告，Qt 6.5+ / CMake / MSVC 或 MinGW

## 非目标

- 移动端（Android/iOS）部署
- 多 BMS 设备同时连接（仅支持单设备）
- CAN 总线回放/模拟器
- 远程 Web 访问
- 固件 OTA 升级

## 术语表

| 术语 | 定义 | Avoid |
|------|------|-------|
| Snapshot | 一次完整 BMS 数据采样，包含电芯电压/温度/电流/SOC/保护状态 | frame batch、data packet |
| permil (‰) | SOC 内部精度单位，0-1000 对应 0%-100% | per mille |
| FET | 场效应管，BMS 通过 CHG FET / DSG FET 控制充/放电回路开关 | MOSFET、开关管 |
| 上行帧 | BMS MCU → 上位机的 CAN 帧（遥测数据）| uplink、telemetry |
| 下行帧 | 上位机 → BMS MCU 的 CAN 帧（指令）| downlink、command |

## 影响面

本需求为**新建独立项目**，不影响现有 BMS 固件代码。新建仓库目录 `BmsHostApp/`，与 `bms_9s_f103/` 同级。

| 模块 | 影响 | 说明 |
|------|------|------|
| BMS 固件 | 无 | 上位机仅通过 CAN 总线通信，固件无感知 |
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
  → BmsSnapshot 结构体
  → BmsDataModel::applySnapshot(snapshot)
    → Q_PROPERTY NOTIFY (仅变更时发射)
    → CellVoltageModel::dataChanged (仅变更 cell)
    → CsvLogger::appendRow (条件写入)
  → QML Views (属性绑定自动刷新)
```

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
- `decodeFrames(batch) → QVector<BmsSnapshot>`: 按 CAN ID 分发解码
- `encodeCommand(BmsCommand) → CanFrame`: 下行指令编码
- 内置手写解码器（主路径）；DBC 解码器可选（`BmsDbcHelper`）
- 解码规则（与 BMS 固件完全一致，见附录 A）

**BmsDbcHelper (protocol/BmsDbcHelper.h/cpp)**:
- `loadDbc(path) → bool`: 加载 DBC 文件
- `decodeFrame(frame) → QMap<QString, QVariant>`: DBC 解码单帧
- 使用 `QCanDbcFileParser` + `QCanFrameProcessor`（Qt 6.5+ 实验性 API）
- 加载失败回退到内置解码器

**CsvLogger (protocol/CsvLogger.h/cpp)**:
- `start(filePath, intervalMs)`: 打开 CSV 文件，写表头
- `appendRow(snapshot)`: 缓冲写入（内部计数，达到 interval 才 flush）
- `stop()`: flush + close
- 列: timestamp, pack_voltage_mv, pack_current_ma, soc_permil, prot_level, active_faults, cell_1_mv..cell_9_mv, ts1_mdeg, ts2_mdeg, ts3_mdeg, fet_chg, fet_dsg

**BmsDataModel (model/BmsDataModel.h/cpp)**:
- 中央数据模型，QObject 单例，注册到 QML context
- Q_PROPERTY 标量（12 个，全部 NOTIFY）:
  - `packVoltage` (quint32, mV), `packCurrent` (qint32, mA)
  - `socPermil` (quint16, ‰), `realSocPermil` (quint16, ‰)
  - `remainingMah` (qint32), `qMaxMah` (qint32)
  - `protLevel` (quint8), `activeFaults` (quint16)
  - `cellMin`/`cellMax`/`cellDiff` (quint16, mV)
  - `connectionStatus` (enum: CONNECTED/STALE/DISCONNECTED)
- Q_PROPERTY 列表模型:
  - `cellVoltageModel` (CellVoltageModel*), `temperatureModel` (TemperatureModel*), `faultListModel` (FaultListModel*)
- Q_INVOKABLE:
  - `sendQuery(quint8 subCmd)`, `sendControl(quint8 cmd, quint16 mask)`, `sendConfig(quint8 param, quint16 value)`
  - `saveCsvLog(url)`, `loadDbcFile(url)`
- 30ms QTimer → `applySnapshot(snapshot)`: 批量比较新旧值，仅变更时 `setProperty` + emit NOTIFY

**CellVoltageModel (QAbstractListModel, 9 rows)**:
- Roles: `VoltageRole` (quint16 mV), `CellIndexRole` (int), `IsMaxRole`/`IsMinRole` (bool), `IsBalancingRole` (bool), `ColorRole` (QColor)

**TemperatureModel (QAbstractListModel, 3 rows)**:
- Roles: `TempRole` (qreal °C), `SensorIndexRole`, `ColorRole`

**FaultListModel (QAbstractListModel, dynamic)**:
- Roles: `FaultNameRole` (QString), `FaultCodeRole` (int bit), `SeverityRole` (enum)

**QML Views (qml/)**:
- `main.qml`: ApplicationWindow + TabBar 导航 (总览/电芯/曲线) + 状态栏(连接状态/最后更新时间)
- `OverviewPage.qml`: SOC GaugeArc + 总压/电流数值 + FET 状态指示灯 + FaultRibbon + 温度/压差摘要
- `CellsPage.qml`: 9 个 CellBar 柱状图(Repeater+CellVoltageModel) + 温度条 + 均衡状态标记
- `TrendsPage.qml`: QtCharts LineSeries × 4，TabBar 切换(总压/电流/SOC/温度)，300s 环形缓冲区，30fps 刷新，缩放平移
- `ControlPanel.qml`: 侧边抽屉 — FET 开关按钮(无故障时启用) + 均衡设置 + 清除故障 + 关机(确认对话框)
- `ConfigDialog.qml`: 6 参数输入(OV/UV/DSG_OC/CHG_OC/BAL_THRESH/BAL_MIN) + 发送按钮
- `GaugeArc.qml`: Canvas 绘制 270° 圆弧仪表，颜色渐变(红→橙→绿)，动画过渡
- `CellBar.qml`: 单节电压柱 Rectangle，高度映射 3000-4200mV→0-100%，颜色按阈值编码
- `FaultRibbon.qml`: 故障信息条，activeFaults 非零时红色+故障名列表，零时绿色"无活跃故障"

---

## 行为需求

### Requirement: CAN 连接管理
系统 SHALL 通过 PCAN-USB 适配器连接 BMS CAN 总线，支持连接/断开/重连。

#### Scenario: 正常连接
GIVEN PCAN-USB 已插入且驱动已安装
WHEN 用户启动应用程序
THEN 系统自动连接 `peakcan` 插件的 `usb0` 接口，500kbps 波特率
AND 状态栏显示 "● 已连接"
AND 开始接收 CAN 帧

#### Scenario: 适配器未插入
GIVEN PCAN-USB 未插入或驱动未安装
WHEN 用户启动应用程序
THEN 系统弹出错误对话框，列出 `QCanBus::availableDevices()` 返回的可用设备列表
AND 状态栏显示 "✗ 未连接"
AND 提供 [重试] 按钮

#### Scenario: 运行时断开
GIVEN CAN 连接正常
WHEN PCAN-USB 被拔出或总线错误发生
THEN 系统在 500ms 内检测到帧超时
AND 状态栏显示 "⚠ 已断开"
AND 所有数据字段显示 "---"
AND 每 2 秒自动尝试重连

### Requirement: 上行数据接收与解码
系统 SHALL 接收 BMS 周期上报的 CAN 帧并解码为物理值。

#### Scenario: 正常周期数据
GIVEN CAN 连接正常，BMS 以 100ms 周期发送 4 帧（0x110/0x111/0x120/0x130）
WHEN 系统接收到一批 CAN 帧
THEN 解码电芯电压为 mV 值（uint16 big-endian × 9）
AND 解码总压为 mV（uint16 big-endian）
AND 解码电流为 mA（int16 big-endian，×10 还原）
AND 解码 SOC 为 permil（uint16 big-endian）
AND 解码温度为 0.1°C（int16 big-endian）
AND 在 30ms 内批量更新 UI 数据模型

#### Scenario: 故障事件帧
GIVEN BMS 发生故障
WHEN 系统接收到 0x101 故障帧
THEN 解码保护等级（0=NONE/1=WARNING/2=ALERT/3=FAULT）
AND 解码 12-bit 故障掩码
AND 解码最高/最低电芯电压 ≈ (data[3]/data[4] × 10 mV)
AND 解码最高温度 ≈ (data[7] - 40 °C)
AND FaultRibbon 从绿色切换为红色并列出活跃故障名

#### Scenario: 帧乱序/丢帧
GIVEN CAN 总线上帧到达顺序与发送顺序不一致
WHEN 系统接收到一帧
THEN 按 CAN ID 分别解码，不依赖帧间顺序
AND 单帧缺失不阻塞其他帧的解码
AND 缺失的帧在下一次周期到达时自动补充

#### Scenario: 损坏帧
GIVEN CAN 总线上存在 CRC 错误或格式损坏的帧
WHEN 系统通过 `QCanBusFrame::isValid()` 检测到无效帧
THEN 跳过该帧并记录到日志（计数 + 时间戳）
AND 不影响后续有效帧的处理

### Requirement: 数据可视化 — 总览页
系统 SHALL 在总览页以仪表盘形式展示 BMS 关键参数。

#### Scenario: SOC 仪表显示
GIVEN BMS 上报 SOC = 780‰ (78%)
WHEN 总览页可见
THEN GaugeArc 圆弧填充至 78% 位置，颜色为绿色
AND 圆弧中央显示 "78%"
AND 动画过渡时长 500ms

#### Scenario: 数值显示
GIVEN BMS 上报 packVoltage=36720mV, packCurrent=-2350mA
WHEN 总览页可见
THEN 总压显示 "36.72 V"
AND 电流显示 "-2.35 A (放电)"（负值=放电，正值=充电）
AND 剩余容量显示计算后的 Ah 值

#### Scenario: FET 状态指示
GIVEN BMS 上报 protLevel=NONE
WHEN 总览页可见
THEN FET 指示灯: CHG=绿色● DSG=绿色●
AND 文字标注 "充电: 开启 | 放电: 开启"

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
THEN 9 个 CellBar 垂直柱在 3.0-4.2V 坐标系内正确排列
AND 最高电芯 C3 标记 * 号并高亮
AND 最低电芯 C7 标记 _ 号并高亮
AND 颜色: 4000-4100mV 为绿色
AND 显示压差 = 17mV

#### Scenario: 过压电芯告警
GIVEN C3 电压 = 4300mV (> 4100mV 阈值)
WHEN 电芯页可见
THEN C3 柱状图颜色为红色
AND 电芯编号旁显示 ⚠ 图标

#### Scenario: 温度显示
GIVEN 3 路 NTC 温度传感器值
WHEN 电芯页可见
THEN 温度值以 °C 显示（ts_mdeg / 10 → °C，保留 1 位小数）
AND 高于 50°C 显示红色，低于 -5°C 显示蓝色

### Requirement: 数据可视化 — 曲线页
系统 SHALL 在曲线页滚动显示历史趋势。

#### Scenario: 实时趋势滚动
GIVEN 曲线页可见，300s 滚动窗口开启
WHEN BMS 持续上报数据
THEN 总压/电流/SOC/温度四条 LineSeries 持续更新
AND 旧数据点（> 300s）自动移除
AND 30fps 刷新率，无视觉闪烁

#### Scenario: Tab 切换曲线
GIVEN 曲线页可见
WHEN 用户点击 "电流" Tab
THEN 显示电流趋势 LineSeries，隐藏其他三条
AND Y 轴自动调整范围

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
AND 将当前缓冲区数据导出为 CSV（列: timestamp, 各系列值）

### Requirement: 下行控制指令
系统 SHALL 支持发送控制指令到 BMS。

#### Scenario: FET 控制 — 安全条件
GIVEN 无活跃故障（activeFaults == 0x0000）
WHEN 用户点击 [充电 ON]
THEN 系统发送 CAN ID 0x201, data[0]=0x10
AND 状态反馈 "充电 FET 已开启"

#### Scenario: FET 控制 — 有故障拒绝
GIVEN 存在活跃故障（activeFaults != 0x0000）
WHEN 用户尝试点击 [充电 ON] 或 [放电 ON]
THEN 按钮呈灰色禁用状态
AND 提示 "存在活跃故障，无法开启 FET"

#### Scenario: 均衡控制
GIVEN 电芯页可见，C2 和 C5 被勾选
WHEN 用户点击 [开启均衡]
THEN 系统发送 CAN ID 0x201, data[0]=0x30, data[1..2]=0x0012 (bit1+bit4)
AND C2 和 C5 CellBar 显示橙色均衡中标记

#### Scenario: 关机 — 确认对话框
GIVEN 用户点击 [关机]
WHEN 弹出确认对话框 "确认向 BMS 发送关机指令？此操作将关闭电池输出。"
AND 用户点击 [确认]
THEN 系统发送 CAN ID 0x201, data[0]=0xFF
AND 显示 "关机指令已发送"

#### Scenario: 关机 — 取消
GIVEN 关机确认对话框可见
WHEN 用户点击 [取消]
THEN 不发送任何 CAN 帧
AND 对话框关闭

### Requirement: 参数配置
系统 SHALL 支持在线修改 BMS 保护阈值。

#### Scenario: 修改保护阈值
GIVEN 配置对话框可见
WHEN 用户修改 "单芯过压 (mV)" 为 4200 并点击 [发送配置]
THEN 系统发送 CAN ID 0x202, data[0]=0x01, data[1..2]=0x1068 (4200 big-endian)
AND 显示 "配置已发送"

#### Scenario: 输入校验
GIVEN 配置对话框可见
WHEN 用户输入 "单芯过压" = 5000（超出有效范围 3000-5000mV）
THEN 输入框边框变红
AND [发送配置] 按钮禁用
AND 提示 "有效范围: 3000 - 5000 mV"

### Requirement: 数据查询
系统 SHALL 支持主动查询 BMS 指定数据。

#### Scenario: 查询全部数据
GIVEN CAN 连接正常
WHEN 用户点击 [刷新全部数据]
THEN 系统发送 CAN ID 0x200, data[0]=0x00
AND BMS 返回 BMS_STATUS 帧 (0x100)，系统解码并更新 UI

#### Scenario: 查询指定数据
GIVEN CAN 连接正常
WHEN 用户选择查询 "电芯电压"
THEN 系统发送 CAN ID 0x200, data[0]=0x02
AND BMS 返回 CELL_VOLT_1_4 帧

### Requirement: 数据日志
系统 SHALL 支持将 BMS 数据记录为 CSV 文件。

#### Scenario: 定时日志
GIVEN CSV 日志已启用，间隔 1000ms
WHEN BMS 数据持续到达
THEN 每隔 1000ms 追加一行到 CSV 文件
AND 每行包含: 时间戳 + 全部电压/电流/SOC/温度/故障/FET 字段

#### Scenario: 故障触发日志
GIVEN CSV 日志已启用
WHEN activeFaults 从 0x0000 变为非零
THEN 立即追加一行（不受间隔限制）
AND 后续故障状态变化也立即记录

#### Scenario: 日志文件管理
GIVEN CSV 日志已启用
WHEN 文件大小超过 10MB
THEN 自动轮转: 关闭当前文件 → 重命名为 `bms_log_YYYYMMDD_HHMMSS.csv` → 创建新文件

### Requirement: 配置持久化
系统 SHALL 在应用退出时保存用户配置，启动时恢复。

#### Scenario: 窗口布局恢复
GIVEN 用户上次使用后退出了应用
WHEN 用户再次启动应用
THEN 窗口恢复到上次的位置和大小
AND TabBar 恢复到上次的页面

#### Scenario: CAN 配置恢复
GIVEN 用户上次配置了 CAN 接口参数
WHEN 用户再次启动应用
THEN 自动加载上次的 plugin/interface/bitrate 配置
AND 无需每次手动输入

### Requirement: 国际化
系统 SHALL 支持中文界面，代码预留 i18n 能力。

#### Scenario: 中文显示
GIVEN 系统语言为中文
WHEN 应用启动
THEN 全部 UI 文本（菜单/标签/按钮/提示）显示为中文
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
| Qt 6.5+ LTS + CMake | Qt6 是当前 LTS，DBC 解码内置（QCanDbcFileParser），CMake 是 Qt6 唯一支持构建系统 |
| PCAN-USB + peakcan 插件 | Qt 官方插件零代码接入，Windows 驱动成熟，用户指定 |
| QML 纯声明式 UI | 用户指定。仪表盘效果最佳，GPU 渲染动画流畅 |
| Qt Charts QML 曲线 | 官方 QML 集成，OpenGL 加速，用户指定 |
| 混合分层 MVVM（Q_PROPERTY + QAbstractListModel）| 标量绑定简洁（属性），列表稳扎（Model/View），30ms 批量快照避免信号洪峰 |
| Worker Thread CAN 隔离 | QCanBusDevice 非线程安全，必须单线程操作；跨线程 QueuedConnection 标准模式 |
| 内置手写解码器为主，DBC 可选 | DBC API 实验性（Qt 6.5），手写解码器零依赖兜底 |
| QSettings INI + JSON 预设 | QSettings 适合窗口/CAN 配置；JSON 适合多套 BMS 阈值预设的分享和版本管理 |
| 全功能一期交付 | 用户明确指定。模块化设计使各模块（监控/控制/日志/DBC）可独立开发测试 |

## 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| QML 高频更新造成 UI 卡顿 | 中 | 30ms 批量快照 + 仅变更时 NOTIFY + Behavior 动画吸收差值 |
| Qt Charts QML 长时运行内存增长 | 中 | 环形缓冲区 3000 点/系列上限 + 定期 `removePoints(0, n)` |
| Qt Charts GPLv3 商业化受限 | 低 | 学习项目无影响；后续可替换 Qt Graphs (LGPL) 或 Canvas |
| QCanDbcFileParser 实验性 API 变更 | 低 | 内置解码器兜底，DBC 仅作可选增强 |
| PCAN 驱动/DLL 缺失 | 低 | 启动时 `availableDevices()` 检查 + 友好错误提示 |
| QML 学习曲线 | 低 | 自定义组件(仪表/柱状图)参考开源实现(Serial Studio, AGL cluster demo) |

---

## 附录 A: CAN 协议完整定义

### 上行帧 (BMS → 上位机)

| CAN ID | 名称 | 周期 | Data[0] | Data[1] | Data[2] | Data[3] | Data[4] | Data[5] | Data[6] | Data[7] |
|--------|------|------|---------|---------|---------|---------|---------|---------|---------|---------|
| 0x100 | BMS_STATUS | 查询响应 | SOC% | PackV_H | PackV_L | I_H | I_L | prot_lvl | 0 | 0 |
| 0x101 | BMS_FAULT | 事件驱动 | level | faults_H | faults_L | max_mV/10 | min_mV/10 | I_H | I_L | maxT/10+40 |
| 0x110 | CELL_VOLT_1_4 | 100ms | C1_H | C1_L | C2_H | C2_L | C3_H | C3_L | C4_H | C4_L |
| 0x111 | CELL_VOLT_5_9 | 100ms | C5_H | C5_L | C6_H | C6_L | C7_H | C7_L | C8_H | C8_L |
| 0x120 | BMS_STATUS | 100ms | SOC% | PackV_H | PackV_L | I_H | I_L | prot_lvl | C9_H | C9_L |
| 0x130 | SOC_OCV | 100ms | SOC_H | SOC_L | rSOC_H | rSOC_L | rmAh_H | rmAh_L | qMax_H | qMax_L |

**解码规则**:
- 电压: `(byte_H << 8) | byte_L` → mV
- 电流: `int16_t` 有符号，值 × 10 → mA（正=充电，负=放电）
- SOC: `(byte_H << 8) | byte_L` → permil (‰)，÷10 = %
- SOC% 字段 (data[0]  in 0x100/0x120): 已经是百分比 (0-100)
- 温度: 故障帧 data[7] - 40 → °C
- 剩余容量: `(byte_H << 8) | byte_L` × 100 → mAh
- Q_max: `(byte_H << 8) | byte_L` × 100 → mAh

### 下行帧 (上位机 → BMS)

| CAN ID | 名称 | Data[0] | Data[1] | Data[2] | Data[3..7] |
|--------|------|---------|---------|---------|------------|
| 0x200 | QUERY | sub_cmd | — | — | — |
| 0x201 | CONTROL | cmd | mask_H | mask_L | — |
| 0x202 | CONFIG | param | val_H | val_L | — |

**子命令/参数定义**:
- QUERY sub_cmd: 0x00=ALL, 0x01=STATUS, 0x02=CELLS, 0x03=PROTECTION, 0x04=SOC
- CONTROL cmd: 0x01=CLEAR_FAULT, 0x10=CHG_ON, 0x11=CHG_OFF, 0x20=DSG_ON, 0x21=DSG_OFF, 0x30=BAL_SET, 0x31=BAL_OFF, 0xFF=SHUTDOWN
- CONFIG param: 0x01=cell_ov_mV, 0x02=cell_uv_mV, 0x03=discharge_oc_mA, 0x04=charge_oc_mA, 0x05=balance_thresh_mV, 0x06=balance_min_mV

### 故障位定义 (12-bit)

| Bit | 名称 | Bit | 名称 | Bit | 名称 |
|-----|------|-----|------|-----|------|
| 0 | 单芯过压 (CELL_OV) | 4 | 放电过流 (DISCHARGE_OC) | 8 | 欠温 (UNDER_TEMP) |
| 1 | 单芯欠压 (CELL_UV) | 5 | 充电过流 (CHARGE_OC) | 9 | 压差过大 (CELL_IMBALANCE) |
| 2 | 总压过压 (PACK_OV) | 6 | 短路 (SHORT_CIRCUIT) | 10 | 通信丢失 (COMM_LOSS) |
| 3 | 总压欠压 (PACK_UV) | 7 | 过温 (OVER_TEMP) | 11 | 看门狗 (WATCHDOG) |

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
Q_PROPERTY(int     connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
Q_PROPERTY(QObject* cellVoltageModel READ cellVoltageModel CONSTANT)
Q_PROPERTY(QObject* temperatureModel READ temperatureModel CONSTANT)
Q_PROPERTY(QObject* faultListModel   READ faultListModel   CONSTANT)
```
