import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Industrial 1.0

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

    component SectionTitle: RowLayout {
        property string title: ""
        Layout.fillWidth: true
        spacing: 8

        Rectangle {
            width: 4
            height: 18
            radius: 2
            color: window.accentColor
        }

        Label {
            text: title
            color: window.textColor
            font.pixelSize: 18
            font.bold: true
        }
    }

    component FieldLabel: ColumnLayout {
        property string title: ""
        property string hint: ""
        Layout.fillWidth: true
        spacing: 2

        Label {
            text: title
            color: window.textColor
            font.pixelSize: 13
            font.bold: true
        }

        Label {
            text: hint
            color: window.mutedTextColor
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    component StatTile: Rectangle {
        property string title: ""
        property string value: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 64
        radius: 8
        color: "#12191d"
        border.color: window.lineColor

        Column {
            anchors.centerIn: parent
            spacing: 4
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: title
                color: window.mutedTextColor
                font.pixelSize: 12
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: value
                color: window.textColor
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

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

            Rectangle {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                color: "#141a1e"
                border.color: window.lineColor

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 16

                    ColumnLayout {
                        width: parent.width
                        spacing: 14

                        SectionTitle { title: "串口参数" }

                        FieldLabel {
                            title: "端口号"
                            hint: "Windows 常见为 COM3、COM4；无真实串口时会进入仿真模式。"
                        }

                        TextField {
                            Layout.fillWidth: true
                            text: backend.portName
                            placeholderText: "端口名"
                            onEditingFinished: backend.portName = text
                        }

                        FieldLabel {
                            title: "波特率"
                            hint: "Modbus RTU 通信速率，默认 115200。"
                        }

                        SpinBox {
                            Layout.fillWidth: true
                            from: 1200
                            to: 921600
                            value: backend.baudRate
                            editable: true
                            onValueModified: backend.baudRate = value
                        }

                        FieldLabel {
                            title: "数据位"
                            hint: "串口数据位长度，工业设备常用 8。"
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [5, 6, 7, 8]
                            currentIndex: Math.max(0, model.indexOf(backend.dataBits))
                            onActivated: backend.dataBits = model[index]
                        }

                        FieldLabel {
                            title: "停止位"
                            hint: "每帧结束标记，常用 1 位。"
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [1, 2]
                            currentIndex: Math.max(0, model.indexOf(backend.stopBits))
                            onActivated: backend.stopBits = model[index]
                        }

                        FieldLabel {
                            title: "校验位"
                            hint: "None 无校验，Even 偶校验，Odd 奇校验。"
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: ["None", "Even", "Odd"]
                            currentIndex: Math.max(0, model.indexOf(backend.parity))
                            onActivated: backend.parity = model[index]
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: window.lineColor
                            Layout.topMargin: 8
                        }

                        SectionTitle {
                            title: "设备参数"
                            Layout.topMargin: 8
                        }

                        FieldLabel {
                            title: "从机站号"
                            hint: "Modbus RTU 从机地址，合法范围 1 到 247。"
                        }

                        SpinBox {
                            Layout.fillWidth: true
                            from: 1
                            to: 247
                            value: backend.slaveId
                            editable: true
                            onValueModified: backend.slaveId = value
                        }

                        FieldLabel {
                            title: "刷新周期"
                            hint: "轮询保持寄存器的周期，单位 ms，默认 100。"
                        }

                        SpinBox {
                            Layout.fillWidth: true
                            from: 20
                            to: 5000
                            stepSize: 10
                            value: backend.refreshInterval
                            editable: true
                            onValueModified: backend.refreshInterval = value
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            FieldLabel {
                                Layout.fillWidth: true
                                title: "仿真模式"
                                hint: "开启后不访问真实串口，由通信线程生成 Modbus RTU 响应。"
                            }

                            Switch {
                                checked: backend.simulationMode
                                onToggled: backend.simulationMode = checked
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "保存配置"
                            onClicked: backend.saveConfig()
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: window.lineColor
                            Layout.topMargin: 8
                        }

                        SectionTitle {
                            title: "寄存器地址表"
                            Layout.topMargin: 8
                        }

                        Repeater {
                            model: [
                                "40001 温度 Float 只读",
                                "40003 压力 Float 只读",
                                "40005 液位 Float 只读",
                                "40007 流量 Float 只读",
                                "40009 报警 UINT16 读写"
                            ]
                            delegate: Label {
                                text: modelData
                                color: "#b7c4c0"
                                font.pixelSize: 13
                            }
                        }
                    }
                }
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

                    GridLayout {
                        Layout.fillWidth: true
                        columns: window.width < 980 ? 2 : 4
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
                                color: window.panelColor
                                border.color: window.lineColor

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
                                        color: window.accentColor
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

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 126
                        radius: 8
                        color: window.panelColor
                        border.color: window.lineColor

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: "通信统计"
                                    color: window.textColor
                                    font.pixelSize: 17
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: backend.communicationModeText
                                    color: backend.communicationModeText === "仿真模式" ? "#ffcc66" : window.accentColor
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: window.width < 1060 ? 3 : 6
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

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300
                        radius: 8
                        color: window.panelColor
                        border.color: window.lineColor

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Label {
                                text: "温度实时波形"
                                color: window.textColor
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
                                    color: window.mutedTextColor
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 11
                                }

                                Label {
                                    x: 6
                                    y: parent.height - 36
                                    width: 38
                                    text: backend.temperatureMin.toFixed(1)
                                    color: window.mutedTextColor
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
                                        color: window.mutedTextColor
                                        font.pixelSize: 12
                                    }
                                    Label {
                                        text: "Avg " + backend.temperatureAvg.toFixed(1)
                                        color: window.mutedTextColor
                                        font.pixelSize: 12
                                    }
                                    Label {
                                        text: "Max " + backend.temperatureMax.toFixed(1)
                                        color: window.mutedTextColor
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

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(360, window.height - 430)
                        radius: 8
                        color: window.panelColor
                        border.color: window.lineColor

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Label {
                                text: "报警日志"
                                color: window.textColor
                                font.pixelSize: 17
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
                                        color: "#edf5f2"
                                        font.pixelSize: 13
                                        text: model.display
                                    }
                                }
                            }
                        }
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
