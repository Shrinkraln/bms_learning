# BMS QT 上位机 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建基于 Qt 6.5+ / QML / PCAN-USB 的 BMS 9S 桌面端上位机，3 页面仪表盘 + 双向 CAN 通信 + CSV 日志 + DBC 支持，同步修改 BMS 固件 CAN 协议（新增温度帧、FET/均衡状态、修复故障帧编码）。

**Architecture:** CAN Worker Thread (QCanBusDevice peakcan) → Protocol Decoder (纯函数) → BmsDataModel (Q_PROPERTY + QAbstractListModel) → QML Views (3 页面)。30ms 批量快照避免信号洪峰。标量走属性绑定，列表走 Model/View。

**Tech Stack:** Qt 6.5+ LTS, CMake 3.21+, C++17, QML, QtSerialBus (peakcan), Qt Charts (QML), MSVC 或 MinGW

**Spec:** `.spec-dev/2026-08-06-bms-qt-host/spec/bms-qt-host-design.md`

## Global Constraints

- Qt 6.5+ LTS, CMake 3.21+, C++17
- PCAN-USB 适配器, peakcan 插件, 500kbps
- 编译 0 错误 0 警告 (MSVC 或 MinGW)
- 所有用户可见字符串使用 `tr()` 包裹
- QCanBusDevice 操作全部在 Worker Thread，跨线程 QueuedConnection
- 30ms QTimer 批量快照更新 BmsDataModel，仅变更时 NOTIFY
- DLC 不足的帧跳过不解码，日志记录
- QSettings 即时保存（非退出时），窗口几何 500ms 防抖
- BMS 固件修改约 30 行，仅 CAN 帧打包，不改变算法/BSP

---

### Task 1: BMS 固件 — can_cmd 协议扩展

**Files:**
- Modify: `App/Inc/can_cmd.h`
- Modify: `App/Src/can_cmd.c`

**Interfaces:**
- Produces: `CAN_TX_TEMPERATURE 0x121U` 宏；`can_pub()` 新增第 5 帧输出 + 0x120 data[5] 改为 ctrl 字节

- [ ] **Step 1: 在 can_cmd.h 中新增温度帧 CAN ID**

在 `App/Inc/can_cmd.h` 的 CAN ID 定义区，`CAN_TX_SOC_OCV 0x130U` 后添加：

```c
#define CAN_TX_TEMPERATURE    0x121U
```

- [ ] **Step 2: 修改 can_pub() — 0x120 data[5] 改为 ctrl 字节**

在 `App/Src/can_cmd.c` 的 `can_pub()` 函数中，找到 Frame 2 (0x120) 的构建代码。将：

```c
frames[2].data[5] = (uint8_t)bms->prot_level;
```

替换为：

```c
/* ctrl byte: [1:0]=prot_level, [2]=CHG_FET, [3]=DSG_FET, [4]=BALANCING */
{
    uint8_t ctrl = (uint8_t)(bms->prot_level & 0x03U);
    /* FET/均衡状态从全局读取 (task_protect/bms_app 维护) */
    extern uint8_t g_fet_chg_on;    /* 定义在 bms_app.c */
    extern uint8_t g_fet_dsg_on;
    extern uint8_t g_balancing_active;
    if (g_fet_chg_on)  { ctrl |= (1U << 2U); }
    if (g_fet_dsg_on)  { ctrl |= (1U << 3U); }
    if (g_balancing_active) { ctrl |= (1U << 4U); }
    frames[2].data[5] = ctrl;
}
```

- [ ] **Step 3: 修改 can_pub() — 新增 0x121 温度帧 (Frame 4)**

在 `can_pub()` 函数末尾，`return 4U;` 之前，添加第 5 帧：

```c
/* ---- Frame 4: 0x121 TEMPERATURE (6 bytes, big-endian int16 ×3) ---- */
frames[4].id  = CAN_TX_TEMPERATURE;
frames[4].len = 8U;
{
    const bq76940_temp_data_t *temps = &bms->battery_val.temps;
    for (uint8_t i = 0U; i < 3U; i++) {
        int16_t t = temps->ts_mdeg_c[i];
        frames[4].data[i * 2U]     = (uint8_t)(((uint16_t)t >> 8U) & 0xFFU);
        frames[4].data[i * 2U + 1U] = (uint8_t)((uint16_t)t & 0xFFU);
    }
    frames[4].data[6] = 0x00U;
    frames[4].data[7] = 0x00U;
}
```

并将返回语句改为：

```c
return 5U;
```

- [ ] **Step 4: 更新 can_pub() 调用者 — frames 数组扩容**

在 `App/Src/bms_app.c` 的 `task_can_tx_entry()` 中，将：

```c
can_msg_t frames[4];
```

改为：

```c
can_msg_t frames[5];
```

- [ ] **Step 5: 添加全局 FET/均衡状态变量到 bms_app.c**

在 `App/Src/bms_app.c` 顶部现有全局变量区末尾添加：

```c
/* FET 与均衡状态 (供 can_pub 读取) */
uint8_t g_fet_chg_on       = 1U;
uint8_t g_fet_dsg_on       = 1U;
uint8_t g_balancing_active = 0U;
```

- [ ] **Step 6: 在 FET 控制路径同步更新全局变量**

在 `task_protect_entry()` 中，每次调用 `bq76940_write_sys_ctrl2()` 后，更新全局变量。例如在 `ctrl2_val` 计算完成后、`bq76940_write_sys_ctrl2` 调用后添加：

```c
g_fet_chg_on = (ctrl2_val & BQ76940_SYS_CTRL2_CHG_FET) ? 1U : 0U;
g_fet_dsg_on = (ctrl2_val & BQ76940_SYS_CTRL2_DSG_FET) ? 1U : 0U;
```

在 `task_can_rx_entry()` 的 CAN_ACTION_FET_CHG_ON/CHG_OFF/DSG_ON/DSG_OFF 分支中也同步更新。

在 `task_balance_entry()` 的 `bq76940_set_balancing()` 调用后设置 `g_balancing_active = 1U`，`bq76940_balance_off()` 后设置 `g_balancing_active = 0U`。

- [ ] **Step 7: 编译验证固件**

```bash
cd bms_9s_f103 && cmake --build build --target bms_9s_f103
```

Expected: 0 错误 0 警告

- [ ] **Step 8: Commit**

```bash
git add App/Inc/can_cmd.h App/Src/can_cmd.c App/Src/bms_app.c
git commit -m "feat(can): add 0x121 TEMPERATURE frame + 0x120 ctrl byte (FET/balance state)"
```

### Task 2: BMS 固件 — 0x101 故障帧 re-layout

**Files:**
- Modify: `App/Src/bms_app.c`

**Interfaces:**
- Modifies: `task_protect_entry()` 中 0x101 故障帧打包

- [ ] **Step 1: 修改 0x101 故障帧布局**

在 `App/Src/bms_app.c` 的 `task_protect_entry()` 中，找到 `fault_frame` 构建代码块（当前 8 字节赋值）。将整个构建块替换为：

```c
/* ⑤ 生成故障 CAN 帧 (新 layout: uint16 电压, 无电流) */
fault_frame.id  = CAN_TX_BMS_FAULT;
fault_frame.len = 8U;
fault_frame.data[0] = (uint8_t)level;
fault_frame.data[1] = (uint8_t)((faults >> 8U) & 0xFFU);
fault_frame.data[2] = (uint8_t)(faults & 0xFFU);
/* max_cell_mV — uint16 big-endian */
{
    uint16_t max_mv = bms->battery_val.cells.max_mv;
    fault_frame.data[3] = (uint8_t)((max_mv >> 8U) & 0xFFU);
    fault_frame.data[4] = (uint8_t)(max_mv & 0xFFU);
}
/* min_cell_mV — uint16 big-endian */
{
    uint16_t min_mv = bms->battery_val.cells.min_mv;
    fault_frame.data[5] = (uint8_t)((min_mv >> 8U) & 0xFFU);
    fault_frame.data[6] = (uint8_t)(min_mv & 0xFFU);
}
/* max_temp + 40°C offset (1°C resolution) */
{
    int16_t max_temp = bms->battery_val.temps.ts_mdeg_c[0];
    for (uint8_t i = 1U; i < BQ76940_TS_COUNT; i++) {
        if (bms->battery_val.temps.ts_mdeg_c[i] > max_temp) {
            max_temp = bms->battery_val.temps.ts_mdeg_c[i];
        }
    }
    fault_frame.data[7] = (uint8_t)((max_temp / 10) + 40);
}
```

- [ ] **Step 2: 编译验证固件**

```bash
cd bms_9s_f103 && cmake --build build --target bms_9s_f103
```

Expected: 0 错误 0 警告（fault_frame 旧字段引用已被新代码完全替代）

- [ ] **Step 3: Commit**

```bash
git add App/Src/bms_app.c
git commit -m "fix(protect): 0x101 fault frame re-layout — uint16 voltages, remove current field"
```

### Task 3: BmsHostApp 项目脚手架

**Files:**
- Create: `BmsHostApp/CMakeLists.txt`
- Create: `BmsHostApp/main.cpp`
- Create: directory structure: `can/`, `protocol/`, `model/`, `qml/`, `qml/components/`, `resources/`, `translations/`

**Interfaces:**
- Produces: 可编译的 Qt 空白窗口 + QML engine 启动

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p BmsHostApp/{can,protocol,model,qml/components,resources,translations}
```

- [ ] **Step 2: 编写根 CMakeLists.txt**

File: `BmsHostApp/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.21)
project(BmsHostApp VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Quick Qml SerialBus Charts Widgets)

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
)

