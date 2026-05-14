import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property color lineColor
    required property color accentColor
    required property color textColor
    required property color mutedTextColor

    color: "#141a1e"
    border.color: lineColor

    component SectionTitle: RowLayout {
        property string title: ""
        Layout.fillWidth: true
        spacing: 8

        Rectangle {
            width: 4
            height: 18
            radius: 2
            color: root.accentColor
        }

        Label {
            text: title
            color: root.textColor
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
            color: root.textColor
            font.pixelSize: 13
            font.bold: true
        }

        Label {
            text: hint
            color: root.mutedTextColor
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    ScrollView {
        id: scrollView

        anchors.fill: parent
        anchors.margins: 16
        clip: true

        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: contentLayout

            width: scrollView.availableWidth
            spacing: 14

            SectionTitle {
                title: "串口参数"
            }

            FieldLabel {
                title: "端口号"
                hint: "真实串口或虚拟串口，例如 COM10；SIM 表示内部仿真。"
            }

            TextField {
                Layout.fillWidth: true
                text: backend.portName
                placeholderText: "例如 COM10"
                selectByMouse: true

                onEditingFinished: {
                    backend.portName = text.trim()
                }
            }

            FieldLabel {
                title: "波特率"
                hint: "Modbus RTU 通信速率，默认 115200。"
            }

            SpinBox {
                Layout.fillWidth: true
                from: 1200
                to: 921600
                stepSize: 1200
                value: backend.baudRate
                editable: true

                textFromValue: function(value, locale) {
                    return value.toString()
                }

                valueFromText: function(text, locale) {
                    var cleaned = text.replace(/,/g, "").trim()
                    var parsed = parseInt(cleaned)
                    if (isNaN(parsed)) {
                        return backend.baudRate
                    }
                    return parsed
                }

                onValueModified: {
                    backend.baudRate = value
                }
            }

            FieldLabel {
                title: "数据位"
                hint: "串口数据位长度，工业设备常用 8。"
            }

            ComboBox {
                Layout.fillWidth: true
                model: [5, 6, 7, 8]
                currentIndex: Math.max(0, model.indexOf(backend.dataBits))

                onActivated: {
                    backend.dataBits = model[index]
                }
            }

            FieldLabel {
                title: "停止位"
                hint: "每帧结束标记，常用 1 位。"
            }

            ComboBox {
                Layout.fillWidth: true
                model: [1, 2]
                currentIndex: Math.max(0, model.indexOf(backend.stopBits))

                onActivated: {
                    backend.stopBits = model[index]
                }
            }

            FieldLabel {
                title: "校验位"
                hint: "None 无校验，Even 偶校验，Odd 奇校验。"
            }

            ComboBox {
                Layout.fillWidth: true
                model: ["None", "Even", "Odd"]
                currentIndex: Math.max(0, model.indexOf(backend.parity))

                onActivated: {
                    backend.parity = model[index]
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.lineColor
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

                onValueModified: {
                    backend.slaveId = value
                }
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

                onValueModified: {
                    backend.refreshInterval = value
                }
            }

            /*
              关键修改：
              原来这里是 RowLayout + FieldLabel + Switch。
              FieldLabel 设置 Layout.fillWidth 后，在窄面板中会把 Switch 挤到右侧不可见。
              现在改成独立卡片，并固定 Switch 在右侧。
            */
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: simulationRow.implicitHeight + 22
                radius: 8
                color: "#10171b"
                border.color: root.lineColor

                RowLayout {
                    id: simulationRow

                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 3

                        Label {
                            text: "仿真模式"
                            color: root.textColor
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Label {
                            text: "开启：使用内部模拟数据；关闭：访问真实串口 / HHD / Modbus Slave。"
                            color: root.mutedTextColor
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Label {
                            text: backend.simulationMode ? "当前：内部仿真数据" : "当前：真实串口通信"
                            color: backend.simulationMode ? "#ffbf3f" : root.accentColor
                            font.pixelSize: 12
                            font.bold: true
                            Layout.topMargin: 2
                        }
                    }

                    Switch {
                        id: simulationSwitch

                        Layout.alignment: Qt.AlignVCenter
                        checked: backend.simulationMode
                        text: checked ? "开" : "关"

                        onToggled: {
                            backend.simulationMode = checked
                        }
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: "保存配置"

                onClicked: {
                    backend.saveConfig()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.lineColor
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
                    Layout.fillWidth: true
                    text: modelData
                    color: "#b7c4c0"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            Item {
                Layout.fillWidth: true
                height: 16
            }
        }
    }
}
