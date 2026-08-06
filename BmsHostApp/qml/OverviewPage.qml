import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Page {
    title: "总览"
    id: overviewPage

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Row 0: SOC 仪表 (左) | 电池状态 + FET均衡 (右)
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            // SOC 仪表
            GroupBox {
                title: "SOC"
                Layout.preferredWidth: 220
                Layout.preferredHeight: 240
                GaugeArc {
                    anchors.centerIn: parent
                    width: 180; height: 180
                    value: bms.socPermil
                }
            }

            // 右侧：电池状态 + FET 均衡 上下排列
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                GroupBox {
                    title: "电池状态"
                    Layout.fillWidth: true
                    GridLayout {
                        anchors.fill: parent
                        columns: 2; columnSpacing: 16; rowSpacing: 4
                        Label { text: "总压:" }
                        Label { text: (bms.packVoltage / 1000).toFixed(2) + " V"; font.bold: true }
                        Label { text: "电流:" }
                        Label { text: (bms.packCurrent / 1000).toFixed(2) + " A"; font.bold: true
                                color: bms.packCurrent < 0 ? "#e74c3c" : "#27ae60" }
                        Label { text: "剩余容量:" }
                        Label { text: (bms.remainingMah / 1000).toFixed(2) + " Ah" }
                        Label { text: "Q_max:" }
                        Label { text: (bms.qMaxMah / 1000).toFixed(2) + " Ah" }
                    }
                }

                GroupBox {
                    title: "FET & 均衡"
                    Layout.fillWidth: true
                    GridLayout {
                        anchors.fill: parent
                        columns: 2; columnSpacing: 16; rowSpacing: 2
                        Label { text: "充电 FET:" }
                        Label { text: bms.fetCharge ? "● 开启" : "○ 关闭"
                                color: bms.fetCharge ? "#27ae60" : "#999" }
                        Label { text: "放电 FET:" }
                        Label { text: bms.fetDischarge ? "● 开启" : "○ 关闭"
                                color: bms.fetDischarge ? "#27ae60" : "#999" }
                        Label { text: "均衡:" }
                        Label { text: bms.balancing ? "⚖ 均衡中" : "○ 未均衡"
                                color: bms.balancing ? "#e67e22" : "#999" }
                    }
                }
            }
        }

        // Row 1: 电芯摘要 (左) | 温度 (右)
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            GroupBox {
                title: "电芯摘要"
                Layout.fillWidth: true
                GridLayout {
                    anchors.fill: parent
                    columns: 2; columnSpacing: 16; rowSpacing: 4
                    Label { text: "最高:" }
                    Label { text: (bms.cellMax > 0 ? "C" + "?" : "--") + "  " + (bms.cellMax / 1000).toFixed(3) + " V" }
                    Label { text: "最低:" }
                    Label { text: (bms.cellMin > 0 ? "C" + "?" : "--") + "  " + (bms.cellMin / 1000).toFixed(3) + " V" }
                    Label { text: "压差:" }
                    Label { text: bms.cellDiff + " mV" }
                }
            }

            GroupBox {
                title: "温度"
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    Repeater {
                        model: bms.temperatureModel
                        RowLayout {
                            Label { text: "TS" + model.sensorIndex + ":"; Layout.preferredWidth: 35 }
                            Label { text: model.temperature.toFixed(1) + "°C"; color: model.tempColor }
                        }
                    }
                }
            }
        }
    }

    // 故障条（底部）
    footer: FaultRibbon {
        activeFaults: bms.activeFaults
        faultModel: bms.faultListModel
    }
}
