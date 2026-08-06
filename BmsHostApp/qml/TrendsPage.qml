import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCharts

Page {
    id: root
    title: "曲线"
    property bool paused: false
    property int elapsed: 0          // 采样秒数，仅保留 300s 窗口

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 8
        spacing: 4

        // TabBar 切换曲线
        TabBar {
            id: trendTab
            TabButton { text: "总压" }
            TabButton { text: "电流" }
            TabButton { text: "SOC" }
            TabButton { text: "温度" }
            onCurrentIndexChanged: root.setActiveSeries(currentIndex)
        }

        // 图表
        ChartView {
            id: chartView
            Layout.fillWidth: true; Layout.fillHeight: true
            antialiasing: true
            legend.visible: false
            animationOptions: ChartView.NoAnimation

            // 4 条 LineSeries，共享 300s 时间轴，各自独立 Y 轴
            LineSeries { id: seriesVoltage; name: "总压 (V)";  axisX: axisX; axisY: axisYVolt }
            LineSeries { id: seriesCurrent; name: "电流 (A)";  axisX: axisX; axisY: axisYCur }
            LineSeries { id: seriesSoc;     name: "SOC (%)";   axisX: axisX; axisY: axisYSoc }
            LineSeries { id: seriesTemp;    name: "温度 (°C)"; axisX: axisX; axisY: axisYTemp }

            ValueAxis { id: axisX; min: 0; max: 300; labelFormat: "%d s"; tickCount: 7 }
            ValueAxis { id: axisYVolt; min: 0;    max: 50;   labelFormat: "%d" }
            ValueAxis { id: axisYCur;  min: -150; max: 150;  labelFormat: "%d" }
            ValueAxis { id: axisYSoc;  min: 0;    max: 100;  labelFormat: "%d" }
            ValueAxis { id: axisYTemp; min: -20;  max: 120;  labelFormat: "%d" }
        }

        // 控制栏
        RowLayout {
            Button { text: paused ? "▶ 继续" : "⏸ 暂停"; onClicked: paused = !paused }
            Button { text: "⏹ 清除"; onClicked: root.clearSeries() }
            Button { text: "📁 导出CSV"; onClicked: exportDialog.open() }
            Item { Layout.fillWidth: true }
            Label { text: "300s 窗口"; color: "#999" }
        }
    }

    // 1s 采样
    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: root.appendSample()
    }

    // 导出: 弹保存对话框，复用 C++ Q_INVOKABLE saveCsvLog
    FileDialog {
        id: exportDialog
        title: "导出 CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)"]
        defaultSuffix: "csv"
        onAccepted: {
            var path = exportDialog.selectedFile.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            bms.saveCsvLog(path)
        }
    }

    function setActiveSeries(index) {
        seriesVoltage.visible = (index === 0)
        seriesCurrent.visible = (index === 1)
        seriesSoc.visible = (index === 2)
        seriesTemp.visible = (index === 3)
        axisYVolt.visible = (index === 0)
        axisYCur.visible = (index === 1)
        axisYSoc.visible = (index === 2)
        axisYTemp.visible = (index === 3)
    }

    function prune(series, windowStart) {
        while (series.count > 0 && series.at(0).x < windowStart)
            series.remove(0)
    }

    function appendSample() {
        if (root.paused) return
        var t = root.elapsed++
        seriesVoltage.append(t, bms.packVoltage / 1000.0)
        seriesCurrent.append(t, bms.packCurrent / 1000.0)
        seriesSoc.append(t, bms.socPermil / 10.0)
        seriesTemp.append(t, bms.avgTempC)
        var ws = t - 300
        root.prune(seriesVoltage, ws)
        root.prune(seriesCurrent, ws)
        root.prune(seriesSoc, ws)
        root.prune(seriesTemp, ws)
        if (t > 300) { axisX.min = ws; axisX.max = t }
        else         { axisX.min = 0;  axisX.max = 300 }
    }

    function clearSeries() {
        seriesVoltage.clear(); seriesCurrent.clear()
        seriesSoc.clear(); seriesTemp.clear()
        root.elapsed = 0
        axisX.min = 0; axisX.max = 300
    }

    Component.onCompleted: root.setActiveSeries(0)
}