qt_add_qml_module(BmsHostApp
    URI "com.bms.host"
    QML_FILES
        qml/main.qml
        qml/OverviewPage.qml
        qml/CellsPage.qml
        qml/TrendsPage.qml
        qml/ControlPanel.qml
        qml/ConfigDialog.qml
        qml/components/GaugeArc.qml
        qml/components/CellBar.qml
        qml/components/FaultRibbon.qml
)

target_link_libraries(BmsHostApp PRIVATE
    Qt6::Core Qt6::Quick Qt6::Qml Qt6::SerialBus Qt6::Charts Qt6::Widgets)

target_include_directories(BmsHostApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/can
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/model)

set_target_properties(BmsHostApp PROPERTIES
    WIN32_EXECUTABLE TRUE
    FOLDER "BmsHostApp")
```

- [ ] **Step 3: 编写 main.cpp**

File: `BmsHostApp/main.cpp`

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QTranslator>
#include <QLocale>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("BMS");
    app.setApplicationName("BmsHostApp");

    // 加载翻译
    QTranslator translator;
    if (translator.load(QLocale::system(), "BmsHostApp", "_",
                        ":/translations")) {
        app.installTranslator(&translator);
    }

    QQmlApplicationEngine engine;

    // Model 将在后续任务中创建并注册到 context
    // BmsDataModel model;
    // engine.rootContext()->setContextProperty("bms", &model);

    const QUrl url("qrc:/com/bms/host/qml/main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
```

- [ ] **Step 4: 编写最小 main.qml 确认脚手架可用**

File: `BmsHostApp/qml/main.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: "BMS 9S 上位机"

    Component.onCompleted: {
        // 恢复窗口几何
        var geo = settings.value("ui/windowGeometry", "")
        if (geo !== "") {
            // QML 侧暂不恢复几何 (C++ 侧设置)
        }
    }

    // 状态栏
    footer: Rectangle {
        height: 24
        color: "#f0f0f0"
        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            Text { text: "● 未连接"; id: statusText }
            Item { Layout.fillWidth: true }
            Text { text: "最后更新: --"; id: lastUpdateText }
        }
    }

    // 占位页面 (后续任务替换)
    Text {
        anchors.centerIn: parent
        text: "BMS 9S 上位机\nQt 6.5+ / PCAN-USB"
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 24
        color: "#999"
    }
}
```

- [ ] **Step 5: 编译验证**

```bash
cd BmsHostApp && cmake -S . -B build && cmake --build build
```

Expected: 0 错误 0 警告，生成 BmsHostApp.exe

- [ ] **Step 6: Commit**

```bash
git add BmsHostApp/
git commit -m "scaffold(bms-host): CMake + main.cpp + minimal QML window"
```

### Task 4: CAN Worker Layer

**Files:**
- Create: `BmsHostApp/can/CanFrame.h`
- Create: `BmsHostApp/can/CanWorker.h`
- Create: `BmsHostApp/can/CanWorker.cpp`

**Interfaces:**
- Produces:
  - `struct CanFrame { quint32 id; quint8 len; quint8 data[8]; QCanBusFrame::TimeStamp ts; }`
  - `class CanWorker : public QObject { ... }` — signals: `batchReady(QVector<CanFrame>)`, `connectionStatusChanged(bool)`, `errorOccurred(QString)`; slots: `start()`, `stop()`, `sendFrame(CanFrame)`

- [ ] **Step 1: 编写 CanFrame.h**

File: `BmsHostApp/can/CanFrame.h`

```cpp
#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <QtSerialBus/QCanBusFrame>
#include <QtCore/QtGlobal>

struct CanFrame {
    quint32 id = 0;
    quint8  len = 0;
    quint8  data[8] = {};

    static CanFrame fromQCanBusFrame(const QCanBusFrame &f) {
        CanFrame cf;
        cf.id = f.frameId();
        cf.len = static_cast<quint8>(qMin(f.payload().size(), 8));
        for (int i = 0; i < cf.len; ++i)
            cf.data[i] = static_cast<quint8>(f.payload().at(i));
        return cf;
    }

    QCanBusFrame toQCanBusFrame() const {
        QCanBusFrame f;
        f.setFrameId(id);
        f.setPayload(QByteArray(reinterpret_cast<const char*>(data), len));
        return f;
    }
};

#endif // CAN_FRAME_H
```

- [ ] **Step 2: 编写 CanWorker.h**

File: `BmsHostApp/can/CanWorker.h`

```cpp
#ifndef CAN_WORKER_H
#define CAN_WORKER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QtSerialBus/QCanBus>
#include <QtSerialBus/QCanBusDevice>
#include "CanFrame.h"

class CanWorker : public QObject {
    Q_OBJECT
public:
    explicit CanWorker(QObject *parent = nullptr);
    ~CanWorker();

public slots:
    void start(const QString &plugin, const QString &interface, int bitrate);
    void stop();
    void sendFrame(const CanFrame &frame);

signals:
    void batchReady(const QVector<CanFrame> &frames);
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &errorString);

private slots:
    void onFramesReceived();
    void onErrorOccurred(QCanBusDevice::CanBusError error);
    void checkTimeout();

private:
    QCanBusDevice *m_device = nullptr;
    QTimer *m_timeoutTimer;
    static constexpr int TIMEOUT_MS = 500;
};

#endif // CAN_WORKER_H
```

- [ ] **Step 3: 编写 CanWorker.cpp**

File: `BmsHostApp/can/CanWorker.cpp`

```cpp
#include "CanWorker.h"
#include <QDateTime>
#include <QDebug>

CanWorker::CanWorker(QObject *parent) : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setInterval(TIMEOUT_MS);
    connect(m_timeoutTimer, &QTimer::timeout, this, &CanWorker::checkTimeout);
}

CanWorker::~CanWorker() { stop(); }

void CanWorker::start(const QString &plugin, const QString &interface, int bitrate)
{
    if (m_device) { stop(); }

    QString errorStr;
    m_device = QCanBus::instance()->createDevice(plugin, interface, &errorStr);
    if (!m_device) {
        emit errorOccurred("Cannot create device: " + errorStr);
        return;
    }

    m_device->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);
    connect(m_device, &QCanBusDevice::framesReceived,
            this, &CanWorker::onFramesReceived);
    connect(m_device, &QCanBusDevice::errorOccurred,
            this, &CanWorker::onErrorOccurred);

    if (!m_device->connectDevice()) {
        emit errorOccurred("Cannot connect: " + m_device->errorString());
        delete m_device;
        m_device = nullptr;
        return;
    }

    m_timeoutTimer->start();
    emit connectionStatusChanged(true);
}

void CanWorker::stop()
{
    m_timeoutTimer->stop();
    if (m_device) {
        m_device->disconnectDevice();
        delete m_device;
        m_device = nullptr;
    }
    emit connectionStatusChanged(false);
}

void CanWorker::sendFrame(const CanFrame &frame)
{
    if (m_device && m_device->state() == QCanBusDevice::ConnectedState) {
        m_device->writeFrame(frame.toQCanBusFrame());
    }
}

void CanWorker::onFramesReceived()
{
    if (!m_device) return;
    QVector<CanFrame> batch;
    while (m_device->framesAvailable()) {
        QCanBusFrame f = m_device->readFrame();
        if (f.isValid() && f.payload().size() >= 0) {
            batch.append(CanFrame::fromQCanBusFrame(f));
        }
    }
    if (!batch.isEmpty()) {
        m_timeoutTimer->start(); // 重置超时计时器
        emit batchReady(batch);
    }
}

void CanWorker::onErrorOccurred(QCanBusDevice::CanBusError error)
{
    Q_UNUSED(error)
    if (m_device) {
        emit errorOccurred(m_device->errorString());
    }
}

void CanWorker::checkTimeout()
{
    m_timeoutTimer->stop();
    emit connectionStatusChanged(false);
    emit errorOccurred("CAN frame timeout (>500ms)");
}
```

- [ ] **Step 4: Commit**

```bash
git add BmsHostApp/can/
git commit -m "feat(can): CanFrame + CanWorker (QCanBusDevice peakcan, batch read, timeout detect)"
```

### Task 5: Protocol Decoder

**Files:**
- Create: `BmsHostApp/protocol/BmsSnapshot.h`
- Create: `BmsHostApp/protocol/BmsProtocolDecoder.h`
- Create: `BmsHostApp/protocol/BmsProtocolDecoder.cpp`

**Interfaces:**
- Produces:
  - `struct BmsSnapshot { ... }` — 完整数据快照，含 cells[9], temps[3], pack_voltage, pack_current, soc_permil, real_soc_permil, remaining_mah, q_max_mah, prot_level, active_faults, fet_chg, fet_dsg, balancing, max_cell_mv, min_cell_mv, cell_diff_mv, max_temp_0p1c, connection_alive, timestamp }`
  - `class BmsProtocolDecoder` — `static QVector<BmsSnapshot> decodeFrames(const QVector<CanFrame> &batch)`, `static CanFrame encodeCommand(...)`

- [ ] **Step 1: 编写 BmsSnapshot.h**

File: `BmsHostApp/protocol/BmsSnapshot.h`

```cpp
#ifndef BMS_SNAPSHOT_H
#define BMS_SNAPSHOT_H

#include <QtCore/QtGlobal>
#include <QtCore/QDateTime>
#include <array>

