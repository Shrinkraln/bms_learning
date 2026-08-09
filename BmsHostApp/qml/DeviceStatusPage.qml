import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "设备状态"
    id: deviceStatusPage

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        spacing: 0

        // 标题行
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "通信链路健康度: " + commMonitor.healthSummary + "/3"
            font.pixelSize: 18
            font.bold: true
            color: commMonitor.healthSummary === 3 ? "#27ae60"
                 : commMonitor.healthSummary >= 2 ? "#f39c12" : "#e74c3c"
        }

        Item { Layout.preferredHeight: 24 }

        // === Layer 1: PCAN 硬件 ===
        LayerCard {
            layerName: "PCAN 硬件"
            statusColor: commMonitor.hardwareConnected ? "#27ae60" : "#e74c3c"
            statusText: {
                if (commMonitor.hardwareConnected) return "● 设备已连接"
                if (commMonitor.reconnectCount > 0) return "● 设备丢失 (重连中...)"
                return "— 等待设备"
            }
            blinkState: commMonitor.hardwareConnected ? "solid"
                       : commMonitor.reconnectCount > 0 ? "fast" : "solid"
            metrics: "丢失计数: " + commMonitor.reconnectCount
            metricsVisible: commMonitor.reconnectCount > 0
        }

        // 层间连线
        ConnectorLine {
            color: commMonitor.hardwareConnected ? "#27ae60" : "#999"
        }

        // === Layer 2: CAN 总线 ===
        LayerCard {
            layerName: "CAN 总线"
            statusColor: {
                if (!commMonitor.hardwareConnected) return "#999"
                if (!commMonitor.canBusActive) return "#e74c3c"
                // 黄色过渡: canBusActive==true && canFrameRate==0 (刚恢复)
                if (commMonitor.canFrameRate <= 0.0) return "#f39c12"
                return "#27ae60"
            }
            statusText: {
                if (!commMonitor.hardwareConnected) return "— 链路中断"
                if (!commMonitor.canBusActive) return "● 帧超时"
                if (commMonitor.canFrameRate <= 0.0) return "● 等待首帧..."
                return "● 帧率正常"
            }
            blinkState: {
                if (!commMonitor.hardwareConnected) return "solid"
                if (!commMonitor.canBusActive) return "fast"
                if (commMonitor.canFrameRate <= 0.0) return "slow"
                return "solid"
            }
            metrics: commMonitor.canFrameRate.toFixed(0) + " fps  |  "
                     + commMonitor.totalFrameCount.toLocaleString() + " 帧  |  "
                     + "超时 " + commMonitor.timeoutEventCount + " 次"
            metricsVisible: commMonitor.hardwareConnected
        }

        // 层间连线
        ConnectorLine {
            color: {
                if (!commMonitor.hardwareConnected) return "#999"
                if (!commMonitor.canBusActive) return "#e74c3c"
                if (commMonitor.canFrameRate <= 0.0) return "#f39c12"
                return "#27ae60"
            }
        }

        // === Layer 3: AFE 通信 ===
        LayerCard {
            layerName: "AFE 芯片 (I2C)"
            statusColor: {
                if (!commMonitor.hardwareConnected || !commMonitor.canBusActive) return "#999"
                if (!commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0) return "#e74c3c"
                if (!commMonitor.afeOnline) return "#f39c12"
                return "#27ae60"
            }
            statusText: {
                if (!commMonitor.hardwareConnected || !commMonitor.canBusActive) return "— 链路中断"
                if (!commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0)
                    return "● 芯片离线"
                if (!commMonitor.afeOnline) return "— 等待数据"
                return "● 芯片在线"
            }
            blinkState: {
                if (!commMonitor.hardwareConnected || !commMonitor.canBusActive) return "solid"
                if (!commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0) return "fast"
                if (!commMonitor.afeOnline) return "slow"
                return "solid"
            }
            metrics: {
                if (commMonitor.afeOnline) return ""
                if (commMonitor.afeOfflineSeconds > 0) {
                    var s = commMonitor.afeOfflineSeconds
                    var m = Math.floor(s / 60)
                    s = s % 60
                    return "离线: " + m + "分" + s + "秒"
                }
                return ""
            }
            metricsVisible: !commMonitor.afeOnline && commMonitor.afeOfflineSeconds > 0
        }
    }

    // === 子组件 ===

    // LayerCard: 单层状态卡片
    component LayerCard: Rectangle {
        property string layerName: ""
        property color statusColor: "#999"
        property string statusText: ""
        property string blinkState: "solid"
        property string metrics: ""
        property bool metricsVisible: false

        width: parent.width
        height: metricsVisible ? 72 : 52
        radius: 8
        color: "#fafafa"
        border.color: "#e0e0e0"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // 状态指示灯
            Rectangle {
                width: 16; height: 16; radius: 8
                color: statusColor
                opacity: 1.0

                // 脉冲动画
                SequentialAnimation on opacity {
                    running: blinkState !== "solid"
                    loops: Animation.Infinite
                    PropertyAnimation {
                        from: 1.0; to: 0.15
                        duration: blinkState === "fast" ? 250 : 1000
                    }
                    PropertyAnimation {
                        from: 0.15; to: 1.0
                        duration: blinkState === "fast" ? 250 : 1000
                    }
                }
            }

            ColumnLayout {
                spacing: 2
                Text {
                    text: layerName
                    font.pixelSize: 14
                    font.bold: true
                    color: "#333"
                }
                Text {
                    text: statusText
                    font.pixelSize: 13
                    color: statusColor
                }
                Text {
                    visible: metricsVisible
                    text: metrics
                    font.pixelSize: 11
                    color: "#999"
                }
            }
        }
    }

    // ConnectorLine: 层间连接线
    component ConnectorLine: Rectangle {
        property color lineColor: "#27ae60"

        width: 2
        height: 8
        anchors.horizontalCenter: parent.horizontalCenter
        color: lineColor
    }
}
