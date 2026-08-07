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

    m_timedOut = false;
    m_timeoutTimer->start();
    emit connectionStatusChanged(true);
}

void CanWorker::stop()
{
    m_timedOut = false;
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
        if (m_timedOut) {
            m_timedOut = false;
            emit connectionStatusChanged(true);
        }
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
    m_timedOut = true;
    m_timeoutTimer->stop();
    emit connectionStatusChanged(false);
    emit errorOccurred("CAN frame timeout (>500ms)");
}