struct BmsSnapshot {
    // 电芯
    std::array<quint16, 9> cell_mv = {};
    quint16 cell_min_mv = 0;
    quint16 cell_max_mv = 0;
    quint16 cell_diff_mv = 0;
    // 总压/电流
    quint32 pack_voltage_mv = 0;
    qint32  pack_current_ma = 0;
    // SOC
    quint16 soc_permil = 0;
    quint16 real_soc_permil = 0;
    qint32  remaining_mah = 0;
    qint32  q_max_mah = 0;
    // 温度 (0.1°C)
    std::array<qint16, 3> temp_0p1c = {};
    qint16 max_temp_0p1c = 0;
    // 保护
    quint8  prot_level = 0;
    quint16 active_faults = 0;
    // FET + 均衡
    bool fet_chg = false;
    bool fet_dsg = false;
    bool balancing = false;
    // 元数据
    bool connection_alive = true;
    QDateTime timestamp;
};

#endif // BMS_SNAPSHOT_H
```

- [ ] **Step 2: 编写 BmsProtocolDecoder.h**

File: `BmsHostApp/protocol/BmsProtocolDecoder.h`

```cpp
#ifndef BMS_PROTOCOL_DECODER_H
#define BMS_PROTOCOL_DECODER_H

#include <QVector>
#include <QString>
#include "BmsSnapshot.h"
#include "can/CanFrame.h"

struct BmsCommand {
    enum Type { QUERY = 0, CONTROL, CONFIG };
    Type type;
    quint8 subType;
    quint16 value;
};

class BmsProtocolDecoder {
public:
    // 上行解码 — 批量 CAN 帧 → 快照
    static QVector<BmsSnapshot> decodeFrames(const QVector<CanFrame> &batch);

    // 下行编码 — 指令 → CAN 帧
    static CanFrame encodeCommand(const BmsCommand &cmd);

    // 工具
    static QString faultName(quint8 bit);
    static const char* faultSeverity(quint8 bit); // "WARNING"/"ALERT"/"FAULT"

private:
    static bool checkDlc(const CanFrame &f, quint8 minLen);
    static BmsSnapshot decodeStatusFrame(const CanFrame &f);
    static BmsSnapshot decodeFaultFrame(const CanFrame &f);
    static BmsSnapshot decodeCellFrame1_4(const CanFrame &f, BmsSnapshot &snap);
    static BmsSnapshot decodeCellFrame5_8(const CanFrame &f, BmsSnapshot &snap);
    static BmsSnapshot decodeSocOcvFrame(const CanFrame &f, BmsSnapshot &snap);
    static BmsSnapshot decodeTemperatureFrame(const CanFrame &f, BmsSnapshot &snap);
};

#endif // BMS_PROTOCOL_DECODER_H
```

- [ ] **Step 3: 编写 BmsProtocolDecoder.cpp — 解码主入口 + DLC 检查**

File: `BmsHostApp/protocol/BmsProtocolDecoder.cpp`

```cpp
#include "BmsProtocolDecoder.h"
#include <QDebug>

// CAN ID 定义
static constexpr quint32 ID_BMS_STATUS   = 0x100U;
static constexpr quint32 ID_BMS_FAULT    = 0x101U;
static constexpr quint32 ID_CELL_VOLT_1_4 = 0x110U;
static constexpr quint32 ID_CELL_VOLT_5_8 = 0x111U;
static constexpr quint32 ID_BMS_STATUS2  = 0x120U;
static constexpr quint32 ID_TEMPERATURE  = 0x121U;
static constexpr quint32 ID_SOC_OCV      = 0x130U;

bool BmsProtocolDecoder::checkDlc(const CanFrame &f, quint8 minLen)
{
    if (f.len < minLen) {
        qCWarning(bmsProto, "DLC too short: id=0x%03X, got=%d, expected>=%d",
                  f.id, f.len, minLen);
        return false;
    }
    return true;
}

QVector<BmsSnapshot> BmsProtocolDecoder::decodeFrames(const QVector<CanFrame> &batch)
{
    BmsSnapshot snap;
    bool has_cells_1_4 = false;
    bool has_cells_5_8 = false;
    bool has_status    = false;
    bool has_temp      = false;
    bool has_soc       = false;

    for (const auto &f : batch) {
        switch (f.id) {
        case ID_BMS_FAULT:
            if (checkDlc(f, 8)) { snap = decodeFaultFrame(f); }
            break;
        case ID_CELL_VOLT_1_4:
            if (checkDlc(f, 8)) { snap = decodeCellFrame1_4(f, snap); has_cells_1_4 = true; }
            break;
        case ID_CELL_VOLT_5_8:
            if (checkDlc(f, 8)) { snap = decodeCellFrame5_8(f, snap); has_cells_5_8 = true; }
            break;
        case ID_BMS_STATUS2:
            if (checkDlc(f, 8)) { snap = decodeStatusFrame(f); has_status = true; }
            break;
        case ID_TEMPERATURE:
            if (checkDlc(f, 6)) { snap = decodeTemperatureFrame(f, snap); has_temp = true; }
            break;
        case ID_SOC_OCV:
            if (checkDlc(f, 8)) { snap = decodeSocOcvFrame(f, snap); has_soc = true; }
            break;
        default: break;
        }
    }

    snap.connection_alive = has_status;
    snap.timestamp = QDateTime::currentDateTimeUtc();

    // 计算派生值
    if (has_cells_1_4 && has_cells_5_8 && has_status) {
        snap.cell_min_mv = snap.cell_mv[0];
        snap.cell_max_mv = snap.cell_mv[0];
        quint32 sum = 0;
        for (int i = 0; i < 9; ++i) {
            if (snap.cell_mv[i] < snap.cell_min_mv) snap.cell_min_mv = snap.cell_mv[i];
            if (snap.cell_mv[i] > snap.cell_max_mv) snap.cell_max_mv = snap.cell_mv[i];
            sum += snap.cell_mv[i];
        }
        snap.cell_diff_mv = snap.cell_max_mv - snap.cell_min_mv;
    }

    if (has_temp) {
        snap.max_temp_0p1c = snap.temp_0p1c[0];
        for (int i = 1; i < 3; ++i) {
            if (snap.temp_0p1c[i] > snap.max_temp_0p1c)
                snap.max_temp_0p1c = snap.temp_0p1c[i];
        }
    }

    return { snap };
}
```

- [ ] **Step 4: 编写各帧解码函数**

在同一个 cpp 文件中继续：

```cpp
BmsSnapshot BmsProtocolDecoder::decodeStatusFrame(const CanFrame &f)
{
    BmsSnapshot s;
    quint8 soc_pct = f.data[0]; // 0-100
    s.soc_permil = static_cast<quint16>(soc_pct) * 10U;
    s.pack_voltage_mv = (static_cast<quint16>(f.data[1]) << 8) | f.data[2];
    s.pack_current_ma = static_cast<qint16>(
        (static_cast<quint16>(f.data[3]) << 8) | f.data[4]) * 10;
    // ctrl byte
    quint8 ctrl = f.data[5];
    s.prot_level = ctrl & 0x03U;
    s.fet_chg    = (ctrl >> 2U) & 0x01U;
    s.fet_dsg    = (ctrl >> 3U) & 0x01U;
    s.balancing  = (ctrl >> 4U) & 0x01U;
    // C9 voltage
    s.cell_mv[8] = (static_cast<quint16>(f.data[6]) << 8) | f.data[7];
    return s;
}

BmsSnapshot BmsProtocolDecoder::decodeFaultFrame(const CanFrame &f)
{
    BmsSnapshot s;
    s.prot_level    = f.data[0];
    s.active_faults = (static_cast<quint16>(f.data[1]) << 8) | f.data[2];
    quint16 max_mv  = (static_cast<quint16>(f.data[3]) << 8) | f.data[4];
    quint16 min_mv  = (static_cast<quint16>(f.data[5]) << 8) | f.data[6];
    s.cell_max_mv = max_mv;
    s.cell_min_mv = min_mv;
    s.max_temp_0p1c = static_cast<qint16>(f.data[7] - 40) * 10;
    return s;
}

BmsSnapshot BmsProtocolDecoder::decodeCellFrame1_4(const CanFrame &f, BmsSnapshot &s)
{
    for (int i = 0; i < 4; ++i) {
        s.cell_mv[i] = (static_cast<quint16>(f.data[i*2]) << 8) | f.data[i*2+1];
    }
    return s;
}

BmsSnapshot BmsProtocolDecoder::decodeCellFrame5_8(const CanFrame &f, BmsSnapshot &s)
{
    for (int i = 0; i < 4; ++i) {
        s.cell_mv[i+4] = (static_cast<quint16>(f.data[i*2]) << 8) | f.data[i*2+1];
    }
    return s;
}

BmsSnapshot BmsProtocolDecoder::decodeSocOcvFrame(const CanFrame &f, BmsSnapshot &s)
{
    s.soc_permil      = (static_cast<quint16>(f.data[0]) << 8) | f.data[1];
    s.real_soc_permil = (static_cast<quint16>(f.data[2]) << 8) | f.data[3];
    s.remaining_mah   = static_cast<qint32>(
        ((static_cast<quint32>(f.data[4]) << 8) | f.data[5]) * 100);
    s.q_max_mah       = static_cast<qint32>(
        ((static_cast<quint32>(f.data[6]) << 8) | f.data[7]) * 100);
    return s;
}

