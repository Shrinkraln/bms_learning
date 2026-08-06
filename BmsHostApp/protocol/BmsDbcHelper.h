#ifndef BMS_DBC_HELPER_H
#define BMS_DBC_HELPER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include "can/CanFrame.h"

class BmsDbcHelper : public QObject {
    Q_OBJECT
public:
    explicit BmsDbcHelper(QObject *parent = nullptr);
    bool loadDbc(const QString &path);
    QMap<QString, QVariant> decodeFrame(const CanFrame &frame) const;
    bool isLoaded() const { return m_loaded; }

private:
    bool m_loaded = false;
    // 简化实现: 内置解码器为主，DBC 为可选增强
    // Qt 6.5+ 可用 QCanDbcFileParser + QCanFrameProcessor
};

#endif // BMS_DBC_HELPER_H
