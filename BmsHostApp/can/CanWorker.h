#pragma once

#include <QObject>

// PCAN 收发 worker (占位) — 后续任务实现
class CanWorker : public QObject
{
    Q_OBJECT
public:
    explicit CanWorker(QObject *parent = nullptr);
};