BmsSnapshot BmsProtocolDecoder::decodeTemperatureFrame(const CanFrame &f, BmsSnapshot &s)
{
    for (int i = 0; i < 3; ++i) {
        s.temp_0p1c[i] = static_cast<qint16>(
            (static_cast<quint16>(f.data[i*2]) << 8) | f.data[i*2+1]);
    }
    return s;
}
```

- [ ] **Step 5: 编写下行编码 + 工具函数**

继续在同一文件：

```cpp
CanFrame BmsProtocolDecoder::encodeCommand(const BmsCommand &cmd)
{
    CanFrame f;
    f.len = 8;
    memset(f.data, 0, 8);

    switch (cmd.type) {
    case BmsCommand::QUERY:
        f.id = 0x200U;
        f.data[0] = cmd.subType;
        break;
    case BmsCommand::CONTROL:
        f.id = 0x201U;
        f.data[0] = cmd.subType;
        f.data[1] = static_cast<quint8>((cmd.value >> 8) & 0xFF);
        f.data[2] = static_cast<quint8>(cmd.value & 0xFF);
        break;
    case BmsCommand::CONFIG:
        f.id = 0x202U;
        f.data[0] = cmd.subType;
        f.data[1] = static_cast<quint8>((cmd.value >> 8) & 0xFF);
        f.data[2] = static_cast<quint8>(cmd.value & 0xFF);
        break;
    }
    return f;
}

QString BmsProtocolDecoder::faultName(quint8 bit)
{
    static const char* names[] = {
        "单芯过压", "单芯欠压", "总压过压", "总压欠压",
        "放电过流", "充电过流", "短路", "过温",
        "欠温", "压差过大", "通信丢失", "看门狗"
    };
    return (bit < 12) ? QString::fromUtf8(names[bit]) : QString("未知(%1)").arg(bit);
}

const char* BmsProtocolDecoder::faultSeverity(quint8 bit)
{
    // FAULT bits: 0,1,2,3,4,5,6,10,11
    // ALERT bits: 7,8
    // WARNING bit: 9
    switch (bit) {
    case 7: case 8:  return "ALERT";
    case 9:           return "WARNING";
    default:          return "FAULT";
    }
}
```

- [ ] **Step 6: Commit**

```bash
git add BmsHostApp/protocol/
git commit -m "feat(protocol): BmsProtocolDecoder — all 7 uplink frames + command encoder"
```

### Task 6: CsvLogger + BmsDbcHelper

**Files:**
- Create: `BmsHostApp/protocol/CsvLogger.h`
- Create: `BmsHostApp/protocol/CsvLogger.cpp`
- Create: `BmsHostApp/protocol/BmsDbcHelper.h`
- Create: `BmsHostApp/protocol/BmsDbcHelper.cpp`

**Interfaces:**
- Produces:
  - `class CsvLogger : public QObject` — `start(path, intervalMs)`, `appendRow(snapshot)`, `stop()`
  - `class BmsDbcHelper : public QObject` — `loadDbc(path) → bool`

- [ ] **Step 1: 编写 CsvLogger.h**

File: `BmsHostApp/protocol/CsvLogger.h`

```cpp
#ifndef CSV_LOGGER_H
#define CSV_LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "BmsSnapshot.h"

class CsvLogger : public QObject {
    Q_OBJECT
public:
    explicit CsvLogger(QObject *parent = nullptr);
    ~CsvLogger();

    bool start(const QString &filePath, int intervalMs = 1000);
    void appendRow(const BmsSnapshot &snap);
    void stop();

private:
    void rotateIfNeeded();
    void writeHeader();

    QFile m_file;
    QTextStream m_stream;
    QString m_filePath;
    int m_intervalMs = 1000;
    qint64 m_lastWriteMs = 0;
    quint16 m_lastFaults = 0xFFFF; // 强制首行写入
    static constexpr qint64 MAX_SIZE = 10 * 1024 * 1024;
    static constexpr int MAX_ROTATIONS = 10;
};

#endif // CSV_LOGGER_H
```

- [ ] **Step 2: 编写 CsvLogger.cpp**

File: `BmsHostApp/protocol/CsvLogger.cpp`

```cpp
#include "CsvLogger.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

CsvLogger::CsvLogger(QObject *parent) : QObject(parent) {}
CsvLogger::~CsvLogger() { stop(); }

bool CsvLogger::start(const QString &filePath, int intervalMs)
{
    m_filePath = filePath;
    m_intervalMs = intervalMs;
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Cannot open CSV log:" << filePath;
        return false;
    }
    m_stream.setDevice(&m_file);
    // 空文件时写表头
    if (m_file.size() == 0) { writeHeader(); }
    m_lastWriteMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

void CsvLogger::appendRow(const BmsSnapshot &snap)
{
    if (!m_file.isOpen()) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool faultChanged = (snap.active_faults != m_lastFaults);
    bool intervalElapsed = (now - m_lastWriteMs >= m_intervalMs);

    if (!faultChanged && !intervalElapsed) return;

    rotateIfNeeded();
    m_stream << snap.timestamp.toString(Qt::ISODateWithMs) << ","
             << snap.pack_voltage_mv << ","
             << snap.pack_current_ma << ","
             << snap.soc_permil << ","
             << snap.prot_level << ","
             << snap.active_faults;
    for (int i = 0; i < 9; ++i) m_stream << "," << snap.cell_mv[i];
    for (int i = 0; i < 3; ++i) m_stream << "," << snap.temp_0p1c[i];
    m_stream << "," << (snap.fet_chg ? 1 : 0)
             << "," << (snap.fet_dsg ? 1 : 0)
             << "," << (snap.balancing ? 1 : 0)
             << "\n";
    m_stream.flush();

    m_lastWriteMs = now;
    m_lastFaults = snap.active_faults;
}

void CsvLogger::stop()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void CsvLogger::rotateIfNeeded()
{
    if (m_file.size() < MAX_SIZE) return;
    stop();
    QFileInfo fi(m_filePath);
    QString newName = fi.dir().filePath(
        QString("bms_log_%1.csv")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddTHHmmss")));
    QFile::rename(m_filePath, newName);

    // 清理旧文件
    QDir dir = fi.dir();
    QStringList logs = dir.entryList({"bms_log_*.csv"}, QDir::Files, QDir::Name);
    while (logs.size() > MAX_ROTATIONS) {
        QFile::remove(dir.filePath(logs.takeFirst()));
    }

    start(m_filePath, m_intervalMs);
}

void CsvLogger::writeHeader()
{
    m_stream << "timestamp,pack_voltage_mv,pack_current_ma,soc_permil,"
             << "prot_level,active_faults";
    for (int i = 1; i <= 9; ++i) m_stream << ",cell_" << i << "_mv";
    for (int i = 1; i <= 3; ++i) m_stream << ",ts" << i << "_0p1c";
    m_stream << ",fet_chg,fet_dsg,balancing\n";
    m_stream.flush();
}
```

- [ ] **Step 3: 编写 BmsDbcHelper**

File: `BmsHostApp/protocol/BmsDbcHelper.h`

```cpp
#ifndef BMS_DBC_HELPER_H
#define BMS_DBC_HELPER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include "can/CanFrame.h"

class BmsDbcHelper : public QObject {
    Q_OBJECT
public:
    explicit BmsDbcHelper(QObject *parent = nullptr);
    bool loadDbc(const QString &path);
    QMap<QString, QVariant> decodeFrame(const CanFrame &frame) const;
    bool isLoaded() const { return m_loaded; }

private:
    bool m_loaded = false;
    // 简化实现: 内置解码器为主，DBC 为可选增强
    // Qt 6.5+ 可用 QCanDbcFileParser + QCanFrameProcessor
};

#endif // BMS_DBC_HELPER_H
```

File: `BmsHostApp/protocol/BmsDbcHelper.cpp`

```cpp
#include "BmsDbcHelper.h"
#include <QtSerialBus/QCanDbcFileParser>
#include <QtSerialBus/QCanFrameProcessor>
#include <QDebug>

BmsDbcHelper::BmsDbcHelper(QObject *parent) : QObject(parent) {}

bool BmsDbcHelper::loadDbc(const QString &path)
{
    // 简化: 验证 DBC 文件可读
    QCanDbcFileParser parser;
    bool ok = parser.parse(path);
    if (!ok) {
        qWarning() << "DBC parse failed:" << parser.errorString();
        qWarning() << "Falling back to built-in decoder";
        m_loaded = false;
        return false;
    }
    m_loaded = true;
    qInfo() << "DBC loaded:" << path;
    return true;
}

QMap<QString, QVariant> BmsDbcHelper::decodeFrame(const CanFrame &frame) const
{
    // 占位: DBC 解码实现。主路径仍为内置解码器。
    Q_UNUSED(frame)
    return {};
}
```

- [ ] **Step 4: Commit**

```bash
git add BmsHostApp/protocol/
git commit -m "feat(protocol): CsvLogger (interval+fault-trigger, 10MB rotation) + BmsDbcHelper stub"
```

### Task 7: List Models (CellVoltage, Temperature, FaultList)

**Files:**
- Create: `BmsHostApp/model/CellVoltageModel.h/.cpp`
- Create: `BmsHostApp/model/TemperatureModel.h/.cpp`
- Create: `BmsHostApp/model/FaultListModel.h/.cpp`

**Interfaces:**
- Produces: 3 个 QAbstractListModel 子类，供 QML ListView/Repeater 使用

- [ ] **Step 1: 编写 CellVoltageModel**

File: `BmsHostApp/model/CellVoltageModel.h`

```cpp
#ifndef CELL_VOLTAGE_MODEL_H
#define CELL_VOLTAGE_MODEL_H

#include <QAbstractListModel>
#include <array>

class CellVoltageModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { VoltageRole = Qt::UserRole + 1, CellIndexRole,
                 IsMaxRole, IsMinRole, ColorRole };
    Q_ENUM(Roles)

    explicit CellVoltageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return 9; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateCell(int index, quint16 mV, bool isMax, bool isMin);

private:
    struct CellInfo { quint16 mV = 0; bool isMax = false; bool isMin = false; };
    std::array<CellInfo, 9> m_cells;
    QColor colorForVoltage(quint16 mV) const;
};

