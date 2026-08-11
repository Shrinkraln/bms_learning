import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Page {
    title: "电芯"
    property var selectedCells: ([])

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12
        spacing: 8

        // 电压柱状图
        GroupBox {
            title: "电芯电压 (" + bms.cellDiff + " mV 压差)"
            Layout.fillWidth: true; Layout.preferredHeight: 260

            RowLayout {
                anchors.fill: parent
                spacing: 4

                // Y 轴标签 — 与 bar 可视区域对齐 (bar 范围 120px, 从 y≈58 到 y≈178)
                Item {
                    Layout.preferredWidth: 36
                    Layout.fillHeight: true

                    Column {
                        anchors.top: parent.top
                        anchors.topMargin: 58   // 对齐 bar 顶部 (4300mV 对应 bar.top)
                        spacing: 0
                        Repeater {
                            model: ["4.3V","4.0V","3.7V","3.4V","3.1V","2.8V"]
                            Text {
                                text: modelData
                                font.pixelSize: 9; color: "#999"
                                width: 36; height: 20
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                // 电芯柱
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: bms.cellVoltageModel
                        CellBar {
                            voltage: model.voltage
                            cellIndex: model.cellIndex
                            isMax: model.isMax
                            isMin: model.isMin
                            barColor: model.barColor
                        }
                    }
                }
            }
        }

        // 均衡勾选
        GroupBox {
            title: "均衡选择"
            RowLayout {
                spacing: 8
                Repeater {
                    model: 9
                    CheckBox {
                        text: "C" + (index + 1)
                        checked: false
                        onCheckedChanged: {
                            if (checked) {
                                if (selectedCells.indexOf(index) < 0) selectedCells.push(index)
                            } else {
                                var i = selectedCells.indexOf(index)
                                if (i >= 0) selectedCells.splice(i, 1)
                            }
                            selectedCellsChanged()
                        }
                    }
                }
            }
        }

        // 温度条
        GroupBox {
            title: "温度"
            Layout.fillWidth: true
            RowLayout {
                spacing: 12
                Repeater {
                    model: bms.temperatureModel
                    RowLayout {
                        Label { text: "TS" + model.sensorIndex + ":"; font.bold: true }
                        Rectangle {
                            width: Math.max(4, model.temperature * 2)
                            height: 20; radius: 3; color: model.tempColor
                        }
                        Label { text: model.temperature.toFixed(1) + "°C" }
                    }
                }
            }
        }
    }
}
