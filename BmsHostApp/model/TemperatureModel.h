#ifndef TEMPERATURE_MODEL_H
#define TEMPERATURE_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <array>

class TemperatureModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { TempRole = Qt::UserRole + 1, SensorIndexRole, ColorRole };
    Q_ENUM(Roles)

    explicit TemperatureModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return 3; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void updateTemp(int index, qint16 temp0p1c);

private:
    std::array<qint16, 3> m_temps = {};
    QColor colorForTemp(qreal degC) const;
};

#endif
