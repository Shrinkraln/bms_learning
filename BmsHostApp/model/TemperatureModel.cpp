#include "TemperatureModel.h"

TemperatureModel::TemperatureModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> TemperatureModel::roleNames() const
{
    return {{TempRole, "temperature"}, {SensorIndexRole, "sensorIndex"}, {ColorRole, "tempColor"}};
}

QVariant TemperatureModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= 3) return {};
    switch (role) {
    case TempRole:       return m_temps[row] / 10.0;
    case SensorIndexRole: return row + 1;
    case ColorRole:       return colorForTemp(m_temps[row] / 10.0);
    }
    return {};
}

void TemperatureModel::updateTemp(int index, qint16 temp0p1c)
{
    if (index < 0 || index >= 3) return;
    if (m_temps[index] == temp0p1c) return;
    m_temps[index] = temp0p1c;
    QModelIndex idx = createIndex(index, 0);
    emit dataChanged(idx, idx);
}

QColor TemperatureModel::colorForTemp(qreal degC) const
{
    if (degC > 50.0) return QColor("#e74c3c");
    if (degC >= 30.0) return QColor("#f39c12");
    if (degC >= 0.0) return QColor("#27ae60");
    return QColor("#3498db");
}
