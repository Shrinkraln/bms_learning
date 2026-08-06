import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280; height: 800; visible: true
    title: "BMS 9S 上位机"

    property var cellsPage: null

    // 恢复窗口几何 (settings 为 C++ 注册的 QSettings 上下文属性)
    Component.onCompleted: {
        var geo = settings.value("ui/windowGeometry", "")
        if (geo) {
            var p = String(geo).split(",")
            if (p.length === 4) {
                root.x = parseInt(p[0]); root.y = parseInt(p[1])
                root.width = parseInt(p[2]); root.height = parseInt(p[3])
            }
        }
    }
    onClosing: {
        settings.setValue("ui/windowGeometry",
            root.x + "," + root.y + "," + root.width + "," + root.height)
    }

    header: TabBar {
        id: mainTab
        TabButton { text: "总览" }
        TabButton { text: "电芯" }
        TabButton { text: "曲线" }
        Item { Layout.fillWidth: true }
        ToolButton { text: "☰"; onClicked: controlPanel.open() }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: mainTab.currentIndex
        OverviewPage {}
        CellsPage { id: cellsPageObj; Component.onCompleted: root.cellsPage = cellsPageObj }
        TrendsPage {}
    }

    // 状态栏
    footer: Rectangle {
        height: 28; color: "#f5f5f5"
        RowLayout {
            anchors.fill: parent; anchors.margins: 6
            Text { text: bms.statusText; id: statusLabel }
            Item { Layout.fillWidth: true }
            Text { text: "最后更新: " + bms.lastUpdate; color: "#999" }
        }
    }

    ControlPanel {
        id: controlPanel
        cellsPage: root.cellsPage
        configDialog: configDialog
    }

    ConfigDialog { id: configDialog }
}