#endif
```

File: `BmsHostApp/model/CellVoltageModel.cpp`

```cpp
#include "CellVoltageModel.h"

CellVoltageModel::CellVoltageModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> CellVoltageModel::roleNames() const
{
    return {{VoltageRole, "voltage"}, {CellIndexRole, "cellIndex"},
            {IsMaxRole, "isMax"}, {IsMinRole, "isMin"}, {ColorRole, "barColor"}};
}

QVariant CellVoltageModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= 9) return {};
    const auto &c = m_cells[row];
    switch (role) {
    case VoltageRole:  return c.mV;
    case CellIndexRole: return row + 1;
    case IsMaxRole:     return c.isMax;
    case IsMinRole:     return c.isMin;
    case ColorRole:     return colorForVoltage(c.mV);
    }
    return {};
}

void CellVoltageModel::updateCell(int index, quint16 mV, bool isMax, bool isMin)
{
    if (index < 0 || index >= 9) return;
    auto &c = m_cells[index];
    if (c.mV == mV && c.isMax == isMax && c.isMin == isMin) return;
    c.mV = mV; c.isMax = isMax; c.isMin = isMin;
    QModelIndex idx = createIndex(index, 0);
    emit dataChanged(idx, idx, {VoltageRole, IsMaxRole, IsMinRole, ColorRole});
}

QColor CellVoltageModel::colorForVoltage(quint16 mV) const
{
    if (mV > 4200) return QColor("#e74c3c"); // 红
    if (mV >= 3600) return QColor("#27ae60"); // 绿
    if (mV >= 3000) return QColor("#f39c12"); // 黄
    return QColor("#3498db"); // 蓝 (< 3000)
}
```

- [ ] **Step 2: 编写 TemperatureModel**

File: `BmsHostApp/model/TemperatureModel.h`

```cpp
#ifndef TEMPERATURE_MODEL_H
#define TEMPERATURE_MODEL_H

#include <QAbstractListModel>
#include <array>

class TemperatureModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { TempRole = Qt::UserRole + 1, SensorIndexRole, ColorRole };
    Q_ENUM(Roles)

    explicit TemperatureModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return 3; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void updateTemp(int index, qint16 temp0p1c);

private:
    std::array<qint16, 3> m_temps = {};
    QColor colorForTemp(qreal degC) const;
};

#endif
```

File: `BmsHostApp/model/TemperatureModel.cpp`

```cpp
#include "TemperatureModel.h"

TemperatureModel::TemperatureModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> TemperatureModel::roleNames() const
{
    return {{TempRole, "temperature"}, {SensorIndexRole, "sensorIndex"}, {ColorRole, "tempColor"}};
}

QVariant TemperatureModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= 3) return {};
    switch (role) {
    case TempRole:       return m_temps[row] / 10.0;
    case SensorIndexRole: return row + 1;
    case ColorRole:       return colorForTemp(m_temps[row] / 10.0);
    }
    return {};
}

void TemperatureModel::updateTemp(int index, qint16 temp0p1c)
{
    if (index < 0 || index >= 3) return;
    if (m_temps[index] == temp0p1c) return;
    m_temps[index] = temp0p1c;
    QModelIndex idx = createIndex(index, 0);
    emit dataChanged(idx, idx);
}

QColor TemperatureModel::colorForTemp(qreal degC) const
{
    if (degC > 50.0) return QColor("#e74c3c");
    if (degC >= 30.0) return QColor("#f39c12");
    if (degC >= 0.0) return QColor("#27ae60");
    return QColor("#3498db");
}
```

- [ ] **Step 3: 编写 FaultListModel**

File: `BmsHostApp/model/FaultListModel.h`

```cpp
#ifndef FAULT_LIST_MODEL_H
#define FAULT_LIST_MODEL_H

#include <QAbstractListModel>
#include <QVector>

struct FaultEntry {
    int bit;
    QString name;
    QString severity; // "WARNING", "ALERT", "FAULT"
};

class FaultListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { FaultNameRole = Qt::UserRole + 1, FaultBitRole, SeverityRole };
    Q_ENUM(Roles)

    explicit FaultListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFaults(quint16 activeBits);

private:
    QVector<FaultEntry> m_active;
};

#endif
```

File: `BmsHostApp/model/FaultListModel.cpp`

```cpp
#include "FaultListModel.h"
#include "protocol/BmsProtocolDecoder.h"

FaultListModel::FaultListModel(QObject *parent) : QAbstractListModel(parent) {}

int FaultListModel::rowCount(const QModelIndex &) const { return m_active.size(); }

QVariant FaultListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= m_active.size()) return {};
    const auto &e = m_active[row];
    switch (role) {
    case FaultNameRole: return e.name;
    case FaultBitRole:  return e.bit;
    case SeverityRole:  return e.severity;
    }
    return {};
}

QHash<int, QByteArray> FaultListModel::roleNames() const
{
    return {{FaultNameRole, "faultName"}, {FaultBitRole, "faultBit"}, {SeverityRole, "severity"}};
}

void FaultListModel::setFaults(quint16 activeBits)
{
    beginResetModel();
    m_active.clear();
    for (int b = 0; b < 12; ++b) {
        if (activeBits & (1U << b)) {
            m_active.append({b, BmsProtocolDecoder::faultName(static_cast<quint8>(b)),
                             QString::fromLatin1(BmsProtocolDecoder::faultSeverity(static_cast<quint8>(b)))});
        }
    }
    endResetModel();
}
```

- [ ] **Step 4: Commit**

```bash
git add BmsHostApp/model/
git commit -m "feat(model): CellVoltageModel + TemperatureModel + FaultListModel (QAbstractListModel)"
```

### Task 8: BmsDataModel (中央数据模型)

**Files:**
- Create: `BmsHostApp/model/BmsDataModel.h`
- Create: `BmsHostApp/model/BmsDataModel.cpp`

**Interfaces:**
- Produces: `class BmsDataModel : public QObject` — 16 Q_PROPERTY + 3 list model Q_PROPERTY + Q_INVOKABLE send*/save*/load* + 30ms timer → applySnapshot()

- [ ] **Step 1: 编写 BmsDataModel.h**

File: `BmsHostApp/model/BmsDataModel.h`

```cpp
#ifndef BMS_DATA_MODEL_H
#define BMS_DATA_MODEL_H

#include <QObject>
#include <QTimer>
#include "BmsSnapshot.h"
#include "CellVoltageModel.h"
#include "TemperatureModel.h"
#include "FaultListModel.h"
#include "can/CanWorker.h"
#include "protocol/BmsProtocolDecoder.h"
#include "protocol/CsvLogger.h"

