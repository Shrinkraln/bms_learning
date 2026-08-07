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
    if (m_canBusConnected != connected) {
        m_canBusConnected = connected;
        emit canBusConnectedChanged();
    }

    if (m_shutdownSent && !connected) {
        m_connStatus = SHUTDOWN;
    } else {
        m_connStatus = connected ? CONNECTED : DISCONNECTED;
    }
    emit connectionStatusChanged();
    m_statusText = m_connStatus == CONNECTED ? "● 已连接"
        : m_connStatus == SHUTDOWN ? "⏻ BMS 已关机" : "⚠ 已断开";
    emit statusTextChanged(m_statusText);
}

void BmsDataModel::applySnapshot(const BmsSnapshot &snap)
{
    // 仅变更时 set + emit。为简洁起见，此处批量比较并 emit（实际项目可逐字段）
    bool changed = false;

    // 注: 与 brief 不同 — prop##Changed 会生成 cell_min_mvChanged 等不存在的信号名，
    // 信号名须显式传入 (packVoltageChanged / cellMinChanged / ...)
#define UPDATE_PROP(prop, sig, val) if (m_snap.prop != (val)) { m_snap.prop = (val); emit sig(); changed = true; }

    UPDATE_PROP(pack_voltage_mv, packVoltageChanged, snap.pack_voltage_mv)
    UPDATE_PROP(pack_current_ma, packCurrentChanged, snap.pack_current_ma)
    UPDATE_PROP(soc_permil, socPermilChanged, snap.soc_permil)
    UPDATE_PROP(real_soc_permil, realSocPermilChanged, snap.real_soc_permil)
    UPDATE_PROP(remaining_mah, remainingMahChanged, snap.remaining_mah)
    UPDATE_PROP(q_max_mah, qMaxMahChanged, snap.q_max_mah)
    UPDATE_PROP(prot_level, protLevelChanged, snap.prot_level)

    if (m_snap.active_faults != snap.active_faults) {
        m_snap.active_faults = snap.active_faults;
        emit activeFaultsChanged();
        m_faultModel.setFaults(snap.active_faults);
        changed = true;
    }

    UPDATE_PROP(cell_min_mv, cellMinChanged, snap.cell_min_mv)
    UPDATE_PROP(cell_max_mv, cellMaxChanged, snap.cell_max_mv)
    UPDATE_PROP(cell_diff_mv, cellDiffChanged, snap.cell_diff_mv)
    UPDATE_PROP(fet_chg, fetChargeChanged, snap.fet_chg)
    UPDATE_PROP(fet_dsg, fetDischargeChanged, snap.fet_dsg)
    UPDATE_PROP(balancing, balancingChanged, snap.balancing)

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
    // 3 路 NTC 平均温度 (°C) — 曲线页等标量场景使用
    qreal avg = (snap.temp_0p1c[0] + snap.temp_0p1c[1] + snap.temp_0p1c[2]) / 3.0 / 10.0;
    if (m_avgTempC != avg) {
        m_avgTempC = avg;
        emit avgTempCChanged();
    }

    // CSV
    if (m_csvLogger) { m_csvLogger->appendRow(snap); }

    // AFE 在线状态 → 连接显示
    if (!snap.afe_online && m_connStatus == CONNECTED) {
        m_connStatus = DISCONNECTED;
        m_statusText = QString::fromUtf8("⚠ 电池未接入");
        emit connectionStatusChanged();
        emit statusTextChanged(m_statusText);
    } else if (snap.afe_online && m_connStatus == DISCONNECTED && !m_shutdownSent) {
        m_connStatus = CONNECTED;
        m_statusText = QString::fromUtf8("● 已连接");
        emit connectionStatusChanged();
        emit statusTextChanged(m_statusText);
    }

    // 时间戳
    m_lastUpdate = snap.timestamp.toString("HH:mm:ss");
    emit lastUpdateChanged(m_lastUpdate);
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
