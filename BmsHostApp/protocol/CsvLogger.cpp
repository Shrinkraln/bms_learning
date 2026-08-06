#include "CsvLogger.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

CsvLogger::CsvLogger(QObject *parent) : QObject(parent) {}
CsvLogger::~CsvLogger() { stop(); }

bool CsvLogger::start(const QString &filePath, int intervalMs)
{
    m_filePath = filePath;
    m_intervalMs = intervalMs;
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Cannot open CSV log:" << filePath;
        return false;
    }
    m_stream.setDevice(&m_file);
    // 空文件时写表头
    if (m_file.size() == 0) { writeHeader(); }
    m_lastWriteMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

void CsvLogger::appendRow(const BmsSnapshot &snap)
{
    if (!m_file.isOpen()) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool faultChanged = (snap.active_faults != m_lastFaults);
    bool intervalElapsed = (now - m_lastWriteMs >= m_intervalMs);

    if (!faultChanged && !intervalElapsed) return;

    rotateIfNeeded();
    m_stream << snap.timestamp.toString(Qt::ISODateWithMs) << ","
             << snap.pack_voltage_mv << ","
             << snap.pack_current_ma << ","
             << snap.soc_permil << ","
             << snap.prot_level << ","
             << snap.active_faults;
    for (int i = 0; i < 9; ++i) m_stream << "," << snap.cell_mv[i];
    for (int i = 0; i < 3; ++i) m_stream << "," << snap.temp_0p1c[i];
    m_stream << "," << (snap.fet_chg ? 1 : 0)
             << "," << (snap.fet_dsg ? 1 : 0)
             << "," << (snap.balancing ? 1 : 0)
             << "\n";
    m_stream.flush();

    m_lastWriteMs = now;
    m_lastFaults = snap.active_faults;
}

void CsvLogger::stop()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void CsvLogger::rotateIfNeeded()
{
    if (m_file.size() < MAX_SIZE) return;
    stop();
    QFileInfo fi(m_filePath);
    QString newName = fi.dir().filePath(
        QString("bms_log_%1.csv")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddTHHmmss")));
    QFile::rename(m_filePath, newName);

    // 清理旧文件
    QDir dir = fi.dir();
    QStringList logs = dir.entryList({"bms_log_*.csv"}, QDir::Files, QDir::Name);
    while (logs.size() > MAX_ROTATIONS) {
        QFile::remove(dir.filePath(logs.takeFirst()));
    }

    start(m_filePath, m_intervalMs);
}

void CsvLogger::writeHeader()
{
    m_stream << "timestamp,pack_voltage_mv,pack_current_ma,soc_permil,"
             << "prot_level,active_faults";
    for (int i = 1; i <= 9; ++i) m_stream << ",cell_" << i << "_mv";
    for (int i = 1; i <= 3; ++i) m_stream << ",ts" << i << "_0p1c";
    m_stream << ",fet_chg,fet_dsg,balancing\n";
    m_stream.flush();
}
