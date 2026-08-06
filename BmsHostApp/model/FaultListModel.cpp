#include "FaultListModel.h"
#include "protocol/BmsProtocolDecoder.h"

FaultListModel::FaultListModel(QObject *parent) : QAbstractListModel(parent) {}

int FaultListModel::rowCount(const QModelIndex &) const { return m_active.size(); }

QVariant FaultListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= m_active.size()) return {};
    const auto &e = m_active[row];
    switch (role) {
    case FaultNameRole: return e.name;
    case FaultBitRole:  return e.bit;
    case SeverityRole:  return e.severity;
    }
    return {};
}

QHash<int, QByteArray> FaultListModel::roleNames() const
{
    return {{FaultNameRole, "faultName"}, {FaultBitRole, "faultBit"}, {SeverityRole, "severity"}};
}

void FaultListModel::setFaults(quint16 activeBits)
{
    beginResetModel();
    m_active.clear();
    for (int b = 0; b < 12; ++b) {
        if (activeBits & (1U << b)) {
            m_active.append({b, BmsProtocolDecoder::faultName(static_cast<quint8>(b)),
                             QString::fromLatin1(BmsProtocolDecoder::faultSeverity(static_cast<quint8>(b)))});
        }
    }
    endResetModel();
}
