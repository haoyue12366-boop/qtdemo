import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Industrial 1.0

Rectangle {
    id: root

    required property color panelColor
    required property color lineColor
    required property color textColor
    required property color mutedTextColor

    radius: 8
    color: panelColor
    border.color: lineColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Label {
            text: "温度实时波形"
            color: root.textColor
            font.pixelSize: 17
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0c1114"
            border.color: "#253036"

            Repeater {
                model: 5
                delegate: Rectangle {
                    x: 46
                    y: 18 + index * ((parent.height - 42) / 4)
                    width: parent.width - 62
                    height: 1
                    color: "#1f2a2f"
                }
            }

            Repeater {
                model: 6
                delegate: Rectangle {
                    x: 46 + index * ((parent.width - 62) / 5)
                    y: 18
                    width: 1
                    height: parent.height - 42
                    color: "#1f2a2f"
                }
            }

            Rectangle {
                x: 46
                y: 18
                width: 1
                height: parent.height - 42
                color: "#5e7470"
            }

            Rectangle {
                x: 46
                y: parent.height - 24
                width: parent.width - 62
                height: 1
                color: "#5e7470"
            }

            Label {
                x: 6
                y: 10
                width: 38
                text: backend.temperatureMax.toFixed(1)
                color: root.mutedTextColor
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 11
            }

            Label {
                x: 6
                y: parent.height - 36
                width: 38
                text: backend.temperatureMin.toFixed(1)
                color: root.mutedTextColor
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 11
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 8
                spacing: 12

                Label {
                    text: "Min " + backend.temperatureMin.toFixed(1)
                    color: root.mutedTextColor
                    font.pixelSize: 12
                }

                Label {
                    text: "Avg " + backend.temperatureAvg.toFixed(1)
                    color: root.mutedTextColor
                    font.pixelSize: 12
                }

                Label {
                    text: "Max " + backend.temperatureMax.toFixed(1)
                    color: root.mutedTextColor
                    font.pixelSize: 12
                }
            }

            CurvePlotProvider {
                anchors.fill: parent
                anchors.leftMargin: 46
                anchors.rightMargin: 16
                anchors.topMargin: 18
                anchors.bottomMargin: 24
                dataPoints: backend.temperatureData
            }
        }
    }
}
