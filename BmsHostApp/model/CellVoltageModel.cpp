#include "CellVoltageModel.h"

CellVoltageModel::CellVoltageModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> CellVoltageModel::roleNames() const
{
    return {{VoltageRole, "voltage"}, {CellIndexRole, "cellIndex"},
            {IsMaxRole, "isMax"}, {IsMinRole, "isMin"}, {ColorRole, "barColor"}};
}

QVariant CellVoltageModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= 9) return {};
    const auto &c = m_cells[row];
    switch (role) {
    case VoltageRole:  return c.mV;
    case CellIndexRole: return row + 1;
    case IsMaxRole:     return c.isMax;
    case IsMinRole:     return c.isMin;
    case ColorRole:     return colorForVoltage(c.mV);
    }
    return {};
}

void CellVoltageModel::updateCell(int index, quint16 mV, bool isMax, bool isMin)
{
    if (index < 0 || index >= 9) return;
    auto &c = m_cells[index];
    // 防御: 新数据为 0 但之前有有效数据 → 保留旧值, 不被覆盖
    // 场景: 固件停止发送某路 CAN 帧 (如 0x110) 后,
    //       decodeFrames 创建新 snapshot 时 cell_mv 默认 0,
    //       applySnapshot 用 0 覆盖掉之前的有效数据导致显示清零
    if (mV == 0 && c.mV != 0) return;
    if (c.mV == mV && c.isMax == isMax && c.isMin == isMin) return;
    c.mV = mV; c.isMax = isMax; c.isMin = isMin;
    QModelIndex idx = createIndex(index, 0);
    emit dataChanged(idx, idx, {VoltageRole, IsMaxRole, IsMinRole, ColorRole});
}

QColor CellVoltageModel::colorForVoltage(quint16 mV) const
{
    if (mV > 4200) return QColor("#e74c3c"); // 红
    if (mV >= 3600) return QColor("#27ae60"); // 绿
    if (mV >= 3000) return QColor("#f39c12"); // 黄
    return QColor("#3498db"); // 蓝 (< 3000)
}
