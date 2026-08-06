#pragma once

#include <QObject>

// 温度模型 (占位) — 后续任务实现
class TemperatureModel : public QObject
{
    Q_OBJECT
public:
    explicit TemperatureModel(QObject *parent = nullptr);
};
