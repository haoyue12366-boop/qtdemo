import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property int windowWidth
    required property color panelColor
    required property color lineColor
    required property color accentColor
    required property color textColor
    required property color mutedTextColor

    radius: 8
    color: panelColor
    border.color: lineColor

    component StatTile: Rectangle {
        property string title: ""
        property string value: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 64
        radius: 8
        color: "#12191d"
        border.color: root.lineColor

        Column {
            anchors.centerIn: parent
            spacing: 4

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: title
                color: root.mutedTextColor
                font.pixelSize: 12
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: value
                color: root.textColor
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "通信统计"
                color: root.textColor
                font.pixelSize: 17
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: backend.communicationModeText
                color: backend.communicationModeText === "仿真模式" ? "#ffcc66" : root.accentColor
                font.pixelSize: 13
                font.bold: true
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.windowWidth < 1060 ? 3 : 6
            columnSpacing: 8
            rowSpacing: 8

            StatTile { title: "TX"; value: backend.txCount }
            StatTile { title: "RX"; value: backend.rxCount }
            StatTile { title: "超时"; value: backend.timeoutCount }
            StatTile { title: "重试"; value: backend.retryCount }
            StatTile { title: "CRC"; value: backend.crcErrorCount }
            StatTile { title: "异常"; value: backend.exceptionCount }
        }
    }
}
