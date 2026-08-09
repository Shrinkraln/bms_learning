#include "CanWorker.h"
#include <QDateTime>
#include <QDebug>

CanWorker::CanWorker(QObject *parent) : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setInterval(TIMEOUT_MS);
    connect(m_timeoutTimer, &QTimer::timeout, this, &CanWorker::checkTimeout);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(RECONNECT_MS);
    connect(m_reconnectTimer, &QTimer::timeout, this, &CanWorker::tryReconnect);
}

CanWorker::~CanWorker() { stop(); }

void CanWorker::start(const QString &plugin, const QString &interface, int bitrate)
{
    // 保存参数供重连使用
    m_plugin = plugin;
    m_interface = interface;
    m_bitrate = bitrate;

    if (m_device) { stop(); }

    // 诊断: 列出所有可用接口
    const auto devices = QCanBus::instance()->availableDevices(plugin, nullptr);
    QStringList ifaces;
    for (const auto &d : devices)
        ifaces.append(d.name());
    qInfo() << "CAN available devices (plugin=" << plugin << "):" << ifaces;

    QString errorStr;
    m_device = QCanBus::instance()->createDevice(plugin, interface, &errorStr);
    if (!m_device) {
        QString msg = QString("无法创建设备 '%1/%2': %3\n可用接口: %4")
            .arg(plugin, interface, errorStr,
                 ifaces.isEmpty() ? QString("(无)") : ifaces.join(", "));
        qWarning() << msg;
        emit errorOccurred(msg);
        emit deviceConnectedChanged(false);
        // 启动重连 — 等待设备插入
        if (!m_reconnectTimer->isActive()) {
            m_reconnectAttempt = 0;
            m_reconnectTimer->start();
        }
        return;
    }

    m_device->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);
    connect(m_device, &QCanBusDevice::framesReceived,
            this, &CanWorker::onFramesReceived);
    connect(m_device, &QCanBusDevice::errorOccurred,
            this, &CanWorker::onErrorOccurred);
    connect(m_device, &QCanBusDevice::stateChanged,
            this, &CanWorker::onDeviceStateChanged);

    if (!m_device->connectDevice()) {
        QString msg = QString("无法连接 '%1/%2': %3\n(设备被占用或未插入; 可用: %4)")
            .arg(plugin, interface, m_device->errorString(),
                 ifaces.isEmpty() ? QString("(无)") : ifaces.join(", "));
        qWarning() << msg;
        emit errorOccurred(msg);
        emit deviceConnectedChanged(false);
        delete m_device;
        m_device = nullptr;
        // 启动重连
        if (!m_reconnectTimer->isActive()) {
            m_reconnectAttempt = 0;
            m_reconnectTimer->start();
        }
        return;
    }

    // 连接成功
    qInfo() << "CAN device connected:" << plugin << interface << bitrate;
    m_reconnectTimer->stop();
    m_reconnectAttempt = 0;
    m_timedOut = false;
    m_deviceConnected = true;
    m_timeoutTimer->start();
    emit deviceConnectedChanged(true);
    emit connectionStatusChanged(true);
}

void CanWorker::stop()
{
    m_reconnectTimer->stop();
    m_timedOut = false;
    m_timeoutTimer->stop();
    if (m_device) {
        m_device->disconnectDevice();
        delete m_device;
        m_device = nullptr;
    }
    if (m_deviceConnected) {
        m_deviceConnected = false;
        emit deviceConnectedChanged(false);
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
        if (m_timedOut) {
            m_timedOut = false;
            emit connectionStatusChanged(true);
        }
        emit batchReady(batch);
    }
}

void CanWorker::onErrorOccurred(QCanBusDevice::CanBusError error)
{
    if (!m_device) return;
    // 设备级错误 (USB 拔除) — 触发硬件丢失
    if (error == QCanBusDevice::ConnectionError
            || m_device->state() != QCanBusDevice::ConnectedState) {
        handleDeviceLost();
    }
    emit errorOccurred(m_device->errorString());
}

void CanWorker::onDeviceStateChanged(QCanBusDevice::CanBusDeviceState state)
{
    if (state != QCanBusDevice::ConnectedState && m_deviceConnected) {
        handleDeviceLost();
    }
}

void CanWorker::handleDeviceLost()
{
    if (!m_deviceConnected) return;
    m_timeoutTimer->stop();
    m_reconnectCount++;
    emit reconnectCountChanged(m_reconnectCount);
    m_deviceConnected = false;
    m_timedOut = true;
    emit deviceConnectedChanged(false);
    emit connectionStatusChanged(false);
    // 启动自动重连
    if (!m_reconnectTimer->isActive()) {
        m_reconnectAttempt = 0;
        m_reconnectTimer->start();
    }
}

void CanWorker::tryReconnect()
{
    // 还没有创建设备对象 → 走完整 start 流程
    if (!m_device) {
        start(m_plugin, m_interface, m_bitrate);
        return;
    }

    // 设备对象存在但未连接 → 尝试重连
    if (m_device->state() == QCanBusDevice::ConnectedState) {
        m_reconnectTimer->stop();
        return;
    }

    if (m_device->connectDevice()) {
        qInfo() << "CAN device reconnected:" << m_plugin << m_interface;
        m_reconnectTimer->stop();
        m_reconnectAttempt = 0;
        m_deviceConnected = true;
        m_timedOut = true;   // 等待帧到达时清除
        m_timeoutTimer->start();
        emit deviceConnectedChanged(true);
        emit connectionStatusChanged(true);
    } else {
        m_reconnectAttempt++;
        // 每 15 次 (~30s) 通知一次，避免刷屏
        if (m_reconnectAttempt % 15 == 1) {
            emit errorOccurred(QString("正在重连 %1/%2 (第 %3 次)...")
                .arg(m_plugin, m_interface).arg(m_reconnectAttempt));
        }
    }
}

void CanWorker::checkTimeout()
{
    // 超时仅意味无 CAN 帧, 不代表 PCAN 硬件断开 —
    // 不修改 m_deviceConnected
    m_timedOut = true;
    m_timeoutTimer->stop();
    emit connectionStatusChanged(false);
    emit errorOccurred("CAN frame timeout (>500ms)");
}

