import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "参数配置"
    width: 420; height: 500
    standardButtons: Dialog.Close

    property var params: [
        { label: "单芯过压 (mV)",    paramId: 0x01, min: 3000, max: 5000, unit: "mV" },
        { label: "单芯欠压 (mV)",    paramId: 0x02, min: 2000, max: 3500, unit: "mV" },
        { label: "放电过流 (mA)",    paramId: 0x03, min: 1000, max: 100000, unit: "mA" },
        { label: "充电过流 (mA)",    paramId: 0x04, min: 1000, max: 50000, unit: "mA" },
        { label: "均衡阈值 (mV)",    paramId: 0x05, min: 10,   max: 500, unit: "mV" },
        { label: "均衡最低电压 (mV)", paramId: 0x06, min: 2500, max: 4000, unit: "mV" }
    ]
    // 已修改参数: paramId -> value (SpinBox 值始终在合法范围内)
    property var values: ({})

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Repeater {
            model: root.params
            RowLayout {
                Label { text: modelData.label; Layout.preferredWidth: 150 }
                SpinBox {
                    id: spinBox
                    from: modelData.min; to: modelData.max
                    editable: true
                    Layout.fillWidth: true
                    property bool valid: value >= modelData.min && value <= modelData.max
                    onValueChanged: root.values[modelData.paramId] = value
                }
                Label { text: modelData.unit; color: "#999" }
            }
        }

        Button {
            text: "发送配置"
            Layout.alignment: Qt.AlignRight
            enabled: bms.connectionStatus === 1
            onClicked: {
                var sent = 0
                for (var i = 0; i < root.params.length; i++) {
                    var p = root.params[i]
                    var v = root.values[p.paramId]
                    if (v !== undefined) {
                        bms.sendConfig(p.paramId, v)
                        sent++
                    }
                }
                sendHint.text = sent === 0 ? "未修改任何参数" : "已发送 " + sent + " 项配置"
            }
        }

        Label {
            text: "注意: 配置掉电不保存。阈值不通过 CAN 回读。"
            color: "#999"; font.pixelSize: 11
            Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        Label { id: sendHint; color: "#27ae60"; font.pixelSize: 11 }
    }
}
