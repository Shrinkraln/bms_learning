import QtQuick

// 状态指示器: 圆点 + 文本
//   active=true  → activeColor (默认绿)
//   active=false → inactiveColor (默认红)
//   neutral=true → neutralColor (默认灰, 优先级最高)
Row {
    id: root
    property bool active: false
    property bool neutral: false
    property string text: ""
    property color activeColor: "#2e7d32"
    property color inactiveColor: "#c62828"
    property color neutralColor: "#9e9e9e"

    spacing: 5
    Rectangle {
        width: 10; height: 10; radius: 5
        anchors.verticalCenter: parent.verticalCenter
        color: root.neutral ? root.neutralColor
             : root.active ? root.activeColor : root.inactiveColor
    }
    Text {
        text: root.text
        color: root.neutral ? root.neutralColor
             : root.active ? root.activeColor : root.inactiveColor
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 12
    }
}
