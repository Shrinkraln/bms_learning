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
    bool afe_online = true;     /**< AFE 在线: true=正常响应, false=离线 (0x120 ctrl bit5) */
    QDateTime timestamp;
};

#endif // BMS_SNAPSHOT_H
