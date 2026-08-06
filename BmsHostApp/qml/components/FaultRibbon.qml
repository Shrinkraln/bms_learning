import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 36; radius: 4
    property int activeFaults: 0
    property QtObject faultModel: null

    color: activeFaults === 0 ? "#d5f5e3" : "#fadbd8"
    border { width: 1; color: activeFaults === 0 ? "#27ae60" : "#e74c3c" }

    RowLayout {
        anchors.fill: parent; anchors.margins: 8
        Text {
            text: activeFaults === 0 ? "✓ 无活跃故障" : "⚠ " + faultModel.rowCount() + " 个活跃故障"
            font.pixelSize: 13; font.bold: true
            color: activeFaults === 0 ? "#27ae60" : "#c0392b"
        }
        Item { Layout.fillWidth: true }
        Repeater {
            model: faultModel
            Text {
                text: model.faultName
                color: model.severity === "FAULT" ? "#e74c3c"
                     : model.severity === "ALERT" ? "#e67e22" : "#f1c40f"
                font.pixelSize: 11
                visible: activeFaults !== 0
            }
        }
    }

    Behavior on color { ColorAnimation { duration: 300 } }
    Behavior on border.color { ColorAnimation { duration: 300 } }
}
