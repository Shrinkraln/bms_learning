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

    // 计算柱高度 (可用区间 4~120 px)
    function barHeight(): real {
        if (voltage <= 0) return 4   // 无数据时显示占位条
        var h = (voltage - minRange) / (maxRange - minRange) * 116 + 4
        return Math.max(4, Math.min(120, h))
    }

    // 数值是否有意义
    property bool hasData: voltage > 0

    // ---- 顶部: 状态标志 (MAX / MIN) ----
    Text {
        id: statusFlag
        anchors.top: parent.top
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        text: isMax ? "▲ MAX" : isMin ? "▼ MIN" : ""
        font.pixelSize: 10; font.bold: true
        color: isMax ? "#e74c3c" : isMin ? "#3498db" : "transparent"
        visible: isMax || isMin
        height: visible ? implicitHeight : 0
    }

    // ---- 电压数值 ----
    Text {
        id: voltageText
        anchors.top: statusFlag.visible ? statusFlag.bottom : parent.top
        anchors.topMargin: statusFlag.visible ? 1 : 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: hasData ? (voltage / 1000).toFixed(3) + "V" : "---"
        font.pixelSize: 10
        color: hasData ? "#333" : "#bbb"
    }

    // ---- 电芯编号 (底部固定, 保证对齐) ----
    Text {
        id: cellLabel
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        text: "C" + cellIndex
        font.pixelSize: 11; font.bold: true
        color: "#555"
    }

    // ---- 柱状条 (从 cellLabel 向上增长, 底部固定不跳动) ----
    Rectangle {
        id: bar
        anchors.bottom: cellLabel.top
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        width: 32
        height: barHeight()
        color: hasData ? barColor : "#d0d0d0"
        radius: 3

        // 平滑过渡 — 仅动画高度, 底部 anchor 固定不动
        Behavior on height {
            SmoothedAnimation {
                duration: 150
                velocity: 80
            }
        }
        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }
}
