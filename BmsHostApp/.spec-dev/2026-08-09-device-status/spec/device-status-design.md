---
spec_dev:
  version: 1
  feature: device-status
  status: draft
  covers:
    - "can/CanWorker.h"
    - "can/CanWorker.cpp"
    - "model/CommunicationMonitor.h"
    - "model/CommunicationMonitor.cpp"
    - "main.cpp"
    - "qml/DeviceStatusPage.qml"
    - "qml/main.qml"
    - "CMakeLists.txt"
  sync_commit: null
---

# 设备通信状态监控 设计

## 背景与目标

BMS 上位机当前仅在底部状态栏展示硬件连接/电池接入的二元状态（连接/断开），缺乏对通信链路中间态和动态指标的展示。当 CAN 帧间歇性超时、AFE 芯片偶发离线时，用户无法直观感知链路健康度的退化过程。

**成功标准 / Success criteria**：新增「设备状态」Tab 页，以三层健康链（PCAN 硬件 → CAN 总线 → AFE I2C）可视化通信链路的实时健康状态，含动态帧率、超时计数和脉冲动画，不改固件协议。

## 非目标

- 不修改固件协议或新增 CAN 帧
- 不改变现有底部状态栏（保持作为全页面 at-a-glance 参考）
- 不添加历史趋势图/日志存储（后续独立需求）
- 不替换 BmsDataModel 现有的连接状态逻辑

## 术语表

- **PCAN 硬件层（Hardware）**：PCAN-USB 适配器的物理连接状态，由操作系统和 Qt SerialBus 管理
- **CAN 总线层（CAN Bus）**：CAN 总线帧活动，以是否有帧到达判断活性，500ms 无帧超时
- **AFE 通信层（AFE I2C）**：MCU 与 BQ76940 芯片间的 I2C 通信，由固件通过 `afe_online`（0x120 ctrl byte bit5）上报
- **健康度（Health）**：三层各自的三色健康评级——绿色(正常)、黄色(降级/过渡)、红色(故障)

## 影响面

- `can/CanWorker.h/.cpp`：新增 `reconnectCount` 属性和信号（最小增量，1 个成员 + 1 个 signal）
- `model/CommunicationMonitor.h/.cpp`：新增文件，通信链路状态聚合器
- `main.cpp`：创建 CommunicationMonitor 实例，注册 QML 上下文属性，连接信号
- `qml/DeviceStatusPage.qml`：新增文件，设备状态 Tab 页
- `qml/main.qml`：TabBar 增加"设备状态"按钮，StackLayout 增加页面
- `CMakeLists.txt`：添加新源文件

## 已确认的关键决策

- **新增 CommunicationMonitor 独立类（方案 1）**：旁路监听 CanWorker/BmsDataModel 信号，内部计算所有链路指标 —— CanWorker 仅做最小增量暴露重连计数，BmsDataModel 不变。关注点分离，独立可测。
- **三层健康链可视化**：PCAN 硬件 → CAN 总线 → AFE I2C，层间以连接线衔接，每层绿色(正常)/黄色(降级)/红色(故障)+ 脉冲动画。
- **独立 Tab 页**：与总览/电芯/曲线平级，空间充足可完整展示指标。
- **帧率计算在 CommunicationMonitor 内部**：1s 滚动窗口平滑，避免批到达导致的瞬时跳变。
- **不动固件协议**：所有新增指标纯 Host 端计算，零固件改动。

## 行为规范（Requirements）

### Requirement: 系统 SHALL 在独立 Tab 页展示三层通信链路健康状态

新增「设备状态」Tab 页，按 PCAN 硬件 → CAN 总线 → AFE I2C 的层级顺序展示各层健康状态，每层包含状态指示灯和关键指标。

#### Scenario: 所有层正常时展示绿色链路

- **GIVEN** PCAN 设备已连接、CAN 总线有帧活动（帧率 >0）、AFE 芯片在线
- **WHEN** 用户切换到「设备状态」Tab 页
- **THEN** 三层指示灯均为绿色常亮，层间连接线为绿色，综合健康度显示为 3/3

#### Scenario: 中间层故障时展示断裂链路

- **GIVEN** PCAN 设备已连接、CAN 总线超时（500ms 无帧）、AFE 状态未知
- **WHEN** CAN 总线超时事件触发
- **THEN** PCAN 层绿色、CAN 层红色快闪、AFE 层灰色（链路中断），层间连接线 PCAN→CAN 变红

#### Scenario: Tab 切换不丢失状态

- **GIVEN** 设备状态页已展示一段时间，累积了帧率和计数数据
- **WHEN** 用户切换到「总览」Tab 然后再切回「设备状态」Tab
- **THEN** 所有指标数据连续（帧率/计数/离线时长），不发生重置

### Requirement: CommunicationMonitor SHALL 计算并暴露实时 CAN 帧率

