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
    - "model/BmsDataModel.h"
    - "model/BmsDataModel.cpp"
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
- 不替换 BmsDataModel 现有的连接状态逻辑与 `updateBatteryStatus` 派生规则

## 术语表

- **PCAN 硬件层（Hardware）**：PCAN-USB 适配器的物理连接状态，即 Qt `QCanBusDevice::ConnectedState`
- **CAN 总线层（CAN Bus）**：CAN 总线通信活性。设备已连接且未超时（500ms 窗口内有帧到达或刚连接）= active；连接成功但尚未收到首帧时也为 active（超时定时器已启动，首帧未到不判为故障）
- **AFE 通信层（AFE I2C）**：MCU 与 BQ76940 芯片间 I2C 通信状态，三态：未知/等待数据（尚无快照）、在线（afe_online=1）、离线（afe_online=0）
- **健康度（Health）**：各层独立的三色评级——绿色(正常)、黄色(降级/过渡)、红色(故障)、灰色(未知/链路中断导致不可达)
- **级联置灰（Cascade gray-out）**：上层链路中断时，下层状态不可信——强制置灰，仅展示「链路中断」

## 影响面

- `can/CanWorker.h/.cpp`：新增 `reconnectCount` 属性与信号（1 成员 + 1 signal）
- `model/BmsDataModel.h`：新增 `afeOnline()` getter（1 行 getter，复用既有 `m_snap.afe_online`）
- `model/CommunicationMonitor.h/.cpp`：新增文件，通信链路状态聚合器
- `main.cpp`：创建 CommunicationMonitor 实例，注册 QML 上下文属性，连接信号
- `qml/DeviceStatusPage.qml`：新增文件，设备状态 Tab 页
- `qml/main.qml`：TabBar 增加"设备状态"按钮，StackLayout 增加页面
- `CMakeLists.txt`：添加新源文件

## 已确认的关键决策

- **新增 CommunicationMonitor 独立类（方案 1）**：旁路监听 CanWorker/BmsDataModel 信号，内部计算 fps/计数/离线时长等派生指标；源数据属性（硬件连接/CAN 活性/AFE 在线）也由其 relay 到 QML，保持 DeviceStatusPage 单数据源绑定。CanWorker 最小增量（1 个信号），BmsDataModel 最小增量（1 个 getter）。
- **三层健康链可视化**：PCAN 硬件 → CAN 总线 → AFE I2C，层间以连接线衔接，每层绿/黄/红/灰四色 + 脉冲动画
- **独立 Tab 页**：与总览/电芯/曲线平级
- **帧率计算**：1s 滚动窗口平滑，每 1s 仅变化时 emit
- **超时检测区分**：CommunicationMonitor 通过匹配 `errorOccurred("CAN frame timeout...")` 字符串识别真正的帧超时事件，与设备丢失（`deviceConnectedChanged(false)`）分离计数
- **不动固件协议**：所有新增指标纯 Host 端计算

## 行为规范（Requirements）

### Requirement: 系统 SHALL 在独立 Tab 页展示三层通信链路健康状态

新增「设备状态」Tab 页，按 PCAN 硬件 → CAN 总线 → AFE I2C 的层级顺序展示各层健康状态。每层包含：状态指示灯（绿/黄/红/灰四色）、状态文本、关键指标数值。层间以竖连接线衔接，颜色跟随上方层的健康色。综合健康度以 "N/3" 格式显示（N = 当前绿色层数，黄色记 0.5 截断取整）。

#### Scenario: 所有层正常时展示绿色链路

- **GIVEN** PCAN 设备已连接、CAN 总线未超时、AFE 芯片在线
- **WHEN** 用户切换到「设备状态」Tab 页
- **THEN** 三层指示灯均为绿色常亮，层间连接线为绿色，综合健康度显示为 3/3

#### Scenario: 中间层故障时级联置灰

- **GIVEN** PCAN 设备已连接、CAN 总线超时（500ms 无帧）
- **WHEN** CAN 超时事件触发
- **THEN** PCAN 层绿色、CAN 层红色快闪、AFE 层灰色常亮（级联置灰——上层断开则下层不可达），连接线 PCAN→CAN 变红、CAN→AFE 变灰

