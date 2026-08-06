#ifndef CSV_LOGGER_H
#define CSV_LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "BmsSnapshot.h"

class CsvLogger : public QObject {
    Q_OBJECT
public:
    explicit CsvLogger(QObject *parent = nullptr);
    ~CsvLogger();

    bool start(const QString &filePath, int intervalMs = 1000);
    void appendRow(const BmsSnapshot &snap);
    void stop();

private:
    void rotateIfNeeded();
    void writeHeader();

    QFile m_file;
    QTextStream m_stream;
    QString m_filePath;
    int m_intervalMs = 1000;
    qint64 m_lastWriteMs = 0;
    quint16 m_lastFaults = 0xFFFF; // 强制首行写入
    static constexpr qint64 MAX_SIZE = 10 * 1024 * 1024;
    static constexpr int MAX_ROTATIONS = 10;
};

#endif // CSV_LOGGER_H