CommunicationMonitor 以 1 秒滚动窗口计算帧率（fps），仅当值变化时通过属性通知 QML。

#### Scenario: 固件正常上报时帧率稳定在约 50fps

- **GIVEN** 固件正常、每 100ms 发送 5 帧 CAN 数据
- **WHEN** CommunicationMonitor 运行至少 1 秒
- **THEN** `canFrameRate` 属性值稳定在 45-55 范围内

#### Scenario: CAN 总线静默时帧率降为 0

- **GIVEN** CAN 总线无帧活动超过 1 秒
- **WHEN** 1 秒滚动窗口内的帧计数归零
- **THEN** `canFrameRate` 属性值为 0.0

### Requirement: CommunicationMonitor SHALL 追踪 CAN 超时事件计数

每次 CanWorker 的 500ms 超时检测触发时，CommunicationMonitor 将超时事件计数 +1。

#### Scenario: 间歇性超时正确累计

- **GIVEN** CAN 总线连接正常
- **WHEN** 连续发生 3 次 500ms 帧超时（超时-恢复-超时-恢复-超时-恢复）
- **THEN** `timeoutEventCount` 属性值为 3

### Requirement: CommunicationMonitor SHALL 追踪 PCAN 硬件重连次数

每次 CanWorker 因设备丢失触发重连尝试时，CommunicationMonitor 将重连计数 +1。

#### Scenario: 设备拔插后重连计数递增

- **GIVEN** PCAN 设备正常连接，`reconnectCount` 为 0
- **WHEN** 用户拔出 PCAN 设备，CanWorker 检测到设备丢失并开始重连
- **THEN** `reconnectCount` 为 1（每次自动重连尝试触发一次递增）

### Requirement: CommunicationMonitor SHALL 追踪 AFE 离线时长

当检测到 `afe_online` 从 true 变为 false 时开始计时；恢复时归零。离线时长以秒为单位递增。

#### Scenario: AFE 离线后恢复

- **GIVEN** AFE 芯片在线，`afeOfflineSeconds` 为 0
- **WHEN** AFE 离线持续 5 秒后恢复在线
- **THEN** 离线期间 `afeOfflineSeconds` 从 0 递增至约 5，恢复后归零

### Requirement: 状态指示灯 SHALL 根据健康度呈现不同颜色和动画

- 绿色常亮：该层完全正常
- 黄色慢闪（opacity 2s 周期呼吸）：降级/过渡状态（如 CAN 总线刚从超时恢复、AFE 数据等待中）
- 红色快闪（opacity 0.5s 周期呼吸）：故障状态（如设备丢失、AFE 离线）
- 灰色常亮：未知/等待（如上层链路未就绪导致下层不可达）

#### Scenario: CAN 帧超时时显示红色快闪

- **GIVEN** CAN 总线帧活动正常，指示灯为绿色常亮
- **WHEN** CanWorker 检测到 500ms 帧超时
- **THEN** CAN 层指示灯变为红色快闪动画（0.5s 周期呼吸）

## 方案设计

### 架构与组件

```
CanWorker (已有, 工作线程)
  │  → batchReady(QVector<CanFrame>)
  │  → connectionStatusChanged(bool)
  │  → deviceConnectedChanged(bool)
  │  → errorOccurred(QString)
  │  → reconnectCountChanged(int)        [新增]
  ▼
CommunicationMonitor (新增, 主线程)
  │  输入: CanWorker 信号 + BmsDataModel::onAfeChanged
  │  计算: fps (1s滚动窗口)、超时计数、重连计数、离线时长
  │  输出: Q_PROPERTY ×10
  ▼
QML: DeviceStatusPage.qml (新增)
  └─ 绑定 commMonitor 属性
```

**CommunicationMonitor**：
- **是什么**：通信链路状态聚合器 QObject。旁路监听 CanWorker 和 BmsDataModel，不做 I/O、不持有设备。
- **怎么用**：由 main.cpp 创建、连线、注册为 QML 上下文属性 `commMonitor`。
- **依赖**：CanWorker（已有信号）、BmsDataModel（读取 afe_online 状态变更）。

**CanWorker 增量**：
- 新增 `int m_reconnectCount` 成员，`tryReconnect()` 中每次尝试递增
- 新增 `reconnectCountChanged(int)` 信号

### 数据流

```
[硬件中断] → CanWorker::onFramesReceived()
  → batchReady(batch) → BmsDataModel::onBatchReady (已有)
                      → CommunicationMonitor::onFrames(batch.size()) (新增, 帧计数+帧率)

[500ms Timer] → CanWorker::checkTimeout()
  → connectionStatusChanged(false) → CommunicationMonitor::onBusTimeout() (新增, 超时计数+1)

[设备状态变化] → CanWorker::handleDeviceLost() / tryReconnect()
  → deviceConnectedChanged(bool)
  → reconnectCountChanged(int) → CommunicationMonitor::onReconnect() (新增)

[BMS 快照] → BmsDataModel::applySnapshot()
  → updateBatteryStatus(..., afe_online)
  → CommunicationMonitor::onAfeChanged(bool) (新增)
```