#### Scenario: Tab 切换不丢失统计状态

- **GIVEN** 设备状态页已展示一段时间，累积了帧率和计数数据
- **WHEN** 用户切换到「总览」Tab 然后再切回「设备状态」Tab
- **THEN** 所有指标数据连续（帧率/计数/离线时长），不发生重置或归零

#### Scenario: 启动时硬件未插入展示等待状态

- **GIVEN** 应用启动但 PCAN 设备未插入
- **WHEN** 用户切换到「设备状态」Tab 页
- **THEN** PCAN 层灰色"等待设备"，CAN 和 AFE 层灰色"等待链路"，重连计数为 0

### Requirement: CommunicationMonitor SHALL 计算并暴露实时 CAN 帧率

CommunicationMonitor 以 1 秒滚动窗口累计接收帧数作为帧率（fps），每秒更新一次，仅值变化时 emit。

#### Scenario: 固件正常上报时帧率稳定在 50fps

- **GIVEN** 固件每 100ms 精确发送 5 帧 CAN 数据（合成输入或仿真环境）
- **WHEN** CommunicationMonitor 运行至少 1 秒
- **THEN** `canFrameRate` 属性值为 50.0（1s 窗口在稳态下精确合计 50 帧）

#### Scenario: CAN 总线静默时帧率降为 0

- **GIVEN** CAN 总线无帧活动超过 1 秒
- **WHEN** 1 秒滚动窗口内的帧计数完全归零
- **THEN** `canFrameRate` 属性值为 0.0

### Requirement: CommunicationMonitor SHALL 追踪 CAN 帧超时事件计数

CommunicationMonitor 监听 CanWorker 的 `errorOccurred` 信号，匹配前缀 "CAN frame timeout" 时判定为一次超时事件，计数 +1。设备丢失、主动 stop 等其他 `connectionStatusChanged(false)` 来源不计入。

#### Scenario: 间歇性超时正确累计

- **GIVEN** CAN 总线连接正常，当前 `timeoutEventCount` 为 N
- **WHEN** CanWorker 连续 3 次发出 `errorOccurred("CAN frame timeout (>500ms)")`
- **THEN** `timeoutEventCount` 属性值为 N+3

#### Scenario: 设备拔除不误计为超时

- **GIVEN** CAN 总线连接正常，当前 `timeoutEventCount` 为 N
- **WHEN** 用户拔出 PCAN 设备（触发 `deviceConnectedChanged(false)` 和 `connectionStatusChanged(false)`，但不发 "CAN frame timeout" 错误）
- **THEN** `timeoutEventCount` 属性值仍为 N（不增加）

### Requirement: CommunicationMonitor SHALL 追踪 PCAN 设备丢失事件计数

每次 CanWorker 检测到设备物理丢失（`handleDeviceLost` 触发 `deviceConnectedChanged(false)`）时，CommunicationMonitor 将重连计数 +1。初始连接失败和定时器驱动的重试不计入——一次拔插只计一次丢失事件。

#### Scenario: 设备拔插后丢失计数递增一次

- **GIVEN** PCAN 设备正常连接，`reconnectCount` 为 N
- **WHEN** 用户拔出 PCAN 设备（CanWorker 调用 `handleDeviceLost` 一次）
- **THEN** `reconnectCount` 为 N+1（不会因后续自动重连尝试继续递增）

### Requirement: CommunicationMonitor SHALL 追踪 AFE 在线状态与离线时长

监听 `BmsDataModel::afeOnline` 属性变化。AFE 从在线变为离线时记录时间戳并开始每秒递增 `afeOfflineSeconds`；恢复在线时归零。在首个快照到达前，AFE 状态为「未知」（尚未收到 AFE 数据）。

#### Scenario: AFE 离线后恢复

- **GIVEN** AFE 芯片在线，`afeOfflineSeconds` 为 0
- **WHEN** AFE 离线持续 5 秒后恢复在线
- **THEN** 离线期间 `afeOfflineSeconds` 从 0 递增至约 5，恢复后归零

#### Scenario: 启动后首个快照到达前 AFE 状态为未知

