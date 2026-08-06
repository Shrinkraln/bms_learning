import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 200; height: 200
    property real value: 0      // 0-1000 permil
    property real minValue: 0
    property real maxValue: 1000
    property string unit: "%"
    property int decimals: 0

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var cx = width / 2, cy = height / 2
            var r = Math.min(cx, cy) - 15

            // 背景弧
            ctx.beginPath()
            ctx.arc(cx, cy, r, Math.PI * 1.35, Math.PI * 0.65, false)
            ctx.lineWidth = 14
            ctx.strokeStyle = "#e0e0e0"
            ctx.stroke()

            // 值弧
            var pct = (value - minValue) / (maxValue - minValue)
            var angle = Math.PI * 1.35 + pct * Math.PI * 1.3
            ctx.beginPath()
            ctx.arc(cx, cy, r, Math.PI * 1.35, angle, false)
            ctx.lineWidth = 14
            ctx.lineCap = "round"
            // 颜色: 红(0-200) 橙(201-500) 绿(501-1000)
            if (pct < 0.2) ctx.strokeStyle = "#e74c3c"
            else if (pct < 0.5) ctx.strokeStyle = "#f39c12"
            else ctx.strokeStyle = "#27ae60"
            ctx.stroke()
        }
    }

    // 中央数值
    Column {
        anchors.centerIn: parent
        spacing: 2
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (value / 10).toFixed(decimals)
            font.pixelSize: 36; font.bold: true
            color: "#333"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: unit
            font.pixelSize: 14; color: "#666"
        }
    }

    // 动画
    Behavior on value {
        NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
    }

    // 值变化时重绘
    onValueChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
