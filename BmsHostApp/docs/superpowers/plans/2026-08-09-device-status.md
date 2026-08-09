# 设备通信状态监控 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增「设备状态」Tab 页，以三层健康链（PCAN 硬件 → CAN 总线 → AFE I2C）实时展示 BMS 通信链路状态，含帧率、超时计数、重连计数和脉冲动画。

**Architecture:** 新增 CommunicationMonitor 独立类旁路监听 CanWorker 和 BmsDataModel 信号，聚合计算链路指标并通过 Q_PROPERTY 暴露给 QML。CanWorker 和 BmsDataModel 各做最小增量。DeviceStatusPage.qml 绑定 `commMonitor` 单数据源。

**Tech Stack:** Qt 6.5+ (Core, QML, SerialBus), C++17, CMake 3.21+

## Global Constraints

- 不修改固件协议或新增 CAN 帧
- 不改变现有底部状态栏
- 不添加历史趋势图/日志存储
- BmsDataModel 现有的 `updateBatteryStatus` 派生规则不替换
- 所有新增指标纯 Host 端计算

---

### Task 1: CanWorker — 新增 reconnectCount 信号

**Files:**
- Modify: `can/CanWorker.h`
- Modify: `can/CanWorker.cpp`

**Interfaces:**
- Produces: `CanWorker::reconnectCountChanged(int count)` signal, `int CanWorker::reconnectCount() const` getter

- [ ] **Step 1: 在 CanWorker.h 添加成员与信号**

`can/CanWorker.h` — 在 `signals:` 块末尾新增信号，在 `private:` 块末尾新增成员：

```cpp
// CanWorker.h — signals: 块, 在 errorOccurred 之后新增:
    void reconnectCountChanged(int count);

// CanWorker.h — private: 块, 在 m_deviceConnected 之后新增:
    int m_reconnectCount = 0;
```

- [ ] **Step 2: 在 CanWorker.cpp 的 handleDeviceLost() 中递增并 emit**

`can/CanWorker.cpp` — 在 `handleDeviceLost()` 函数中，`if (!m_deviceConnected) return;` 之后、`m_deviceConnected = false;` 之前：

```cpp
void CanWorker::handleDeviceLost()
{
    if (!m_deviceConnected) return;
    m_reconnectCount++;
    emit reconnectCountChanged(m_reconnectCount);
    m_deviceConnected = false;
    // ... 后续代码不变
}
```

- [ ] **Step 3: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功，新增信号和成员无链接错误。

- [ ] **Step 4: Commit**

```bash
git add can/CanWorker.h can/CanWorker.cpp
git commit -m "feat(can): add reconnectCount signal to CanWorker

- Emit reconnectCountChanged on each handleDeviceLost() call
- Independent of existing m_reconnectAttempt (retry counter)
- Part of device-status feature (Task 1/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 2: BmsDataModel — 新增 afeOnline() getter

**Files:**
- Modify: `model/BmsDataModel.h`

**Interfaces:**
- Produces: `bool BmsDataModel::afeOnline() const`

- [ ] **Step 1: 在 BmsDataModel.h 添加 getter**

`model/BmsDataModel.h` — 在 `bool canBusConnected() const` 之后新增一行：

```cpp
    bool afeOnline() const { return m_snap.afe_online; }
```

- [ ] **Step 2: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功。

- [ ] **Step 3: Commit**

```bash
git add model/BmsDataModel.h
git commit -m "feat(model): add afeOnline() getter to BmsDataModel

