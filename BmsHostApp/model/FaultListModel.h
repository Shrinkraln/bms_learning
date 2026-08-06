#ifndef FAULT_LIST_MODEL_H
#define FAULT_LIST_MODEL_H

#include <QAbstractListModel>
#include <QVector>

struct FaultEntry {
    int bit;
    QString name;
    QString severity; // "WARNING", "ALERT", "FAULT"
};

class FaultListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { FaultNameRole = Qt::UserRole + 1, FaultBitRole, SeverityRole };
    Q_ENUM(Roles)

    explicit FaultListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFaults(quint16 activeBits);

private:
    QVector<FaultEntry> m_active;
};

#endif
