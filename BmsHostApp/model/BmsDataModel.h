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
#include "protocol/BmsDbcHelper.h"

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
    Q_PROPERTY(bool canBusConnected READ canBusConnected NOTIFY canBusConnectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastUpdate READ lastUpdate NOTIFY lastUpdateChanged)
    Q_PROPERTY(qreal avgTempC READ avgTempC NOTIFY avgTempCChanged)
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
    bool canBusConnected() const { return m_canBusConnected; }
    QString statusText() const { return m_statusText; }
    QString lastUpdate() const { return m_lastUpdate; }
    qreal avgTempC() const { return m_avgTempC; }
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
    void canBusConnectedChanged();
    void lastUpdateChanged(const QString &text);
    void statusTextChanged(const QString &text);
    void avgTempCChanged();

private:
    void applySnapshot(const BmsSnapshot &snap);

    BmsSnapshot m_snap;
    int m_connStatus = DISCONNECTED;
    bool m_canBusConnected = false;
    QString m_statusText = "⚠ 已断开";
    QString m_lastUpdate = "--";
    qreal m_avgTempC = 0.0;
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
