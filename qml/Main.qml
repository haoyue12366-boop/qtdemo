import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1320
    height: 820
    visible: true
    title: "qthmi 工业组态仿真监控系统"
    color: "#101417"

    readonly property color panelColor: "#182025"
    readonly property color lineColor: "#2f3a40"
    readonly property color accentColor: "#00ff9d"
    readonly property color textColor: "#eef4f2"
    readonly property color mutedTextColor: "#9dafaa"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: "#151b1f"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 18

                Label {
                    text: "qthmi 工业监控"
                    color: window.textColor
                    font.pixelSize: 22
                    font.bold: true
                }

                Label {
                    text: backend.connected ? "已连接" : "未连接"
                    color: backend.connected ? window.accentColor : "#ffb020"
                    font.pixelSize: 14
                }

                Label {
                    text: "从机 " + backend.slaveId
                    color: "#b7c4c0"
                    font.pixelSize: 14
                }

                Label {
                    text: "运行 " + backend.uptimeText
                    color: "#b7c4c0"
                    font.pixelSize: 14
                }

                Label {
                    Layout.maximumWidth: 360
                    text: "最近错误: " + backend.recentError
                    color: backend.statusError ? "#ffb3b3" : "#8fa39c"
                    elide: Text.ElideRight
                    font.pixelSize: 14
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: backend.connected ? "断开" : "连接"
                    onClicked: backend.connected ? backend.disconnectFromDevice() : backend.connectToDevice()
                }

                Button {
                    text: "确认报警"
                    onClicked: backend.acknowledgeAlarm()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ConfigPanel {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                lineColor: window.lineColor
                accentColor: window.accentColor
                textColor: window.textColor
                mutedTextColor: window.mutedTextColor
            }

            ScrollView {
                id: rightScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: rightScroll.availableWidth
                    spacing: 12
                    anchors.margins: 16

                    TelemetrySummaryPanel {
                        Layout.fillWidth: true
                        windowWidth: window.width
                        panelColor: window.panelColor
                        lineColor: window.lineColor
                        accentColor: window.accentColor
                    }

                    CommunicationStatsPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 126
                        windowWidth: window.width
                        panelColor: window.panelColor
                        lineColor: window.lineColor
                        accentColor: window.accentColor
                        textColor: window.textColor
                        mutedTextColor: window.mutedTextColor
                    }

                    TrendPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300
                        panelColor: window.panelColor
                        lineColor: window.lineColor
                        textColor: window.textColor
                        mutedTextColor: window.mutedTextColor
                    }

                    AlarmPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(700, window.height - 180)
                        panelColor: window.panelColor
                        lineColor: window.lineColor
                        textColor: window.textColor
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 34
            color: backend.statusError ? "#3a1414" : "#13251b"

            Text {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                text: backend.statusText
                color: backend.statusError ? "#ffb3b3" : "#b9f4d5"
                font.pixelSize: 13
            }
        }
    }
}