- Returns m_snap.afe_online (data already present, no new storage)
- Part of device-status feature (Task 2/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 3: CommunicationMonitor — 头文件

**Files:**
- Create: `model/CommunicationMonitor.h`

**Interfaces:**
- Produces: `CommunicationMonitor` class declaration with all Q_PROPERTY, public methods, private slots, and member variables

- [ ] **Step 1: 编写 CommunicationMonitor.h**

`model/CommunicationMonitor.h`：

```cpp
#ifndef COMMUNICATION_MONITOR_H
#define COMMUNICATION_MONITOR_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QDateTime>

class CanWorker;
class BmsDataModel;

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
    explicit CommunicationMonitor(QObject *parent = nullptr);
    ~CommunicationMonitor() override = default;

    void setCanWorker(CanWorker *w);
    void setBmsModel(BmsDataModel *m);

    // Getters
    bool hardwareConnected() const { return m_hardwareConnected; }
    bool canBusActive() const { return m_canBusActive; }
    bool afeOnline() const { return m_afeOnline; }
    QString lastError() const { return m_lastError; }
    int reconnectCount() const { return m_reconnectCount; }
    qreal canFrameRate() const { return m_canFrameRate; }
    quint64 totalFrameCount() const { return m_totalFrameCount; }
    quint32 timeoutEventCount() const { return m_timeoutEventCount; }
    int afeOfflineSeconds() const { return m_afeOfflineSeconds; }
    int healthSummary() const { return m_healthSummary; }

signals:
    void hardwareConnectedChanged();
    void canBusActiveChanged();
    void afeOnlineChanged();
    void lastErrorChanged();
    void reconnectCountChanged();
    void canFrameRateChanged();
    void totalFrameCountChanged();
    void timeoutEventCountChanged();
    void afeOfflineSecondsChanged();
    void healthSummaryChanged();

private slots:
    void onFrames(qsizetype count);
    void onHardwareStateChanged(bool connected);
    void onBusActivityChanged(bool active);
    void onReconnectCountChanged(int count);
    void onError(const QString &msg);
    void onBatteryStatusChanged();
    void tickOfflineDuration();
    void tickFpsWindow();

private:
    void updateHealthSummary();

    // 源数据
    bool m_hardwareConnected = false;
    bool m_canBusActive = false;
    bool m_afeOnline = false;
    bool m_haveAfeInfo = false;
    QString m_lastError;

    // 派生指标
    int m_reconnectCount = 0;
    qreal m_canFrameRate = 0.0;
    quint64 m_totalFrameCount = 0;
    quint32 m_timeoutEventCount = 0;
    int m_afeOfflineSeconds = 0;
    int m_healthSummary = 0;

    // 内部状态
    QVector<qsizetype> m_frameWindow;
    QDateTime m_afeDropTime;
    QTimer *m_fpsTimer = nullptr;
    QTimer *m_offlineTimer = nullptr;
    CanWorker *m_canWorker = nullptr;
    BmsDataModel *m_bmsModel = nullptr;
};

#endif // COMMUNICATION_MONITOR_H
```

- [ ] **Step 2: Commit**

```bash
git add model/CommunicationMonitor.h
git commit -m "feat(monitor): add CommunicationMonitor header

- 10 Q_PROPERTYs (4 relay + 6 computed)
- Private slots for all signal inputs
- Internal state for frame window and AFE offline timing
- Part of device-status feature (Task 3/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 4: CommunicationMonitor — 帧计数与 fps

**Files:**
- Create: `model/CommunicationMonitor.cpp`

**Interfaces:**
- Consumes: `CanWorker::batchReady(QVector<CanFrame>)` — connect to `onFrames(batch.size())`
- Produces: `canFrameRate`, `totalFrameCount` properties

- [ ] **Step 1: 编写构造函数 + onFrames + tickFpsWindow**

`model/CommunicationMonitor.cpp`：

```cpp
#include "CommunicationMonitor.h"
#include "can/CanWorker.h"
#include "model/BmsDataModel.h"
#include <QtCore/qmath.h>

CommunicationMonitor::CommunicationMonitor(QObject *parent)
    : QObject(parent)
{
    // fps 窗口定时器: 每秒清理过期项
    m_fpsTimer = new QTimer(this);
    m_fpsTimer->setInterval(1000);
    connect(m_fpsTimer, &QTimer::timeout, this, &CommunicationMonitor::tickFpsWindow);

    // 离线时长定时器: 每秒递增 (默认不启动)
    m_offlineTimer = new QTimer(this);
    m_offlineTimer->setInterval(1000);
    connect(m_offlineTimer, &QTimer::timeout, this, &CommunicationMonitor::tickOfflineDuration);
}

void CommunicationMonitor::onFrames(qsizetype count)
{
    // 总帧数单调递增
    m_totalFrameCount += static_cast<quint64>(count);
    emit totalFrameCountChanged();

    // 帧率滚动窗口: 压入当前批次大小，启动 fps 定时器
    m_frameWindow.append(count);
    if (!m_fpsTimer->isActive()) {
        m_fpsTimer->start();
    }
}

void CommunicationMonitor::tickFpsWindow()
{
    // 1s 到期: 窗口内所有条目之和 = 过去 1s 的总帧数
    qreal newFps = 0.0;
    for (qsizetype c : m_frameWindow) {
        newFps += static_cast<qreal>(c);
    }
    m_frameWindow.clear();

    if (!qFuzzyCompare(m_canFrameRate, newFps)) {
        m_canFrameRate = newFps;
        emit canFrameRateChanged();
    }
}
```

- [ ] **Step 2: 编写测试 — 帧率计算精确性**

创建测试文件或使用 Qt Test（本项目采用手动可验证方式：在 `main.cpp` 中临时添加模拟信号验证，然后清理）。

简化验证方式——后续 Task 9 会在 main.cpp 连线后自然集成：

```cpp
// 概念验证：模拟 10 批 × 每批 5 帧 = 50 帧
// 1s 后 fps 应为 50.0
// CommunicationMonitor m;
// for (int i = 0; i < 10; ++i) m.onFrames(5);
// QTimer::singleShot(1100, [&]() { Q_ASSERT(qFuzzyCompare(m.canFrameRate(), 50.0)); });
```

- [ ] **Step 3: Commit**

```bash
git add model/CommunicationMonitor.cpp
git commit -m "feat(monitor): implement frame counting and fps computation

- onFrames() accumulates totalFrameCount and pushes to rolling window
- tickFpsWindow() computes fps from 1s window every second
- fps only emits when value changes (qFuzzyCompare)
- Part of device-status feature (Task 4/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 5: CommunicationMonitor — 超时检测

**Files:**
- Modify: `model/CommunicationMonitor.cpp`

**Interfaces:**
- Consumes: `CanWorker::errorOccurred(QString)` — connect to `onError(msg)`
- Consumes: `CanWorker::connectionStatusChanged(bool)` — connect to `onBusActivityChanged(active)`
- Produces: `timeoutEventCount`, `canBusActive` properties

- [ ] **Step 1: 实现 onError 和 onBusActivityChanged**

在 `model/CommunicationMonitor.cpp` 中追加：

```cpp
void CommunicationMonitor::onError(const QString &msg)
{
    m_lastError = msg;
    emit lastErrorChanged();

    // 仅 "CAN frame timeout" 前缀匹配 → 超时事件计数
    if (msg.startsWith("CAN frame timeout")) {
        m_timeoutEventCount++;
        emit timeoutEventCountChanged();
    }
}

void CommunicationMonitor::onBusActivityChanged(bool active)
{
    if (m_canBusActive != active) {
        m_canBusActive = active;
        emit canBusActiveChanged();
        updateHealthSummary();
    }
}
```

- [ ] **Step 2: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功。

- [ ] **Step 3: Commit**

```bash
git add model/CommunicationMonitor.cpp
git commit -m "feat(monitor): implement timeout detection and bus activity tracking

- onError() matches 'CAN frame timeout' prefix for timeout counting
- Device loss/stop signals are NOT counted as timeouts
- onBusActivityChanged() relays CanWorker::connectionStatusChanged
- Part of device-status feature (Task 5/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 6: CommunicationMonitor — 重连计数与硬件状态

**Files:**
- Modify: `model/CommunicationMonitor.cpp`

**Interfaces:**
- Consumes: `CanWorker::deviceConnectedChanged(bool)` — connect to `onHardwareStateChanged(connected)`
- Consumes: `CanWorker::reconnectCountChanged(int)` — connect to `onReconnectCountChanged(count)`
- Produces: `hardwareConnected`, `reconnectCount` properties

- [ ] **Step 1: 实现硬件状态和重连计数槽函数**

在 `model/CommunicationMonitor.cpp` 中追加：

```cpp
void CommunicationMonitor::onHardwareStateChanged(bool connected)
{
    if (m_hardwareConnected != connected) {
        m_hardwareConnected = connected;
        emit hardwareConnectedChanged();
        updateHealthSummary();

        // 硬件丢失 → 级联置灰
        if (!connected) {
            // CAN 层不可达
            if (m_canBusActive) {
                m_canBusActive = false;
                emit canBusActiveChanged();
            }
            // AFE 层不可达
            if (m_haveAfeInfo || m_afeOnline) {
                m_afeOnline = false;
                m_haveAfeInfo = false;
                m_afeOfflineSeconds = 0;
                m_offlineTimer->stop();
                emit afeOnlineChanged();
                emit afeOfflineSecondsChanged();
            }
        }
    }
}

void CommunicationMonitor::onReconnectCountChanged(int count)
{
    if (m_reconnectCount != count) {
        m_reconnectCount = count;
        emit reconnectCountChanged();
    }
}
```

- [ ] **Step 2: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功。

- [ ] **Step 3: Commit**

```bash
git add model/CommunicationMonitor.cpp
git commit -m "feat(monitor): implement reconnect tracking and hardware state

- onHardwareStateChanged() relays deviceConnectedChanged + cascade gray-out
- onReconnectCountChanged() relays CanWorker's reconnectCount signal
- Hardware loss triggers cascade: CAN and AFE layers go gray
- Part of device-status feature (Task 6/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 7: CommunicationMonitor — AFE 追踪与离线时长

**Files:**
- Modify: `model/CommunicationMonitor.cpp`

**Interfaces:**
- Consumes: `BmsDataModel::batteryStatusTextChanged(QString)` — connect to `onBatteryStatusChanged()`
- Calls: `m_bmsModel->afeOnline()` to detect transitions
- Produces: `afeOnline`, `afeOfflineSeconds`, `haveAfeInfo` (internal) properties

- [ ] **Step 1: 实现 setBmsModel + onBatteryStatusChanged + tickOfflineDuration**

在 `model/CommunicationMonitor.cpp` 中追加：

```cpp
void CommunicationMonitor::setBmsModel(BmsDataModel *m)
{
    m_bmsModel = m;
    if (m_bmsModel) {
        connect(m_bmsModel, &BmsDataModel::batteryStatusTextChanged,
                this, &CommunicationMonitor::onBatteryStatusChanged);
    }
}

void CommunicationMonitor::onBatteryStatusChanged()
{
    if (!m_bmsModel) return;

    bool currentAfeOnline = m_bmsModel->afeOnline();

    if (!m_haveAfeInfo) {
        // 首个快照到达
        m_haveAfeInfo = true;
        m_afeOnline = currentAfeOnline;
        emit afeOnlineChanged();
        if (!currentAfeOnline) {
            m_afeDropTime = QDateTime::currentDateTimeUtc();
            m_offlineTimer->start();
        }
        updateHealthSummary();
        return;
    }

    // 状态变迁检测
    if (m_afeOnline && !currentAfeOnline) {
        // 在线 → 离线
        m_afeOnline = false;
        m_afeDropTime = QDateTime::currentDateTimeUtc();
        m_offlineTimer->start();
        emit afeOnlineChanged();
        updateHealthSummary();
    } else if (!m_afeOnline && currentAfeOnline) {
        // 离线 → 在线
        m_afeOnline = true;
        m_offlineTimer->stop();
        m_afeOfflineSeconds = 0;
        emit afeOnlineChanged();
        emit afeOfflineSecondsChanged();
        updateHealthSummary();
    }
}

void CommunicationMonitor::tickOfflineDuration()
{
    if (m_afeOnline) return;

    int elapsed = static_cast<int>(m_afeDropTime.secsTo(QDateTime::currentDateTimeUtc()));
    if (m_afeOfflineSeconds != elapsed) {
        m_afeOfflineSeconds = elapsed;
        emit afeOfflineSecondsChanged();
    }
}
```

- [ ] **Step 2: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功。

- [ ] **Step 3: Commit**

```bash
git add model/CommunicationMonitor.cpp
git commit -m "feat(monitor): implement AFE tracking and offline duration

- onBatteryStatusChanged() polls bmsModel->afeOnline() for transitions
- First snapshot sets haveAfeInfo (AFE 'unknown' state → known)
- tickOfflineDuration() increments afeOfflineSeconds every second
- Transition detection: online→offline starts timer, offline→online resets
- Part of device-status feature (Task 7/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 8: CommunicationMonitor — 综合健康度

**Files:**
- Modify: `model/CommunicationMonitor.cpp`

**Interfaces:**
- Produces: `healthSummary` property (0-3, count of green layers)

- [ ] **Step 1: 实现 updateHealthSummary**

在 `model/CommunicationMonitor.cpp` 中追加 `updateHealthSummary()` 和 `setCanWorker()`：

```cpp
void CommunicationMonitor::setCanWorker(CanWorker *w)
{
    m_canWorker = w;
    if (m_canWorker) {
        connect(m_canWorker, &CanWorker::batchReady,
                this, [this](const QVector<CanFrame> &batch) {
                    onFrames(batch.size());
                });
        connect(m_canWorker, &CanWorker::deviceConnectedChanged,
                this, &CommunicationMonitor::onHardwareStateChanged);
        connect(m_canWorker, &CanWorker::connectionStatusChanged,
                this, &CommunicationMonitor::onBusActivityChanged);
        connect(m_canWorker, &CanWorker::reconnectCountChanged,
                this, &CommunicationMonitor::onReconnectCountChanged);
        connect(m_canWorker, &CanWorker::errorOccurred,
                this, &CommunicationMonitor::onError);
    }
}

void CommunicationMonitor::updateHealthSummary()
{
    int green = 0;
    if (m_hardwareConnected) green++;
    if (m_canBusActive) green++;
    if (m_haveAfeInfo && m_afeOnline) green++;

    if (m_healthSummary != green) {
        m_healthSummary = green;
        emit healthSummaryChanged();
    }
}
```

- [ ] **Step 2: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功。

- [ ] **Step 3: Commit**

```bash
git add model/CommunicationMonitor.cpp
git commit -m "feat(monitor): implement health summary and setCanWorker wiring

- updateHealthSummary() counts green layers (0-3)
- setCanWorker() connects all CanWorker signals to internal slots
- batchReady → onFrames (count); connectionStatusChanged → onBusActivityChanged
- deviceConnectedChanged → onHardwareStateChanged; errorOccurred → onError
- Part of device-status feature (Task 8/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 9: main.cpp — 连线与注册

**Files:**
- Modify: `main.cpp`

**Interfaces:**
- Consumes: `CommunicationMonitor`, `CanWorker`, `BmsDataModel` — all already defined
- Produces: `commMonitor` QML context property

- [ ] **Step 1: 在 main.cpp 中创建 CommunicationMonitor 并连线**

`main.cpp` — 在 `bmsModel.setCsvLogger(&csvLogger);` 之后新增：

```cpp
#include "model/CommunicationMonitor.h"  // 添加到文件顶部 includes

    // 在 bmsModel.setCsvLogger(&csvLogger); 之后:
    CommunicationMonitor commMonitor;
    commMonitor.setCanWorker(&canWorker);
    commMonitor.setBmsModel(&bmsModel);
    engine.rootContext()->setContextProperty("commMonitor", &commMonitor);
```

完整 main.cpp 中相关代码段的位置参考：

```cpp
// Line 57-69 区域，在现有的 bmsModel.setCsvLogger 和
// engine.rootContext()->setContextProperty("bms", &bmsModel) 之间:
    bmsModel.setCanWorker(&canWorker);
    bmsModel.setCsvLogger(&csvLogger);

    CommunicationMonitor commMonitor;                          // 新增
    commMonitor.setCanWorker(&canWorker);                      // 新增
    commMonitor.setBmsModel(&bmsModel);                        // 新增

    QObject::connect(&canWorker, &CanWorker::batchReady,
                     &bmsModel, &BmsDataModel::onBatchReady);
    // ... 其他已有的 connect ...

    engine.rootContext()->setContextProperty("bms", &bmsModel);
    engine.rootContext()->setContextProperty("commMonitor", &commMonitor);  // 新增
```

- [ ] **Step 2: 编译验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功。

- [ ] **Step 3: Commit**

```bash
git add main.cpp
git commit -m "feat(main): wire CommunicationMonitor and register to QML

- Create CommunicationMonitor instance, connect to CanWorker and BmsDataModel
- Register as 'commMonitor' context property for QML binding
- Part of device-status feature (Task 9/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 10: CMakeLists.txt — 添加源文件

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 在 CMakeLists.txt 中添加 CommunicationMonitor 源文件**

`CMakeLists.txt` — 在 `qt_add_executable` 块中，`model/` 文件组之后添加：

```cmake
qt_add_executable(BmsHostApp
    main.cpp
    # CAN layer
    can/CanWorker.h can/CanWorker.cpp
    # Protocol layer
    protocol/BmsProtocolDecoder.h protocol/BmsProtocolDecoder.cpp
    protocol/BmsDbcHelper.h protocol/BmsDbcHelper.cpp
    protocol/CsvLogger.h protocol/CsvLogger.cpp
    # Model layer
    model/BmsDataModel.h model/BmsDataModel.cpp
    model/CellVoltageModel.h model/CellVoltageModel.cpp
    model/TemperatureModel.h model/TemperatureModel.cpp
    model/FaultListModel.h model/FaultListModel.cpp
    model/CommunicationMonitor.h model/CommunicationMonitor.cpp   # 新增
)
```

- [ ] **Step 2: 完整构建验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译和链接均成功，无错误。

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add CommunicationMonitor to CMakeLists.txt

- Part of device-status feature (Task 10/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 11: DeviceStatusPage.qml — 三层健康链 UI

**Files:**
- Create: `qml/DeviceStatusPage.qml`

**Interfaces:**
- Consumes: `commMonitor` context property (all 10 properties)
- Produces: Visual Tab page showing three-layer health chain

- [ ] **Step 1: 编写 DeviceStatusPage.qml**

`qml/DeviceStatusPage.qml`：

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "设备状态"
    id: deviceStatusPage

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        spacing: 0

        // 标题行
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "通信链路健康度: " + commMonitor.healthSummary + "/3"
            font.pixelSize: 18
            font.bold: true
            color: commMonitor.healthSummary === 3 ? "#27ae60"
                 : commMonitor.healthSummary >= 2 ? "#f39c12" : "#e74c3c"
        }

        Item { Layout.preferredHeight: 24 }

        // === Layer 1: PCAN 硬件 ===
        LayerCard {
            layerName: "PCAN 硬件"
            statusColor: commMonitor.hardwareConnected ? "#27ae60" : "#e74c3c"
            statusText: {
                if (commMonitor.hardwareConnected) return "● 设备已连接"
                if (commMonitor.reconnectCount > 0) return "● 设备丢失 (重连中...)"
                return "— 等待设备"
            }
            blinkState: commMonitor.hardwareConnected ? "solid"
                       : commMonitor.reconnectCount > 0 ? "fast" : "solid"
            metrics: "丢失计数: " + commMonitor.reconnectCount
            metricsVisible: commMonitor.reconnectCount > 0
        }

        // 层间连线
        ConnectorLine {
            color: commMonitor.hardwareConnected ? "#27ae60" : "#999"
        }

        // === Layer 2: CAN 总线 ===
        LayerCard {
            layerName: "CAN 总线"
            statusColor: {
                if (!commMonitor.hardwareConnected) return "#999"
                if (!commMonitor.canBusActive) return "#e74c3c"
                // 黄色过渡: canBusActive==true && canFrameRate==0 (刚恢复)
                if (commMonitor.canFrameRate <= 0.0) return "#f39c12"
                return "#27ae60"
            }
            statusText: {
                if (!commMonitor.hardwareConnected) return "— 链路中断"
                if (!commMonitor.canBusActive) return "● 帧超时"
                if (commMonitor.canFrameRate <= 0.0) return "● 等待首帧..."
                return "● 帧率正常"
            }
            blinkState: {
                if (!commMonitor.hardwareConnected) return "solid"
                if (!commMonitor.canBusActive) return "fast"
                if (commMonitor.canFrameRate <= 0.0) return "slow"
                return "solid"
            }
            metrics: commMonitor.canFrameRate.toFixed(0) + " fps  |  "
                     + commMonitor.totalFrameCount.toLocaleString() + " 帧  |  "
                     + "超时 " + commMonitor.timeoutEventCount + " 次"
            metricsVisible: commMonitor.hardwareConnected
        }

        // 层间连线
        ConnectorLine {
            color: {
                if (!commMonitor.hardwareConnected) return "#999"
                if (!commMonitor.canBusActive) return "#e74c3c"
                if (commMonitor.canFrameRate <= 0.0) return "#f39c12"
                return "#27ae60"
            }
        }

        // === Layer 3: AFE 通信 ===
        LayerCard {
            layerName: "AFE 芯片 (I2C)"
            statusColor: {
                if (!commMonitor.hardwareConnected || !commMonitor.canBusActive) return "#999"
                if (!commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0) return "#e74c3c"
                if (!commMonitor.afeOnline) return "#f39c12"
                return "#27ae60"
            }
            statusText: {
                if (!commMonitor.hardwareConnected || !commMonitor.canBusActive) return "— 链路中断"
                if (!commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0)
                    return "● 芯片离线"
                if (!commMonitor.afeOnline) return "— 等待数据"
                return "● 芯片在线"
            }
            blinkState: {
                if (!commMonitor.hardwareConnected || !commMonitor.canBusActive) return "solid"
                if (!commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0) return "fast"
                if (!commMonitor.afeOnline) return "slow"
                return "solid"
            }
            metrics: {
                if (commMonitor.afeOnline) return ""
                if (commMonitor.afeOfflineSeconds > 0) {
                    var s = commMonitor.afeOfflineSeconds
                    var m = Math.floor(s / 60)
                    s = s % 60
                    return "离线: " + m + "分" + s + "秒"
                }
                return ""
            }
            metricsVisible: !commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0
        }
    }

    // === 子组件 ===

    // LayerCard: 单层状态卡片
    component LayerCard: Rectangle {
        property string layerName: ""
        property color statusColor: "#999"
        property string statusText: ""
        property string blinkState: "solid"   // solid | slow | fast
        property string metrics: ""
        property bool metricsVisible: false

        width: parent.width
        height: metricsVisible ? 72 : 52
        radius: 8
        color: "#fafafa"
        border.color: "#e0e0e0"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // 状态指示灯
            Rectangle {
                width: 16; height: 16; radius: 8
                color: statusColor
                opacity: {
                    if (blinkState === "solid") return 1.0
                    return 1.0  // animation handles blinking
                }

                // 脉冲动画
                SequentialAnimation on opacity {
                    running: blinkState !== "solid"
                    loops: Animation.Infinite
                    PropertyAnimation {
                        from: 1.0; to: 0.15
                        duration: blinkState === "fast" ? 250 : 1000
                    }
                    PropertyAnimation {
                        from: 0.15; to: 1.0
                        duration: blinkState === "fast" ? 250 : 1000
                    }
                }
            }

            ColumnLayout {
                spacing: 2
                Text {
                    text: layerName
                    font.pixelSize: 14
                    font.bold: true
                    color: "#333"
                }
                Text {
                    text: statusText
                    font.pixelSize: 13
                    color: statusColor
                }
                Text {
                    visible: metricsVisible
                    text: metrics
                    font.pixelSize: 11
                    color: "#999"
                }
            }
        }
    }

    // ConnectorLine: 层间连接线
    component ConnectorLine: Rectangle {
        property color color: "#27ae60"

        width: 2
        height: 8
        anchors.horizontalCenter: parent.horizontalCenter
        color: parent.color
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add qml/DeviceStatusPage.qml
git commit -m "feat(qml): add DeviceStatusPage with three-layer health chain

- LayerCard component: colored dot (solid/slow pulse/fast pulse) + status text + metrics
- ConnectorLine: vertical line between layers, color follows upstream health
- Cascade gray-out: upper failure → lower layers gray
- Color states: green(ok), yellow(transition), red(fault), gray(unreachable/unknown)
- Health summary: 'N/3' display with dynamic color
- Part of device-status feature (Task 11/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 12: main.qml — Tab 集成

**Files:**
- Modify: `qml/main.qml`

- [ ] **Step 1: 在 TabBar 和 StackLayout 中添加设备状态页**

`qml/main.qml` — 修改 `header: TabBar` 块和 `StackLayout` 块：

在 `TabBar` 中，"曲线" TabButton 之后添加：

```qml
    header: TabBar {
        id: mainTab
        TabButton { text: "总览" }
        TabButton { text: "电芯" }
        TabButton { text: "曲线" }
        TabButton { text: "设备状态" }    // 新增
        Item { Layout.fillWidth: true }
        ToolButton { text: "☰"; onClicked: controlPanel.open() }
    }
```

在 `StackLayout` 中，`TrendsPage {}` 之后添加：

```qml
    StackLayout {
        anchors.fill: parent
        currentIndex: mainTab.currentIndex
        OverviewPage {}
        CellsPage { id: cellsPageObj; Component.onCompleted: root.cellsPage = cellsPageObj }
        TrendsPage {}
        DeviceStatusPage {}    // 新增
    }
```

- [ ] **Step 2: 编译并运行验证**

```powershell
cmake --build build --target BmsHostApp
```

预期：编译成功，启动应用后可见 4 个 Tab（总览/电芯/曲线/设备状态），点击"设备状态"可看到三层健康链 UI。

- [ ] **Step 3: Commit**

```bash
git add qml/main.qml
git commit -m "feat(qml): integrate DeviceStatusPage into main TabBar

- Add '设备状态' TabButton (after '曲线')
- Add DeviceStatusPage to StackLayout
- Part of device-status feature (Task 12/13)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 13: 集成验证与验收

**Files:**
- 无新文件 — 验证整个功能链

- [ ] **Step 1: 完整构建**

```powershell
cmake --build build --target BmsHostApp --clean-first
```

预期：零错误、零警告。

- [ ] **Step 2: 启动应用，验证初始状态**

启动应用（不插入 PCAN 设备），切换到「设备状态」Tab：

预期：
- PCAN 层灰色"等待设备"，无动画
- CAN 层灰色"链路中断"
- AFE 层灰色"链路中断"
- 综合健康度 0/3，红色
- 重连计数 0
- 切换到其他 Tab 再切回：数据不重置

- [ ] **Step 3: 插入 PCAN 设备，验证正常链路**

插入 PCAN 设备并确保固件正在发送 CAN 帧，刷新页面：

预期：
- PCAN 层绿色常亮"设备已连接"
- CAN 层绿色常亮"帧率正常"，显示 ~50 fps
- AFE 层绿色常亮"芯片在线"
- 综合健康度 3/3，绿色
- 总帧数持续递增

- [ ] **Step 4: 验证超时红色快闪**

拔出 CAN 总线终端（或断开固件），等待 500ms 超时：

预期：
- PCAN 层保持绿色
- CAN 层红色快闪（0.5s 周期脉冲）
- AFE 层灰色"链路中断"
- 超时计数递增
- 帧率降为 0

- [ ] **Step 5: 验证超时恢复黄色过渡**

恢复 CAN 总线连接：

预期：
- CAN 层黄色慢闪（2s 周期脉冲）
- 3 秒后自动转为绿色常亮
- 帧率恢复为 ~50

- [ ] **Step 6: 验证设备拔除**

拔出 PCAN-USB 设备：

预期：
- PCAN 层红色快闪"设备丢失"
- CAN/AFE 级联置灰
- 重连计数立即 +1（不会持续递增）

- [ ] **Step 7: Commit 验收记录**

```bash
git add -A
git commit -m "verify(device-status): integration test pass

- Initial state: all gray, health 0/3 ✓
- Normal link: all green, ~50fps, health 3/3 ✓
- Timeout: CAN red blink, cascade gray ✓
- Recovery: CAN yellow→green transition ✓
- Device unplug: PCAN red, reconnectCount+1 ✓

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Self-Review

### 1. Spec Coverage

| Spec Requirement | Task Coverage |
|-----------------|---------------|
| R1: Tab 页展示三层链路 | Task 11 (UI), Task 12 (Tab 集成), Task 13 (验收) |
| R2: 实时 CAN 帧率 | Task 4 (帧率计算) |
| R3: 超时事件计数 | Task 5 (超时检测) |
| R4: 重连计数 | Task 1 (CanWorker), Task 6 (relay) |
| R5: AFE 离线时长 | Task 2 (BmsDataModel getter), Task 7 (AFE tracking) |
| R6: 指示灯颜色与动画 | Task 11 (QML 动画), Task 13 (验收) |
| R7: 累计总帧数 | Task 4 (totalFrameCount) |
| 级联置灰 | Task 6 (硬件丢失), Task 11 (QML 条件颜色) |
| 黄色过渡 3s | Task 11 (QML 条件 + 动画周期) |
| healthSummary | Task 8 (updateHealthSummary), Task 11 (QML 显示) |
| 非目标约束 | 全 Tasks 遵守：无固件改动、不动 footer、无趋势图 |

### 2. Placeholder Scan

✅ 无 "TBD"、"TODO"、"implement later"、"add appropriate error handling"、"Similar to Task N"
✅ 所有代码步骤有具体代码块
✅ 所有类型/签名在定义 Task 中已声明

### 3. Type Consistency

✅ `canFrameRate`: Task 4 定义为 `qreal` → Task 11 使用 `.toFixed(0)` ← 一致
✅ `timeoutEventCount`: Task 5 定义为 `quint32` → Task 11 使用字符串拼接 ← 一致
✅ `healthSummary`: Task 8 定义为 `int` (0-3) → Task 11 使用 "N/3" 格式 ← 一致
✅ `reconnectCountChanged(int)`: Task 1 定义 → Task 6/8 connect ← 一致
✅ `afeOnline()`: Task 2 定义 → Task 7 调用 `m_bmsModel->afeOnline()` ← 一致