- **GIVEN** 应用刚启动，尚无任何 BMS 快照
- **WHEN** 用户查看 AFE 层状态
- **THEN** AFE 指示灯为灰色"等待数据"，`afeOnline` 为 false，`afeOfflineSeconds` 为 0

### Requirement: 状态指示灯 SHALL 根据健康度呈现不同颜色和动画

- **绿色常亮**：该层完全正常
- **黄色慢闪**（opacity 0→1→0，2 秒周期）：过渡/降级。CAN 层从超时恢复到首帧到达后保持黄色 3 秒再转绿；AFE 层数据等待中
- **红色快闪**（opacity 0→1→0，0.5 秒周期）：故障。超时、设备丢失、AFE 离线
- **灰色常亮**：未知状态或级联置灰（上层中断导致下层不可达）

#### Scenario: CAN 帧超时时显示红色快闪

- **GIVEN** CAN 总线帧活动正常，指示灯为绿色常亮
- **WHEN** CanWorker 检测到 500ms 帧超时并发出 `errorOccurred("CAN frame timeout...")`
- **THEN** CAN 层指示灯立即变为红色快闪动画

#### Scenario: CAN 超时恢复后黄色过渡再回绿

- **GIVEN** CAN 层指示灯红色快闪（超时中）
- **WHEN** 首帧 CAN 数据到达（`batchReady` 信号重新触发，`connectionStatusChanged(true)`）
- **THEN** 指示灯变为黄色慢闪，持续 3 秒后若帧率持续 >0 则自动转为绿色常亮

### Requirement: CommunicationMonitor SHALL 暴露累计总帧数

`totalFrameCount` 自 CommunicationMonitor 创建起单调递增，每次 `batchReady` 时累加批大小。不随重连或超时重置。

#### Scenario: 帧数持续累加

- **GIVEN** CommunicationMonitor 运行中，`totalFrameCount` 为 M
- **WHEN** 收到一批 5 帧的 `batchReady`
- **THEN** `totalFrameCount` 为 M+5

## 方案设计

### 架构与组件

```
CanWorker (已有, 工作线程)                    BmsDataModel (已有, 主线程)
  │  → batchReady(QVector<CanFrame>)           │  + afeOnline() getter (新增1行)
  │  → deviceConnectedChanged(bool)            │  → batteryStatusTextChanged (已有)
  │  → errorOccurred(QString)                  │
  │  + reconnectCountChanged(int) [新增]        │
  ▼                                            ▼
         CommunicationMonitor (新增, 主线程)
           输入: CanWorker 信号 + BmsDataModel 属性
           计算: fps(1s滚动窗口), 超时计数, 重连计数, 离线时长, 综合健康度
           输出: Q_PROPERTY ×10
           初始化: 全部默认值(false/0), 首个信号到达时修正
           ▼
         QML: DeviceStatusPage.qml (新增)
           └─ 仅绑定 commMonitor 上下文属性
```

**CommunicationMonitor**：
- **是什么**：通信链路状态聚合器 QObject。旁路监听 CanWorker 和 BmsDataModel，不做 I/O、不持有设备
- **怎么用**：main.cpp 创建、连线、注册为 QML 上下文属性 `commMonitor`
- **依赖**：CanWorker（已有信号 + 新增 `reconnectCountChanged`）、BmsDataModel（新增 `afeOnline` getter + 已有 `batteryStatusTextChanged` 信号）

**CanWorker 增量**（最小）：
- `int m_reconnectCount` 成员，`handleDeviceLost()` 中递增（仅设备物理丢失，不含初始连接失败）
- `reconnectCountChanged(int)` 信号
- 注：`m_reconnectAttempt`（既有）保持原有逻辑不变——`m_reconnectCount` 是独立的新成员，语义不同

**BmsDataModel 增量**（最小）：
- `bool afeOnline() const { return m_snap.afe_online; }` — 1 行 getter，数据已存在

### 数据流

