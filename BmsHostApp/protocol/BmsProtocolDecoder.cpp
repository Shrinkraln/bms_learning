#include "BmsProtocolDecoder.h"
#include <QDebug>
#include <QHash>
#include <cstring>

Q_LOGGING_CATEGORY(bmsProto, "bms.protocol")

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

    // 诊断: 统计本批次中各 CAN ID 的帧数
    QHash<quint32, int> idCounts;
    for (const auto &f : batch) { idCounts[f.id]++; }

    for (const auto &f : batch) {
        switch (f.id) {
        case ID_BMS_FAULT:
            if (checkDlc(f, 8)) {
                const BmsSnapshot s2 = decodeFaultFrame(f);
                snap.prot_level    = s2.prot_level;
                snap.active_faults = s2.active_faults;
                snap.cell_max_mv   = s2.cell_max_mv;
                snap.cell_min_mv   = s2.cell_min_mv;
                snap.max_temp_0p1c = s2.max_temp_0p1c;
            }
            break;
        case ID_CELL_VOLT_1_4:
            if (checkDlc(f, 8)) { snap = decodeCellFrame1_4(f, snap); has_cells_1_4 = true; }
            break;
        case ID_CELL_VOLT_5_8:
            if (checkDlc(f, 8)) { snap = decodeCellFrame5_8(f, snap); has_cells_5_8 = true; }
            break;
        case ID_BMS_STATUS2:
            if (checkDlc(f, 8)) {
                BmsSnapshot s2 = decodeStatusFrame(f);
                for (int i = 0; i < 8; ++i) { s2.cell_mv[i] = snap.cell_mv[i]; }
                s2.active_faults = snap.active_faults;
                snap = s2;
                has_status = true;
            }
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

    // 诊断: 当有 STATUS 帧但缺少 CELL_VOLT_1_4 时告警 (C1-C4 不显示)
    if (has_status && !has_cells_1_4) {
        static int c1c4WarnCount = 0;
        if (++c1c4WarnCount <= 5 || c1c4WarnCount % 50 == 0) {
            qCWarning(bmsProto, "C1-C4 MISSING: 0x110 not in batch (count=%d). "
                      "Batch IDs: %s",
                      c1c4WarnCount,
                      [&idCounts]() {
                          QStringList parts;
                          for (auto it = idCounts.cbegin(); it != idCounts.cend(); ++it)
                              parts.append(QString("0x%1×%2").arg(it.key(), 3, 16, QChar('0')).arg(it.value()));
                          return parts.join(", ").toUtf8();
                      }().constData());
        }
    }
    if (has_status && !has_cells_5_8) {
        static int c5c8WarnCount = 0;
        if (++c5c8WarnCount <= 5 || c5c8WarnCount % 50 == 0) {
            qCWarning(bmsProto, "C5-C8 MISSING: 0x111 not in batch (count=%d). "
                      "Batch IDs: %s",
                      c5c8WarnCount,
                      [&idCounts]() {
                          QStringList parts;
                          for (auto it = idCounts.cbegin(); it != idCounts.cend(); ++it)
                              parts.append(QString("0x%1×%2").arg(it.key(), 3, 16, QChar('0')).arg(it.value()));
                          return parts.join(", ").toUtf8();
                      }().constData());
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
    s.afe_online = (ctrl >> 5U) & 0x01U;
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
