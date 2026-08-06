#pragma once

#include <QObject>

// 0x100/0x101/0x120/0x121 帧解码器 (占位) — 后续任务实现
class BmsProtocolDecoder : public QObject
{
    Q_OBJECT
public:
    explicit BmsProtocolDecoder(QObject *parent = nullptr);
};
