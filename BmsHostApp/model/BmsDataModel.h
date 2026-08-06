#pragma once

#include <QObject>

// BMS 总体数据模型 (占位) — 后续任务实现
class BmsDataModel : public QObject
{
    Q_OBJECT
public:
    explicit BmsDataModel(QObject *parent = nullptr);
};
