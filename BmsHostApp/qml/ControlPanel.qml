import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: root
    width: 280
    height: parent ? parent.height : 0
    edge: Qt.RightEdge

    property var cellsPage: null
    property var configDialog: null

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16
        spacing: 12

        Label { text: "控制面板"; font.pixelSize: 18; font.bold: true }

        GroupBox { title: "FET 控制"
            Layout.fillWidth: true
            GridLayout { columns: 2; columnSpacing: 8; rowSpacing: 8
                Button { text: "充电 ON"; Layout.fillWidth: true
                         enabled: bms.activeFaults === 0 && bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x10, 0) }
                Button { text: "充电 OFF"; Layout.fillWidth: true
                         enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x11, 0) }
                Button { text: "放电 ON"; Layout.fillWidth: true
                         enabled: bms.activeFaults === 0 && bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x20, 0) }
                Button { text: "放电 OFF"; Layout.fillWidth: true
                         enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x21, 0) }
            }
        }

        GroupBox { title: "均衡"
            Layout.fillWidth: true
            RowLayout {
                Button { text: "开启均衡"; Layout.fillWidth: true
                    enabled: bms.connectionStatus === 1
                    onClicked: {
                        var mask = 0
                        if (root.cellsPage) {
                            for (var i = 0; i < root.cellsPage.selectedCells.length; i++)
                                mask |= (1 << root.cellsPage.selectedCells[i])
                        }
                        bms.sendControl(0x30, mask)
                    }
                }
                Button { text: "关闭均衡"; Layout.fillWidth: true
                    enabled: bms.connectionStatus === 1
                    onClicked: bms.sendControl(0x31, 0) }
            }
        }

        GroupBox { title: "系统"
            Layout.fillWidth: true
            ColumnLayout {
                Button { text: "清除故障"; Layout.fillWidth: true
                         enabled: bms.connectionStatus === 1
                         onClicked: bms.sendControl(0x01, 0) }
                Button { text: "刷新数据"; Layout.fillWidth: true
                         enabled: bms.connectionStatus === 1
                         onClicked: bms.sendQuery(0x00) }
                Button { text: "参数配置"; Layout.fillWidth: true
                         enabled: bms.connectionStatus === 1
                         onClicked: {
                             root.close()
                             if (root.configDialog) root.configDialog.open()
                         } }
            }
        }

        Item { Layout.fillHeight: true }

        // 关机 (底部)
        Button {
            text: "⏻ 关机"
            Layout.fillWidth: true
            highlighted: true
            onClicked: shutdownDialog.open()
        }
    }

    Dialog {
        id: shutdownDialog
        title: "确认关机"
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: "确认向 BMS 发送关机指令？\n此操作将关闭电池输出。" }
        onAccepted: {
            bms.sendControl(0xFF, 0)
            root.close()
        }
    }
}
