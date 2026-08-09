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
