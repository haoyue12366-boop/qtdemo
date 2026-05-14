import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: root

    required property color panelColor
    required property color lineColor
    required property color textColor

    radius: 8
    color: panelColor
    border.color: lineColor

    FileDialog {
        id: exportDialog
        title: "选择报警CSV导出位置"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)"]
        defaultSuffix: "csv"

        onAccepted: backend.exportAlarmCsv(selectedFile.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "报警日志"
                color: root.textColor
                font.pixelSize: 17
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "导出 CSV"
                enabled: !backend.backgroundTaskRunning
                onClicked: exportDialog.open()
            }

            Button {
                text: "刷新历史"
                enabled: !backend.alarmHistoryLoading
                onClicked: backend.queryAlarmHistory(startField.text, endField.text,
                                                     keywordField.text, statusFilter.currentText,
                                                     limitSpin.value, offsetSpin.value)
            }

            Button {
                text: "刷新统计"
                enabled: !backend.alarmStatsLoading
                onClicked: backend.refreshAlarmStats(startField.text, endField.text)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: backend.backgroundTaskRunning || backend.backgroundTaskMessage.length > 0
            spacing: 8

            ProgressBar {
                Layout.preferredWidth: 180
                visible: backend.backgroundTaskRunning
                value: backend.backgroundTaskProgress / 100.0
            }

            Label {
                Layout.fillWidth: true
                text: backend.backgroundTaskMessage
                color: "#b7c4c0"
                elide: Text.ElideRight
                font.pixelSize: 12
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 6
            columnSpacing: 8
            rowSpacing: 6

            Label { text: "开始"; color: "#9dafaa"; font.pixelSize: 12 }
            TextField {
                id: startField
                Layout.fillWidth: true
                placeholderText: "yyyy-MM-dd HH:mm:ss"
                font.pixelSize: 12
            }

            Label { text: "结束"; color: "#9dafaa"; font.pixelSize: 12 }
            TextField {
                id: endField
                Layout.fillWidth: true
                placeholderText: "yyyy-MM-dd HH:mm:ss"
                font.pixelSize: 12
            }

            Label { text: "状态"; color: "#9dafaa"; font.pixelSize: 12 }
            ComboBox {
                id: statusFilter
                Layout.preferredWidth: 104
                model: ["全部", "激活", "已确认"]
            }

            Label { text: "关键字"; color: "#9dafaa"; font.pixelSize: 12 }
            TextField {
                id: keywordField
                Layout.fillWidth: true
                placeholderText: "变量/报警值/状态"
                font.pixelSize: 12
            }

            Label { text: "数量"; color: "#9dafaa"; font.pixelSize: 12 }
            SpinBox {
                id: limitSpin
                Layout.preferredWidth: 96
                from: 1
                to: 5000
                value: 500
            }

            Label { text: "偏移"; color: "#9dafaa"; font.pixelSize: 12 }
            SpinBox {
                id: offsetSpin
                Layout.preferredWidth: 96
                from: 0
                to: 1000000
                value: 0
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 14
            rowSpacing: 4

            Repeater {
                model: [
                    "总数 " + backend.alarmTotalCount,
                    "激活 " + backend.alarmActiveCount,
                    "已确认 " + backend.alarmAcknowledgedCount,
                    "最近 " + (backend.latestAlarmTime.length > 0 ? backend.latestAlarmTime : "-"),
                    "温度 " + backend.temperatureAlarmCount,
                    "压力 " + backend.pressureAlarmCount,
                    "液位 " + backend.levelAlarmCount,
                    "流量 " + backend.flowAlarmCount
                ]

                delegate: Label {
                    text: modelData
                    color: "#d7efe1"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: backend.alarmHistoryLoading ? "历史报警查询中..." : backend.alarmHistoryMessage
            color: "#9dafaa"
            elide: Text.ElideRight
            font.pixelSize: 12
        }

        HorizontalHeaderView {
            Layout.fillWidth: true
            syncView: historyTable
            clip: true
        }

        TableView {
            id: historyTable
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            clip: true
            model: backend.alarmHistoryModel
            columnSpacing: 1
            rowSpacing: 1

            delegate: Rectangle {
                implicitWidth: column === 0 ? 190 : column === 1 ? 120 : column === 2 ? 150 : 96
                implicitHeight: 30
                color: "#11181c"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: "#edf5f2"
                    font.pixelSize: 12
                    text: model.display
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.lineColor
        }

        Label {
            text: "实时报警"
            color: root.textColor
            font.pixelSize: 14
            font.bold: true
        }

        HorizontalHeaderView {
            id: horizontalHeader
            Layout.fillWidth: true
            syncView: tableView
            clip: true
        }

        TableView {
            id: tableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.alarmModel
            columnSpacing: 1
            rowSpacing: 1

            Connections {
                target: backend.alarmModel

                function onRowsInserted(parent, first, last) {
                    tableView.contentY = 0
                }
            }

            delegate: Rectangle {
                implicitWidth: column === 0 ? 190 : column === 1 ? 120 : column === 2 ? 220 : 110
                implicitHeight: 32
                color: column === 3 ? model.statusColor : "#11181c"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: model.foregroundColor ? model.foregroundColor : "#edf5f2"
                    font.pixelSize: 13
                    text: model.display
                }
            }
        }
    }
}