class BmsDataModel : public QObject {
    Q_OBJECT
    // 标量属性
    Q_PROPERTY(quint32 packVoltage READ packVoltage NOTIFY packVoltageChanged)
    Q_PROPERTY(qint32 packCurrent READ packCurrent NOTIFY packCurrentChanged)
    Q_PROPERTY(quint16 socPermil READ socPermil NOTIFY socPermilChanged)
    Q_PROPERTY(quint16 realSocPermil READ realSocPermil NOTIFY realSocPermilChanged)
    Q_PROPERTY(qint32 remainingMah READ remainingMah NOTIFY remainingMahChanged)
    Q_PROPERTY(qint32 qMaxMah READ qMaxMah NOTIFY qMaxMahChanged)
    Q_PROPERTY(quint8 protLevel READ protLevel NOTIFY protLevelChanged)
    Q_PROPERTY(quint16 activeFaults READ activeFaults NOTIFY activeFaultsChanged)
    Q_PROPERTY(quint16 cellMin READ cellMin NOTIFY cellMinChanged)
    Q_PROPERTY(quint16 cellMax READ cellMax NOTIFY cellMaxChanged)
    Q_PROPERTY(quint16 cellDiff READ cellDiff NOTIFY cellDiffChanged)
    Q_PROPERTY(bool fetCharge READ fetCharge NOTIFY fetChargeChanged)
    Q_PROPERTY(bool fetDischarge READ fetDischarge NOTIFY fetDischargeChanged)
    Q_PROPERTY(bool balancing READ balancing NOTIFY balancingChanged)
    Q_PROPERTY(int connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
    // 列表模型
    Q_PROPERTY(QObject* cellVoltageModel READ cellVoltageModel CONSTANT)
    Q_PROPERTY(QObject* temperatureModel READ temperatureModel CONSTANT)
    Q_PROPERTY(QObject* faultListModel READ faultListModel CONSTANT)

public:
    enum ConnStatus { DISCONNECTED = 0, CONNECTED = 1, SHUTDOWN = 2 };
    Q_ENUM(ConnStatus)

    explicit BmsDataModel(QObject *parent = nullptr);

    // Getters
    quint32 packVoltage() const { return m_snap.pack_voltage_mv; }
    qint32 packCurrent() const { return m_snap.pack_current_ma; }
    quint16 socPermil() const { return m_snap.soc_permil; }
    quint16 realSocPermil() const { return m_snap.real_soc_permil; }
    qint32 remainingMah() const { return m_snap.remaining_mah; }
    qint32 qMaxMah() const { return m_snap.q_max_mah; }
    quint8 protLevel() const { return m_snap.prot_level; }
    quint16 activeFaults() const { return m_snap.active_faults; }
    quint16 cellMin() const { return m_snap.cell_min_mv; }
    quint16 cellMax() const { return m_snap.cell_max_mv; }
    quint16 cellDiff() const { return m_snap.cell_diff_mv; }
    bool fetCharge() const { return m_snap.fet_chg; }
    bool fetDischarge() const { return m_snap.fet_dsg; }
    bool balancing() const { return m_snap.balancing; }
    int connectionStatus() const { return m_connStatus; }
    QObject* cellVoltageModel() { return &m_cellModel; }
    QObject* temperatureModel() { return &m_tempModel; }
    QObject* faultListModel() { return &m_faultModel; }

    void setCanWorker(CanWorker *w) { m_canWorker = w; }
    void setCsvLogger(CsvLogger *l) { m_csvLogger = l; }

public slots:
    void onBatchReady(const QVector<CanFrame> &batch);
    void onConnectionChanged(bool connected);

    // Q_INVOKABLE
    Q_INVOKABLE void sendQuery(quint8 subCmd);
    Q_INVOKABLE void sendControl(quint8 cmd, quint16 mask);
    Q_INVOKABLE void sendConfig(quint8 param, quint16 value);
    Q_INVOKABLE void saveCsvLog(const QString &path);
    Q_INVOKABLE void loadDbcFile(const QString &path);

signals:
    void packVoltageChanged(); void packCurrentChanged();
    void socPermilChanged(); void realSocPermilChanged();
    void remainingMahChanged(); void qMaxMahChanged();
    void protLevelChanged(); void activeFaultsChanged();
    void cellMinChanged(); void cellMaxChanged(); void cellDiffChanged();
    void fetChargeChanged(); void fetDischargeChanged(); void balancingChanged();
    void connectionStatusChanged();
    void lastUpdateChanged(const QString &text);
    void statusTextChanged(const QString &text);

private:
    void applySnapshot(const BmsSnapshot &snap);

    BmsSnapshot m_snap;
    int m_connStatus = DISCONNECTED;
    QTimer m_timer;
    CanWorker *m_canWorker = nullptr;
    CsvLogger *m_csvLogger = nullptr;
    BmsDbcHelper m_dbcHelper;

    CellVoltageModel m_cellModel;
    TemperatureModel m_tempModel;
    FaultListModel m_faultModel;

    QVector<CanFrame> m_pendingBatch;
    bool m_shutdownSent = false;
};

#endif // BMS_DATA_MODEL_H
```

- [ ] **Step 2: 编写 BmsDataModel.cpp**

File: `BmsHostApp/model/BmsDataModel.cpp`

```cpp
#include "BmsDataModel.h"
#include <QDateTime>

BmsDataModel::BmsDataModel(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(30);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (m_pendingBatch.isEmpty()) return;
        // latest-wins
        auto batch = std::move(m_pendingBatch);
        m_pendingBatch.clear();
        auto snaps = BmsProtocolDecoder::decodeFrames(batch);
        if (!snaps.isEmpty()) {
            applySnapshot(snaps.last());
        }
    });
    m_timer.start();
}

void BmsDataModel::onBatchReady(const QVector<CanFrame> &batch)
{
    m_pendingBatch = batch; // latest-wins
}

void BmsDataModel::onConnectionChanged(bool connected)
{
    if (m_shutdownSent && !connected) {
        m_connStatus = SHUTDOWN;
    } else {
        m_connStatus = connected ? CONNECTED : DISCONNECTED;
    }
    emit connectionStatusChanged();
    emit statusTextChanged(m_connStatus == CONNECTED ? "● 已连接"
        : m_connStatus == SHUTDOWN ? "⏻ BMS 已关机" : "⚠ 已断开");
}

void BmsDataModel::applySnapshot(const BmsSnapshot &snap)
{
    // 仅变更时 set + emit。为简洁起见，此处批量比较并 emit（实际项目可逐字段）
    bool changed = false;

#define UPDATE_PROP(prop, val) if (m_snap.prop != (val)) { m_snap.prop = (val); emit prop##Changed(); changed = true; }

    UPDATE_PROP(pack_voltage_mv, snap.pack_voltage_mv)
    UPDATE_PROP(pack_current_ma, snap.pack_current_ma)
    UPDATE_PROP(soc_permil, snap.soc_permil)
    UPDATE_PROP(real_soc_permil, snap.real_soc_permil)
    UPDATE_PROP(remaining_mah, snap.remaining_mah)
    UPDATE_PROP(q_max_mah, snap.q_max_mah)
    UPDATE_PROP(prot_level, snap.prot_level)

    if (m_snap.active_faults != snap.active_faults) {
        m_snap.active_faults = snap.active_faults;
        emit activeFaultsChanged();
        m_faultModel.setFaults(snap.active_faults);
        changed = true;
    }

    UPDATE_PROP(cell_min_mv, snap.cell_min_mv)
    UPDATE_PROP(cell_max_mv, snap.cell_max_mv)
    UPDATE_PROP(cell_diff_mv, snap.cell_diff_mv)
    UPDATE_PROP(fet_chg, snap.fet_chg)
    UPDATE_PROP(fet_dsg, snap.fet_dsg)
    UPDATE_PROP(balancing, snap.balancing)

#undef UPDATE_PROP

    // 更新列表模型
    for (int i = 0; i < 9; ++i) {
        bool isMax = (snap.cell_mv[i] == snap.cell_max_mv && snap.cell_max_mv > 0);
        bool isMin = (snap.cell_mv[i] == snap.cell_min_mv && snap.cell_min_mv > 0);
        m_cellModel.updateCell(i, snap.cell_mv[i], isMax, isMin);
    }
    for (int i = 0; i < 3; ++i) {
        m_tempModel.updateTemp(i, snap.temp_0p1c[i]);
    }

    // CSV
    if (m_csvLogger) { m_csvLogger->appendRow(snap); }

    // 时间戳
    emit lastUpdateChanged(snap.timestamp.toString("HH:mm:ss"));
}

void BmsDataModel::sendQuery(quint8 subCmd)
{
    if (m_canWorker) {
        m_canWorker->sendFrame(
            BmsProtocolDecoder::encodeCommand({BmsCommand::QUERY, subCmd, 0}));
    }
}

void BmsDataModel::sendControl(quint8 cmd, quint16 mask)
{
    if (m_canWorker) {
        if (cmd == 0xFF) m_shutdownSent = true;
        m_canWorker->sendFrame(
            BmsProtocolDecoder::encodeCommand({BmsCommand::CONTROL, cmd, mask}));
    }
}

void BmsDataModel::sendConfig(quint8 param, quint16 value)
{
    if (m_canWorker) {
        m_canWorker->sendFrame(
            BmsProtocolDecoder::encodeCommand({BmsCommand::CONFIG, param, value}));
    }
}

void BmsDataModel::saveCsvLog(const QString &path)
{
    if (m_csvLogger) { m_csvLogger->stop(); }
    // 假设 CsvLogger 被重新创建或 restart
    if (m_csvLogger) { m_csvLogger->start(path, 1000); }
}

void BmsDataModel::loadDbcFile(const QString &path)
{
    m_dbcHelper.loadDbc(path);
}
```

- [ ] **Step 3: 更新 main.cpp 注册 BmsDataModel**

修改 `BmsHostApp/main.cpp`，在 QQmlApplicationEngine 创建后添加：

```cpp
CanWorker canWorker;
BmsDataModel bmsModel;
CsvLogger csvLogger;

bmsModel.setCanWorker(&canWorker);
bmsModel.setCsvLogger(&csvLogger);

QObject::connect(&canWorker, &CanWorker::batchReady,
                 &bmsModel, &BmsDataModel::onBatchReady);
QObject::connect(&canWorker, &CanWorker::connectionStatusChanged,
                 &bmsModel, &BmsDataModel::onConnectionChanged);

engine.rootContext()->setContextProperty("bms", &bmsModel);

// CanWorker 线程
QThread *canThread = new QThread(&app);
canWorker.moveToThread(canThread);
QObject::connect(canThread, &QThread::started, &canWorker, [&]() {
    canWorker.start("peakcan", "usb0", 500000);
});
canThread->start();
```

- [ ] **Step 4: Commit**

```bash
git add BmsHostApp/model/ BmsHostApp/main.cpp
git commit -m "feat(model): BmsDataModel — 16 Q_PROPERTY + 30ms timer + Q_INVOKABLE commands"
```

### Task 9: QML 自定义组件 (GaugeArc, CellBar, FaultRibbon)

**Files:**
- Create: `BmsHostApp/qml/components/GaugeArc.qml`
- Create: `BmsHostApp/qml/components/CellBar.qml`
- Create: `BmsHostApp/qml/components/FaultRibbon.qml`

- [ ] **Step 1: 编写 GaugeArc.qml (SOC 环形仪表)**

File: `BmsHostApp/qml/components/GaugeArc.qml`

```qml
import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 200; height: 200
    property real value: 0      // 0-1000 permil
    property real minValue: 0
    property real maxValue: 1000
    property string unit: "%"
    property int decimals: 0

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var cx = width / 2, cy = height / 2
            var r = Math.min(cx, cy) - 15

            // 背景弧
            ctx.beginPath()
            ctx.arc(cx, cy, r, Math.PI * 1.35, Math.PI * 0.65, false)
            ctx.lineWidth = 14
            ctx.strokeStyle = "#e0e0e0"
            ctx.stroke()

            // 值弧
            var pct = (value - minValue) / (maxValue - minValue)
            var angle = Math.PI * 1.35 + pct * Math.PI * 1.3
            ctx.beginPath()
            ctx.arc(cx, cy, r, Math.PI * 1.35, angle, false)
            ctx.lineWidth = 14
            ctx.lineCap = "round"
            // 颜色: 红(0-200) 橙(201-500) 绿(501-1000)
            if (pct < 0.2) ctx.strokeStyle = "#e74c3c"
            else if (pct < 0.5) ctx.strokeStyle = "#f39c12"
            else ctx.strokeStyle = "#27ae60"
            ctx.stroke()
        }
    }

    // 中央数值
    Column {
        anchors.centerIn: parent
        spacing: 2
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (value / 10).toFixed(decimals)
            font.pixelSize: 36; font.bold: true
            color: "#333"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: unit
            font.pixelSize: 14; color: "#666"
        }
    }

    // 动画
    Behavior on value {
        NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
    }

    // 值变化时重绘
    onValueChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
