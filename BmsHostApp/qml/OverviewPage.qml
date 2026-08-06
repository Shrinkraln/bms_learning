import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Page {
    title: "总览"
    ScrollView {
        anchors.fill: parent
        GridLayout {
            columns: 2; rowSpacing: 12; columnSpacing: 16
            anchors.margins: 16; anchors.left: parent.left

            // SOC 仪表
            GroupBox {
                title: "SOC"
                Layout.rowSpan: 2
                GaugeArc {
                    width: 180; height: 180
                    value: bms.socPermil
                }
            }

            // 关键数值
            GroupBox {
                title: "电池状态"
                GridLayout { columns: 2; columnSpacing: 12; rowSpacing: 4
                    Label { text: "总压:" } Label { text: (bms.packVoltage / 1000).toFixed(2) + " V"; font.bold: true }
                    Label { text: "电流:" } Label { text: (bms.packCurrent / 1000).toFixed(2) + " A"; font.bold: true
                               color: bms.packCurrent < 0 ? "#e74c3c" : "#27ae60" }
                    Label { text: "剩余容量:" } Label { text: (bms.remainingMah / 1000).toFixed(2) + " Ah" }
                    Label { text: "Q_max:" } Label { text: (bms.qMaxMah / 1000).toFixed(2) + " Ah" }
                }
            }

            // FET + 均衡
            GroupBox {
                title: "FET & 均衡"
                GridLayout { columns: 2; columnSpacing: 8
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

            // 电芯摘要
            GroupBox {
                title: "电芯"
                GridLayout { columns: 2; columnSpacing: 12
                    Label { text: "最高:" } Label { text: "C" + (bms.cellMax > 0 ? "..." : "--") + " " + (bms.cellMax/1000).toFixed(3) + "V" }
                    Label { text: "最低:" } Label { text: "C" + (bms.cellMin > 0 ? "..." : "--") + " " + (bms.cellMin/1000).toFixed(3) + "V" }
                    Label { text: "压差:" } Label { text: bms.cellDiff + " mV" }
                }
            }

            // 温度摘要
            GroupBox {
                title: "温度"
                GridLayout { columns: 2; columnSpacing: 8
                    Repeater {
                        model: bms.temperatureModel
                        Label { text: "TS" + model.sensorIndex + ": " + model.temperature.toFixed(1) + "°C"
                                color: model.tempColor }
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
