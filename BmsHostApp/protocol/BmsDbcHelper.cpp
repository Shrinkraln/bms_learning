#include "BmsDbcHelper.h"
#include <QtSerialBus/QCanDbcFileParser>
#include <QtSerialBus/QCanFrameProcessor>
#include <QDebug>

BmsDbcHelper::BmsDbcHelper(QObject *parent) : QObject(parent) {}

bool BmsDbcHelper::loadDbc(const QString &path)
{
    // 简化: 验证 DBC 文件可读
    QCanDbcFileParser parser;
    bool ok = parser.parse(path);
    if (!ok) {
        qWarning() << "DBC parse failed:" << parser.errorString();
        qWarning() << "Falling back to built-in decoder";
        m_loaded = false;
        return false;
    }
    m_loaded = true;
    qInfo() << "DBC loaded:" << path;
    return true;
}

QMap<QString, QVariant> BmsDbcHelper::decodeFrame(const CanFrame &frame) const
{
    // 占位: DBC 解码实现。主路径仍为内置解码器。
    Q_UNUSED(frame)
    return {};
}