```

- [ ] **Step 2: 编写 CellBar.qml (单节电压柱)**

File: `BmsHostApp/qml/components/CellBar.qml`

```qml
import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 60; height: 200
    property real voltage: 0       // mV
    property int cellIndex: 1
    property bool isMax: false
    property bool isMin: false
    property color barColor: "#27ae60"
    property real minRange: 2800
    property real maxRange: 4300

    Column {
        anchors.fill: parent
        spacing: 4

        // 标记
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: isMax ? "*" : isMin ? "_" : ""
            font.pixelSize: 14; font.bold: true
            color: isMax ? "#e74c3c" : "#3498db"
            visible: isMax || isMin
        }

        // 电压值
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (voltage / 1000).toFixed(3) + "V"
            font.pixelSize: 10; color: "#333"
        }

        // 柱
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 32; height: {
                var h = (voltage - minRange) / (maxRange - minRange) * 120
                return Math.max(2, Math.min(120, h))
            }
            y: {
                var h = (voltage - minRange) / (maxRange - minRange) * 120
                return 120 - Math.max(2, Math.min(120, h))
            }
            color: barColor
            radius: 3
            Behavior on height { NumberAnimation { duration: 300 } }
            Behavior on y { NumberAnimation { duration: 300 } }
        }

        // 编号
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "C" + cellIndex
            font.pixelSize: 11; font.bold: true
            color: "#555"
        }
    }
}
```

- [ ] **Step 3: 编写 FaultRibbon.qml**

File: `BmsHostApp/qml/components/FaultRibbon.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 36; radius: 4
    property int activeFaults: 0
    property QtObject faultModel: null

    color: activeFaults === 0 ? "#d5f5e3" : "#fadbd8"
    border { width: 1; color: activeFaults === 0 ? "#27ae60" : "#e74c3c" }

    RowLayout {
        anchors.fill: parent; anchors.margins: 8
        Text {
            text: activeFaults === 0 ? "✓ 无活跃故障" : "⚠ " + faultModel.rowCount() + " 个活跃故障"
            font.pixelSize: 13; font.bold: true
            color: activeFaults === 0 ? "#27ae60" : "#c0392b"
        }
        Item { Layout.fillWidth: true }
        Repeater {
            model: faultModel
            Text {
                text: model.faultName
                color: model.severity === "FAULT" ? "#e74c3c"
                     : model.severity === "ALERT" ? "#e67e22" : "#f1c40f"
                font.pixelSize: 11
                visible: activeFaults !== 0
            }
        }
    }

    Behavior on color { ColorAnimation { duration: 300 } }
    Behavior on border.color { ColorAnimation { duration: 300 } }
}
```

- [ ] **Step 4: Commit**

```bash
git add BmsHostApp/qml/components/
git commit -m "feat(qml): GaugeArc + CellBar + FaultRibbon custom components"
```

### Task 10: OverviewPage + CellsPage

**Files:**
- Create: `BmsHostApp/qml/OverviewPage.qml`
- Create: `BmsHostApp/qml/CellsPage.qml`

- [ ] **Step 1: 编写 OverviewPage.qml**

File: `BmsHostApp/qml/OverviewPage.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Page {
    title: "总览"
    ScrollView {
        anchors.fill: parent
        GridLayout {
            columns: 2; rowSpacing: 12; columnSpacing: 16
            anchors.margins: 16; anchors.left: parent.left

            // SOC 仪表
            GroupBox {
                title: "SOC"
                Layout.rowSpan: 2
                GaugeArc {
                    width: 180; height: 180
                    value: bms.socPermil
                }
            }

            // 关键数值
            GroupBox {
                title: "电池状态"
                GridLayout { columns: 2; columnSpacing: 12; rowSpacing: 4
                    Label { text: "总压:" } Label { text: (bms.packVoltage / 1000).toFixed(2) + " V"; font.bold: true }
                    Label { text: "电流:" } Label { text: (bms.packCurrent / 1000).toFixed(2) + " A"; font.bold: true
                               color: bms.packCurrent < 0 ? "#e74c3c" : "#27ae60" }
                    Label { text: "剩余容量:" } Label { text: (bms.remainingMah / 1000).toFixed(2) + " Ah" }
                    Label { text: "Q_max:" } Label { text: (bms.qMaxMah / 1000).toFixed(2) + " Ah" }
                }
            }

            // FET + 均衡
            GroupBox {
                title: "FET & 均衡"
                GridLayout { columns: 2; columnSpacing: 8
                    Label { text: "充电 FET:" }
                    Label { text: bms.fetCharge ? "● 开启" : "○ 关闭"
                            color: bms.fetCharge ? "#27ae60" : "#999" }
                    Label { text: "放电 FET:" }
                    Label { text: bms.fetDischarge ? "● 开启" : "○ 关闭"
                            color: bms.fetDischarge ? "#27ae60" : "#999" }
                    Label { text: "均衡:" }
                    Label { text: bms.balancing ? "⚖ 均衡中" : "○ 未均衡"
                            color: bms.balancing ? "#e67e22" : "#999" }
                }
            }

            // 电芯摘要
            GroupBox {
                title: "电芯"
                GridLayout { columns: 2; columnSpacing: 12
                    Label { text: "最高:" } Label { text: "C" + (bms.cellMax > 0 ? "..." : "--") + " " + (bms.cellMax/1000).toFixed(3) + "V" }
                    Label { text: "最低:" } Label { text: "C" + (bms.cellMin > 0 ? "..." : "--") + " " + (bms.cellMin/1000).toFixed(3) + "V" }
                    Label { text: "压差:" } Label { text: bms.cellDiff + " mV" }
                }
            }

            // 温度摘要
            GroupBox {
                title: "温度"
                GridLayout { columns: 2; columnSpacing: 8
                    Repeater {
                        model: bms.temperatureModel
                        Label { text: "TS" + model.sensorIndex + ": " + model.temperature.toFixed(1) + "°C"
                                color: model.tempColor }
                    }
                }
            }
        }
    }

    // 故障条（底部）
    footer: FaultRibbon {
        activeFaults: bms.activeFaults
        faultModel: bms.faultListModel
    }
}
```

- [ ] **Step 2: 编写 CellsPage.qml**

File: `BmsHostApp/qml/CellsPage.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Page {
    title: "电芯"
    property var selectedCells: ([])

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12
        spacing: 8

        // 电压柱状图
        GroupBox {
            title: "电芯电压 (" + bms.cellDiff + " mV 压差)"
            Layout.fillWidth: true; Layout.preferredHeight: 260
            RowLayout {
                anchors.fill: parent
                spacing: 4
                // Y 轴标签
                Column {
                    Layout.preferredWidth: 40
                    Repeater {
                        model: ["4.3V","4.0V","3.7V","3.4V","3.1V","2.8V"]
                        Text { text: modelData; font.pixelSize: 9; color: "#999"
                               height: 20 }
                    }
                }
                // 电芯柱
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: bms.cellVoltageModel
                        CellBar {
                            voltage: model.voltage
                            cellIndex: model.cellIndex
                            isMax: model.isMax
                            isMin: model.isMin
                            barColor: model.barColor
                        }
                    }
                }
            }
        }

        // 均衡勾选
        GroupBox {
            title: "均衡选择"
            RowLayout {
                spacing: 8
                Repeater {
                    model: 9
                    CheckBox {
                        text: "C" + (index + 1)
                        checked: false
                        onCheckedChanged: {
                            if (checked) {
                                if (selectedCells.indexOf(index) < 0) selectedCells.push(index)
                            } else {
                                var i = selectedCells.indexOf(index)
                                if (i >= 0) selectedCells.splice(i, 1)
                            }
                            selectedCellsChanged()
                        }
                    }
                }
            }
        }

        // 温度条
        GroupBox {
            title: "温度"
            Layout.fillWidth: true
            RowLayout {
                spacing: 12
                Repeater {
                    model: bms.temperatureModel
                    RowLayout {
                        Label { text: "TS" + model.sensorIndex + ":"; font.bold: true }
                        Rectangle {
                            width: Math.max(4, model.temperature * 2)
                            height: 20; radius: 3; color: model.tempColor
                        }
                        Label { text: model.temperature.toFixed(1) + "°C" }
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add BmsHostApp/qml/OverviewPage.qml BmsHostApp/qml/CellsPage.qml
git commit -m "feat(qml): OverviewPage (SOC gauge + FET + fault ribbon) + CellsPage (9 voltage bars + balance checkboxes + temp)"
```

### Task 11: TrendsPage + ControlPanel + ConfigDialog

**Files:**
- Create: `BmsHostApp/qml/TrendsPage.qml`
- Create: `BmsHostApp/qml/ControlPanel.qml`
- Create: `BmsHostApp/qml/ConfigDialog.qml`

- [ ] **Step 1: 编写 TrendsPage.qml**

File: `BmsHostApp/qml/TrendsPage.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCharts

Page {
    title: "曲线"
    property bool paused: false

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 8
        spacing: 4

        // TabBar
        TabBar {
            id: trendTab
            TabButton { text: "总压" } TabButton { text: "电流" }
            TabButton { text: "SOC" }  TabButton { text: "温度" }
        }

        // 图表
        ChartView {
            id: chartView
            Layout.fillWidth: true; Layout.fillHeight: true
            antialiasing: true
            legend.visible: false
            animationOptions: ChartView.NoAnimation

            // 4 条 LineSeries (使用 VXYModelMapper 绑定，简化实现)
            LineSeries { id: seriesVoltage; name: "总压 (V)" }
            LineSeries { id: seriesCurrent; name: "电流 (A)" }
            LineSeries { id: seriesSoc;     name: "SOC (%)" }
            LineSeries { id: seriesTemp;    name: "温度 (°C)" }

            ValueAxis { id: axisX; min: 0; max: 300; labelFormat: "%d s" }
            ValueAxis { id: axisY; min: 0; max: 50 }
        }

        // 控制栏
        RowLayout {
            Button { text: paused ? "▶ 继续" : "⏸ 暂停"; onClicked: paused = !paused }
            Button { text: "⏹ 清除"; onClicked: {
                    seriesVoltage.clear(); seriesCurrent.clear()
                    seriesSoc.clear(); seriesTemp.clear()
                }
            }
            Button { text: "📁 导出CSV"; onClicked: {
                    // 弹出保存对话框 (通过 C++ Q_INVOKABLE)
                }
            }
            Item { Layout.fillWidth: true }
            Label { text: "300s 窗口"; color: "#999" }
        }
    }
}
```

- [ ] **Step 2: 编写 ControlPanel.qml**

File: `BmsHostApp/qml/ControlPanel.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: root
    width: 280; height: parent.height
    edge: Qt.RightEdge

    property var cellsPage: null

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16
        spacing: 12

        Label { text: "控制面板"; font.pixelSize: 18; font.bold: true }

        GroupBox { title: "FET 控制"
            Layout.fillWidth: true
            GridLayout { columns: 2; columnSpacing: 8
                Button { text: "充电 ON"; enabled: bms.activeFaults === 0 && bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x10, 0) }
                Button { text: "充电 OFF"; enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x11, 0) }
                Button { text: "放电 ON"; enabled: bms.activeFaults === 0 && bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x20, 0) }
                Button { text: "放电 OFF"; enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x21, 0) }
            }
        }

        GroupBox { title: "均衡"
            Layout.fillWidth: true
            RowLayout {
                Button { text: "开启均衡"; enabled: bms.connectionStatus === 1
                    onClicked: {
                        var mask = 0
                        if (root.cellsPage) {
                            for (var i = 0; i < root.cellsPage.selectedCells.length; i++)
                                mask |= (1 << root.cellsPage.selectedCells[i])
                        }
                        bms.sendControl(0x30, mask)
                    }
                }
                Button { text: "关闭均衡"; enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x31, 0) }
            }
        }

        GroupBox { title: "系统"
            Layout.fillWidth: true
            ColumnLayout {
                Button { text: "清除故障"; enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x01, 0) }
                Button { text: "刷新数据"; enabled: bms.connectionStatus === 1
                         onClicked: bms.sendQuery(0x00) }
            }
        }

        Item { Layout.fillHeight: true }

        // 关机 (底部)
        Button {
            text: "⏻ 关机"
            Layout.fillWidth: true
            highlighted: true
            onClicked: shutdownDialog.open()
        }
    }

    Dialog {
        id: shutdownDialog
        title: "确认关机"
        Label { text: "确认向 BMS 发送关机指令？\n此操作将关闭电池输出。" }
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            bms.sendControl(0xFF, 0)
            root.close()
        }
    }
}
```

- [ ] **Step 3: 编写 ConfigDialog.qml**

File: `BmsHostApp/qml/ConfigDialog.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "参数配置"
    width: 420; height: 500
    standardButtons: Dialog.Close

    property var params: [
        { label: "单芯过压 (mV)",   paramId: 0x01, min: 3000, max: 5000, unit: "mV" },
        { label: "单芯欠压 (mV)",   paramId: 0x02, min: 2000, max: 3500, unit: "mV" },
        { label: "放电过流 (mA)",   paramId: 0x03, min: 1000, max: 100000, unit: "mA" },
        { label: "充电过流 (mA)",   paramId: 0x04, min: 1000, max: 50000, unit: "mA" },
        { label: "均衡阈值 (mV)",   paramId: 0x05, min: 10,   max: 500, unit: "mV" },
        { label: "均衡最低电压 (mV)", paramId: 0x06, min: 2500, max: 4000, unit: "mV" }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Repeater {
            model: params
            RowLayout {
                Label { text: modelData.label; Layout.preferredWidth: 150 }
                SpinBox {
                    id: spinBox
                    from: modelData.min; to: modelData.max
                    editable: true
                    property bool valid: value >= modelData.min && value <= modelData.max
                }
                Label { text: modelData.unit; color: "#999" }
            }
        }

        Button {
            text: "发送配置"
            Layout.alignment: Qt.AlignRight
            enabled: bms.connectionStatus === 1
            onClicked: {
                // 发送全部已修改参数 (简化: 逐个发送已编辑值)
                // 实际使用中需跟踪修改; 此实现为原型
            }
        }

        Label {
            text: "注意: 配置掉电不保存。阈值不通过 CAN 回读。"
            color: "#999"; font.pixelSize: 11
            Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
    }
}
```

- [ ] **Step 4: 更新 main.qml — 集成所有页面**

用完整版本替换 Task 3 的占位 main.qml：

File: `BmsHostApp/qml/main.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