```
[硬件中断] → CanWorker::onFramesReceived()
  → batchReady(batch)
    → CommunicationMonitor::onFrames(batch.size())      [帧计数累加 + 帧率窗口压入]

[500ms Timer] → CanWorker::checkTimeout()
  → errorOccurred("CAN frame timeout (>500ms)")
    → CommunicationMonitor::onError(msg)
      匹配 "CAN frame timeout" 前缀 → timeoutEventCount++       [超时计数]
    → connectionStatusChanged(false)
      → canBusActive = false, 颜色红色                          [总线活性]

[设备丢失] → CanWorker::handleDeviceLost()
  → m_reconnectCount++, emit reconnectCountChanged(m_reconnectCount)
    → CommunicationMonitor::onReconnectCountChanged()           [重连计数]
  → deviceConnectedChanged(false)
    → hardwareConnected = false, 触发级联置灰                    [硬件状态]

[设备恢复] → CanWorker 重连成功
  → deviceConnectedChanged(true)
    → hardwareConnected = true                                  [硬件状态]
  → connectionStatusChanged(true)
    → canBusActive = true, 颜色黄色过渡                          [总线活性]

[BMS 快照] → BmsDataModel::applySnapshot() 内部更新 m_snap.afe_online
  → batteryStatusTextChanged
    → CommunicationMonitor 轮询 bmsModel->afeOnline()
      检测到 true→false 变迁 → 记录离线时间戳, 启动每秒计时器        [AFE离线计时]
      检测到 false→true 变迁 → afeOfflineSeconds 归零, AFE 在线    [AFE恢复]
      首个快照前: haveAfeInfo=false → AFE 灰色"等待数据"
```

### 关键接口

**CommunicationMonitor 公共接口**：

```cpp
class CommunicationMonitor : public QObject {
    Q_OBJECT
    // 源数据 relay（来自 CanWorker / BmsDataModel）
    Q_PROPERTY(bool hardwareConnected READ hardwareConnected
               NOTIFY hardwareConnectedChanged)
    Q_PROPERTY(bool canBusActive READ canBusActive
               NOTIFY canBusActiveChanged)
    Q_PROPERTY(bool afeOnline READ afeOnline
               NOTIFY afeOnlineChanged)
    Q_PROPERTY(QString lastError READ lastError
               NOTIFY lastErrorChanged)
    // 派生指标（CommunicationMonitor 内部计算）
    Q_PROPERTY(int reconnectCount READ reconnectCount
               NOTIFY reconnectCountChanged)
    Q_PROPERTY(qreal canFrameRate READ canFrameRate
               NOTIFY canFrameRateChanged)
    Q_PROPERTY(quint64 totalFrameCount READ totalFrameCount
               NOTIFY totalFrameCountChanged)
    Q_PROPERTY(quint32 timeoutEventCount READ timeoutEventCount
               NOTIFY timeoutEventCountChanged)
    Q_PROPERTY(int afeOfflineSeconds READ afeOfflineSeconds
               NOTIFY afeOfflineSecondsChanged)
    Q_PROPERTY(int healthSummary READ healthSummary
               NOTIFY healthSummaryChanged)

public:
    void setCanWorker(CanWorker *w);
    void setBmsModel(BmsDataModel *m);  // 读取 afeOnline()

private slots:
    void onFrames(qsizetype count);
    void onHardwareStateChanged(bool connected);
    void onBusActivityChanged(bool active);
    void onReconnectCountChanged(int count);
    void onError(const QString &msg);
    void onBatteryStatusChanged();       // 轮询 afeOnline
    void tickOfflineDuration();          // 每秒递增 afeOfflineSeconds

private:
    void updateHealthSummary();
    void applyCascadeGrayOut();

    // 源数据
    bool m_hardwareConnected = false;
    bool m_canBusActive = false;
    bool m_afeOnline = false;
    bool m_haveAfeInfo = false;          // 是否收到过首个快照
    QString m_lastError;

    // 派生指标
    int m_reconnectCount = 0;
    qreal m_canFrameRate = 0.0;
    quint64 m_totalFrameCount = 0;
    quint32 m_timeoutEventCount = 0;
    int m_afeOfflineSeconds = 0;
    int m_healthSummary = 0;

    // 内部状态
    QVector<qsizetype> m_frameWindow;    // 1s 滚动窗口
    QDateTime m_afeDropTime;
    QTimer *m_fpsTimer;
    QTimer *m_offlineTimer;
    CanWorker *m_canWorker = nullptr;
    BmsDataModel *m_bmsModel = nullptr;
};
```

**CanWorker 增量**：

