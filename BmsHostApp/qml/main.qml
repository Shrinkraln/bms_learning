import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: "BMS 9S 上位机"

    // 状态栏
    footer: Rectangle {
        height: 24
        color: "#f0f0f0"
        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            Text { text: "● 未连接"; id: statusText }
            Item { Layout.fillWidth: true }
            Text { text: "最后更新: --"; id: lastUpdateText }
        }
    }

    // 占位页面 (后续任务替换)
    Text {
        anchors.centerIn: parent
        text: "BMS 9S 上位机\nQt 6.5+ / PCAN-USB"
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 24
        color: "#999"
    }
}
