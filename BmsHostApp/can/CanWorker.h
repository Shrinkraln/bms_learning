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
    bool m_timedOut = false;
    static constexpr int TIMEOUT_MS = 500;
};

#endif // CAN_WORKER_H