```cpp
// CanWorker.h 新增:
signals:
    void reconnectCountChanged(int count);
private:
    int m_reconnectCount = 0;    // handleDeviceLost() 中递增
```

**BmsDataModel 增量**：

```cpp
// BmsDataModel.h public 区新增:
bool afeOnline() const { return m_snap.afe_online; }
```

### 错误处理

| 场景 | 行为 |
|------|------|
| 启动时 CAN 硬件未插入 | PCAN 层灰色"等待设备"，CAN/AFE 层灰色"等待链路"，reconnectCount=0 |
| CAN 帧超时 (500ms) | CAN 层立即红色快闪，帧率降为 0，超时计数 +1 |
| 超时恢复（首帧到达） | CAN 层黄色慢闪 3 秒过渡 → 帧率持续 >0 → 转绿色常亮 |
| PCAN 设备物理拔除 | PCAN 层红色快闪"设备丢失"，CAN/AFE 级联置灰，reconnectCount +1 |
| PCAN 设备重新插入并恢复 | PCAN 层绿色常亮，CAN 层黄色慢闪 3 秒过渡 |
| AFE I2C 连续失败 | AFE 层红色快闪"芯片离线"，开始累计离线时长 |
| AFE 离线恢复 | AFE 层绿色常亮，离线时长归零 |
| 启动后首个快照到达前 | AFE 层灰色"等待数据"（未知状态，非故障） |
| CommunicationMonitor 构造时 CanWorker 已在线 | 默认值初始化（false/0），首个信号到达时自然修正——无竞态问题 |
| 帧率瞬时跳变（批到达） | 1s 滚动窗口平滑 |

## 测试与验收策略

| Scenario / 检查项 | 维度 | 执行方式 | 验收证据 |
|-------------------|------|---------|---------|
| 所有层正常时展示绿色链路 | integration | 任务内 TDD | 测试通过 |
| 中间层故障时级联置灰 | integration | 任务内 TDD | 测试通过 |
| Tab 切换不丢失统计状态 | integration | 任务内 TDD | 测试通过 |
| 启动时硬件未插入展示等待状态 | integration | 任务内 TDD | 测试通过 |
| 固件正常上报时帧率稳定在 50fps | unit | 任务内 TDD | 测试通过 |
| CAN 总线静默时帧率降为 0 | unit | 任务内 TDD | 测试通过 |
| 间歇性超时正确累计 | unit | 任务内 TDD | 测试通过 |
| 设备拔除不误计为超时 | unit | 任务内 TDD | 测试通过 |
| 设备拔插后丢失计数递增一次 | integration | 任务内 TDD | 测试通过 |
| AFE 离线后恢复 | unit | 任务内 TDD | 测试通过 |
| 启动后首个快照到达前 AFE 状态为未知 | unit | 任务内 TDD | 测试通过 |
| CAN 帧超时时显示红色快闪 | integration | 验收任务 (D) | 手动验证 |
| CAN 超时恢复后黄色过渡再回绿 | integration | 验收任务 (D) | 手动验证 |
| 帧数持续累加不重置 | unit | 任务内 TDD | 测试通过 |

## 风险与边缘情况

- **帧率初始值**：启动时无帧 → fps=0.0，不触发异常
- **帧率显示波动**：CAN 帧以 100ms 批量到达（一次 5 帧），未平滑时帧率在 0→50 间抖动 → 1s 滚动窗口保证稳定性
- **属性更新频率**：帧率仅每 1s 重新计算且仅变化时 emit → QML 不存在高频刷新
- **固件侧通信丢失检测滞后**：`afe_online` 仅当固件 sample 任务成功采集时才翻为 0（I2C 连续失败 3 次），3 次采样窗口内 AFE 层可能仍显示在线 → 不影响设计，这是协议既有限制
- **重连计数不反映"是否正在重连"**：重连中状态由 `hardwareConnected=false` + `canBusActive=false` 组合表达，页面据此显示等待状态
- **级联置灰与陈旧值的交互**：CAN 层超时 → AFE 层灰色，此时 `afeOnline` 可能是上一次快照的陈旧 true。灰色覆盖显示优先级最高——级联置灰时不读取也不展示下层的 stale 值

## 开放问题

- 无
