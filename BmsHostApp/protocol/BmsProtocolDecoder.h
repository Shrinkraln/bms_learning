#ifndef BMS_PROTOCOL_DECODER_H
#define BMS_PROTOCOL_DECODER_H

#include <QVector>
#include <QString>
#include <QtCore/QLoggingCategory>
#include "BmsSnapshot.h"
#include "CanFrame.h"

Q_DECLARE_LOGGING_CATEGORY(bmsProto)

struct BmsCommand {
    enum Type { QUERY = 0, CONTROL, CONFIG };
    Type type;
    quint8 subType;
    quint16 value;
};

class BmsProtocolDecoder {
public:
    // 上行解码 — 批量 CAN 帧 → 快照
    static QVector<BmsSnapshot> decodeFrames(const QVector<CanFrame> &batch);

    // 下行编码 — 指令 → CAN 帧
    static CanFrame encodeCommand(const BmsCommand &cmd);

    // 工具
    static QString faultName(quint8 bit);
    static const char* faultSeverity(quint8 bit); // "WARNING"/"ALERT"/"FAULT"

private:
    static bool checkDlc(const CanFrame &f, quint8 minLen);
    static BmsSnapshot decodeStatusFrame(const CanFrame &f);
    static BmsSnapshot decodeFaultFrame(const CanFrame &f);
    static BmsSnapshot decodeCellFrame1_4(const CanFrame &f, BmsSnapshot &snap);
    static BmsSnapshot decodeCellFrame5_8(const CanFrame &f, BmsSnapshot &snap);
    static BmsSnapshot decodeSocOcvFrame(const CanFrame &f, BmsSnapshot &snap);
    static BmsSnapshot decodeTemperatureFrame(const CanFrame &f, BmsSnapshot &snap);
};

#endif // BMS_PROTOCOL_DECODER_H