### 关键接口

**CommunicationMonitor 公共接口**：

```cpp
class CommunicationMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hardwareConnected READ hardwareConnected NOTIFY hardwareConnectedChanged)
    Q_PROPERTY(int reconnectCount READ reconnectCount NOTIFY reconnectCountChanged)
    Q_PROPERTY(qreal canFrameRate READ canFrameRate NOTIFY canFrameRateChanged)
    Q_PROPERTY(bool canBusActive READ canBusActive NOTIFY canBusActiveChanged)
    Q_PROPERTY(quint64 totalFrameCount READ totalFrameCount NOTIFY totalFrameCountChanged)
    Q_PROPERTY(quint32 timeoutEventCount READ timeoutEventCount NOTIFY timeoutEventCountChanged)
    Q_PROPERTY(bool afeOnline READ afeOnline NOTIFY afeOnlineChanged)
    Q_PROPERTY(int afeOfflineSeconds READ afeOfflineSeconds NOTIFY afeOfflineSecondsChanged)
    Q_PROPERTY(int healthSummary READ healthSummary NOTIFY healthSummaryChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    void setCanWorker(CanWorker *w);   // 连接信号, 初始化基线
    void setBmsModel(BmsDataModel *m); // 监听 AFE 状态变更

    // 内部槽
    void onFrames(qsizetype count);
    void onBusActivityChanged(bool active);
    void onBusTimeout();
    void onHardwareStateChanged(bool connected);
    void onReconnectCountChanged(int count);
    void onAfeOnlineChanged(bool online);
    void onError(const QString &msg);
};
```

**CanWorker 增量**：

```cpp
// CanWorker.h 新增:
Q_PROPERTY(int reconnectCount READ reconnectCount NOTIFY reconnectCountChanged)
signals:
    void reconnectCountChanged(int count);
private:
    int m_reconnectCount = 0;
```

### 错误处理

| 场景 | 行为 |
|------|------|
| 启动时 CAN 硬件未插入 | PCAN 层灰色"等待设备"，CAN/AFE 层灰色"等待链路" |
| CAN 帧超时 (500ms) | CAN 层黄→红，帧率降为 0，超时计数 +1 |
| AFE I2C 连续失败 | AFE 层变红"芯片离线"，开始计时离线时长 |
| PCAN 设备物理拔除 | PCAN 层变红"设备丢失"，CAN/AFE 变灰，重连计数递增 |
| CommunicationMonitor 构造时 CanWorker 已在线 | 从 CanWorker 当前状态初始化基线（不重播历史） |
| 帧率瞬时跳变（批到达） | 1s 滚动窗口平滑，不额外处理 |

## 测试与验收策略

| Scenario / 检查项 | 维度 | 执行方式 | 验收证据 |
|-------------------|------|---------|---------|
| 所有层正常时展示绿色链路 | integration | 任务内 TDD | 测试通过 |
| 中间层故障时展示断裂链路 | integration | 任务内 TDD | 测试通过 |
| Tab 切换不丢失状态 | e2e | 验收任务 (D) | 手动验证 |
| 固件正常上报时帧率稳定在约 50fps | unit | 任务内 TDD | 测试通过 |
| CAN 总线静默时帧率降为 0 | unit | 任务内 TDD | 测试通过 |
| 间歇性超时正确累计 | unit | 任务内 TDD | 测试通过 |
| 设备拔插后重连计数递增 | integration | 任务内 TDD | 测试通过 |
| AFE 离线后恢复 | unit | 任务内 TDD | 测试通过 |
| CAN 帧超时时显示红色快闪 | integration | 验收任务 (D) | 手动验证 |
| 状态指示灯颜色与动画正确 | visual | 验收任务 (D) | 目视确认 |

## 风险与边缘情况

- **初始数据竞态**：CanWorker 可能在 CommunicationMonitor 连线前已发出信号 → 构造函数中用 CanWorker 当前状态查询初始化基线
- **帧率显示波动**：CAN 帧以 100ms 批量到达（一次 5 帧），未平滑时帧率在 0→50 间抖动 → 1s 滚动窗口保证稳定性
- **属性更新频率**：帧率仅每 1s 重新计算且仅变化时 emit → QML 不存在高频刷新风险
- **固件侧通信丢失检测滞后**：`afe_online` 仅当固件 sample 任务成功采集时才更新，I2C 连续失败 3 次前 AFE 层可能误判为在线 → 不影响设计，这是协议既有限制

## 开放问题

- 无
