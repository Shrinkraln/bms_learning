#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <QtSerialBus/QCanBusFrame>
#include <QtCore/QtGlobal>
#include <QMetaType>

struct CanFrame {
    quint32 id = 0;
    quint8  len = 0;
    quint8  data[8] = {};

    static CanFrame fromQCanBusFrame(const QCanBusFrame &f) {
        CanFrame cf;
        cf.id = f.frameId();
        cf.len = static_cast<quint8>(qMin(f.payload().size(), 8));
        for (int i = 0; i < cf.len; ++i)
            cf.data[i] = static_cast<quint8>(f.payload().at(i));
        return cf;
    }

    QCanBusFrame toQCanBusFrame() const {
        QCanBusFrame f;
        f.setFrameId(id);
        f.setPayload(QByteArray(reinterpret_cast<const char*>(data), len));
        return f;
    }
};

Q_DECLARE_METATYPE(CanFrame)

#endif // CAN_FRAME_H
