import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 60; height: 200
    property real voltage: 0       // mV
    property int cellIndex: 1
    property bool isMax: false
    property bool isMin: false
    property color barColor: "#27ae60"
    property real minRange: 2800
    property real maxRange: 4300

    Column {
        anchors.fill: parent
        spacing: 4

        // 标记
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: isMax ? "*" : isMin ? "_" : ""
            font.pixelSize: 14; font.bold: true
            color: isMax ? "#e74c3c" : "#3498db"
            visible: isMax || isMin
        }

        // 电压值
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (voltage / 1000).toFixed(3) + "V"
            font.pixelSize: 10; color: "#333"
        }

        // 柱
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 32; height: {
                var h = (voltage - minRange) / (maxRange - minRange) * 120
                return Math.max(2, Math.min(120, h))
            }
            y: {
                var h = (voltage - minRange) / (maxRange - minRange) * 120
                return 120 - Math.max(2, Math.min(120, h))
            }
            color: barColor
            radius: 3
            Behavior on height { NumberAnimation { duration: 300 } }
            Behavior on y { NumberAnimation { duration: 300 } }
        }

        // 编号
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "C" + cellIndex
            font.pixelSize: 11; font.bold: true
            color: "#555"
        }
    }
}
