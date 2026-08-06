#pragma once

#include <QObject>

// 单节电压模型 (占位) — 后续任务实现
class CellVoltageModel : public QObject
{
    Q_OBJECT
public:
    explicit CellVoltageModel(QObject *parent = nullptr);
};
