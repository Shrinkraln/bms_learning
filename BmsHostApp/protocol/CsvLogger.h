#pragma once

#include <QObject>

// CSV 数据日志 (占位) — 后续任务实现
class CsvLogger : public QObject
{
    Q_OBJECT
public:
    explicit CsvLogger(QObject *parent = nullptr);
};