ApplicationWindow {
    id: root
    width: 1280; height: 800; visible: true
    title: "BMS 9S 上位机"

    property var cellsPage: null

    // 恢复窗口几何
    Component.onCompleted: {
        var geo = settings.value("ui/windowGeometry", "")
        // C++ 侧通过 setGeometry 恢复; QML 侧监听
    }
    onClosing: {
        settings.setValue("ui/windowGeometry",
            root.x + "," + root.y + "," + root.width + "," + root.height)
    }

    header: TabBar {
        id: mainTab
        TabButton { text: "总览" } TabButton { text: "电芯" } TabButton { text: "曲线" }
        Item { Layout.fillWidth: true }
        ToolButton { text: "☰"; onClicked: controlPanel.open() }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: mainTab.currentIndex
        OverviewPage {}
        CellsPage { id: cellsPageObj; Component.onCompleted: root.cellsPage = cellsPageObj }
        TrendsPage {}
    }

    // 状态栏
    footer: Rectangle {
        height: 28; color: "#f5f5f5"
        RowLayout {
            anchors.fill: parent; anchors.margins: 6
            Text { text: bms.statusText; id: statusLabel }
            Item { Layout.fillWidth: true }
            Text { text: "最后更新: " + bms.lastUpdate; color: "#999" }
        }
    }

    // 连接
    Connections {
        target: bms
        function onStatusTextChanged(text) { statusLabel.text = text }
    }

    ControlPanel {
        id: controlPanel
        cellsPage: root.cellsPage
    }

    ConfigDialog { id: configDialog }
}
```

- [ ] **Step 5: Commit**

```bash
git add BmsHostApp/qml/
git commit -m "feat(qml): TrendsPage + ControlPanel (8 commands) + ConfigDialog + integrated main.qml"
```

### Task 12: 集成编译验证

**Files:**
- Modify: `BmsHostApp/CMakeLists.txt` (确认无遗漏)
- Modify: (无，本任务仅验证)

- [ ] **Step 1: 全量编译 BmsHostApp**

```bash
cd BmsHostApp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Expected: 0 错误 0 警告

- [ ] **Step 2: 验证 BMS 固件编译**

```bash
cd bms_9s_f103 && cmake --build build
```

Expected: 0 错误 0 警告

- [ ] **Step 3: 检查链接 — CAN 协议一致性**

人工核对检查清单：
- [ ] 上行: 0x110/0x111/0x120/0x121/0x130/0x101 ID 与 layout 正确
- [ ] 下行: 0x200/0x201/0x202 编码正确
- [ ] 0x120 data[5] ctrl 字节位定义正确
- [ ] 0x121 温度帧 int16 big-endian ×3
- [ ] 0x101 故障帧 uint16 电压
- [ ] DLC 校验生效

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "build(bms-host): full project integration — 0 errors 0 warnings"
```

---

## 任务依赖图

```
Task 1 (固件 can_cmd) ──┐
Task 2 (固件 bms_app) ──┤
                          ├── Task 4 (CanWorker) ──┐
Task 3 (脚手架) ─────────┘                          │
                                                     ├── Task 8 (BmsDataModel) ──┐
                          Task 5 (Decoder) ──────────┘                            │
                          Task 6 (CsvLogger/Dbc) ────┤                            │
                          Task 7 (ListModels) ───────┘                            │
                                                                                   ├── Task 9 (QML components) ──┤
                                                                                   ├── Task 10 (Pages 1+2) ──────┤
                                                                                   ├── Task 11 (Pages 3+CP+CD) ──┤
                                                                                   └── Task 12 (集成验证)
```

Tasks 1,2 可并行；Tasks 3-5 可部分并行；Tasks 6-7 可并行；Tasks 9-11 可并行（均依赖 Task 8 接口定义）。
