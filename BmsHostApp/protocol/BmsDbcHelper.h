#pragma once

#include <QObject>

// DBC 信号定义辅助 (占位) — 后续任务实现
class BmsDbcHelper : public QObject
{
    Q_OBJECT
public:
    explicit BmsDbcHelper(QObject *parent = nullptr);
};
