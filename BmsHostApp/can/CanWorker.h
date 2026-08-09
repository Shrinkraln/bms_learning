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
    void connectionStatusChanged(bool connected);       // CAN 总线帧活动 (500ms 超时)
    void deviceConnectedChanged(bool connected);        // PCAN 设备物理连接 (new)
    void errorOccurred(const QString &errorString);

private slots:
    void onFramesReceived();
    void onErrorOccurred(QCanBusDevice::CanBusError error);
    void onDeviceStateChanged(QCanBusDevice::CanBusDeviceState state);
    void checkTimeout();
    void tryReconnect();

private:
    void handleDeviceLost();

    QCanBusDevice *m_device = nullptr;
    QTimer *m_timeoutTimer;
    QTimer *m_reconnectTimer;
    QString m_plugin;
    QString m_interface;
    int m_bitrate = 500000;
    int m_reconnectAttempt = 0;
    bool m_timedOut = false;
    bool m_deviceConnected = false;
    static constexpr int TIMEOUT_MS = 500;
    static constexpr int RECONNECT_MS = 2000;
};

#endif // CAN_WORKER_H
