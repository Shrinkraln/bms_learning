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

void CommunicationMonitor::setBmsModel(BmsDataModel *m)
{
    m_bmsModel = m;
    if (m_bmsModel) {
        connect(m_bmsModel, &BmsDataModel::statusTextChanged,
                this, &CommunicationMonitor::onBatteryStatusChanged);
    }
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
            // 级联置灰后重算健康度 (canBusActive/afeOnline 已归零)
            updateHealthSummary();
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
