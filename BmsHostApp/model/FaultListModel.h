#pragma once

#include <QObject>

// 故障列表模型 (占位) — 后续任务实现
class FaultListModel : public QObject
{
    Q_OBJECT
public:
    explicit FaultListModel(QObject *parent = nullptr);
};
