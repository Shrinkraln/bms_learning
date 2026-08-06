#ifndef CELL_VOLTAGE_MODEL_H
#define CELL_VOLTAGE_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <array>

class CellVoltageModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { VoltageRole = Qt::UserRole + 1, CellIndexRole,
                 IsMaxRole, IsMinRole, ColorRole };
    Q_ENUM(Roles)

    explicit CellVoltageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return 9; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateCell(int index, quint16 mV, bool isMax, bool isMin);

private:
    struct CellInfo { quint16 mV = 0; bool isMax = false; bool isMin = false; };
    std::array<CellInfo, 9> m_cells;
    QColor colorForVoltage(quint16 mV) const;
};

#endif
