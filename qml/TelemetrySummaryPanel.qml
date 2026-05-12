import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: root

    required property int windowWidth
    required property color panelColor
    required property color lineColor
    required property color accentColor

    columns: windowWidth < 980 ? 2 : 4
    columnSpacing: 12
    rowSpacing: 12

    Repeater {
        model: [
            { name: "温度", value: backend.temperature.toFixed(1), unit: "C" },
            { name: "压力", value: backend.pressure.toFixed(2), unit: "MPa" },
            { name: "液位", value: backend.level.toFixed(2), unit: "m" },
            { name: "流量", value: backend.flow.toFixed(1), unit: "m3/h" }
        ]

        delegate: Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            radius: 8
            color: root.panelColor
            border.color: root.lineColor

            Column {
                anchors.centerIn: parent
                spacing: 6

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.name
                    color: "#b7c4c0"
                    font.pixelSize: 15
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.value
                    color: root.accentColor
                    font.pixelSize: 30
                    font.bold: true
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.unit
                    color: "#d5ddda"
                    font.pixelSize: 13
                }
            }
        }
    }
}
