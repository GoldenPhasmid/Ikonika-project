import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    headerTitle: "Ikonika"
    showSearch: false
    showTabBar: false

    readonly property var downloadItems: [
        { label: "Dictionary", size: "25 MB", checked: true },
        { label: "Voice", size: "34 MB", checked: false },
        { label: "Sentence translation", size: "68 MB", checked: false }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        Text {
            text: "What are you learning?"
            font.pixelSize: 28
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "LEARNING"
                font.pixelSize: 11
                font.weight: Font.Bold
                font.family: Theme.fontFamily
                font.letterSpacing: 1
                color: Theme.mid
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 47
                radius: Theme.radiusMedium
                color: Theme.panel
                border.color: Theme.line
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Text {
                        text: "French"
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        font.family: Theme.fontFamily
                        color: Theme.ink
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "⌄"
                        font.pixelSize: 16
                        color: Theme.ink
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "FROM"
                font.pixelSize: 11
                font.weight: Font.Bold
                font.family: Theme.fontFamily
                font.letterSpacing: 1
                color: Theme.mid
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 47
                radius: Theme.radiusMedium
                color: Theme.panel
                border.color: Theme.line
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Text {
                        text: "English"
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        font.family: Theme.fontFamily
                        color: Theme.ink
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "⌄"
                        font.pixelSize: 16
                        color: Theme.ink
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusMedium
            color: Theme.panel
            border.color: Theme.line
            border.width: 1
            clip: true
            implicitHeight: itemsColumn.implicitHeight

            ColumnLayout {
                id: itemsColumn
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                Repeater {
                    model: downloadItems
                    delegate: SettingsRow {
                        id: itemDelegate

                        required property var modelData
                        required property int index

                        title: itemDelegate.modelData.label
                        lastRow: itemDelegate.index === downloadItems.length - 1

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: itemDelegate.modelData.size
                            font.pixelSize: 12
                            font.family: Theme.fontFamily
                            color: Theme.mid
                        }

                        Checkbox {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: itemDelegate.modelData.checked
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        WideButton {
            label: "Continue · 25 MB"
        }
    }
}
