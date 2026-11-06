import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    headerTitle: "Ikonika"
    showSearch: false
    showTabBar: false

    readonly property var downloadItems: [
        { label: "Dictionary", status: "16 of 25 MB", statusMuted: false, progress: 0.67 },
        { label: "Voice", status: "waiting", statusMuted: true, progress: 0.0 },
        { label: "Sentence translation", status: "waiting", statusMuted: true, progress: 0.0 }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        Text {
            text: "Downloading French"
            font.pixelSize: 28
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
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
                    delegate: ColumnLayout {
                        id: itemDelegate
                        readonly property bool isLast: index === downloadItems.length - 1

                        Layout.fillWidth: true
                        spacing: 0

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 16
                            Layout.rightMargin: 16
                            Layout.topMargin: 16
                            Layout.bottomMargin: 16
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: modelData.label
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    font.family: Theme.fontFamily
                                    color: Theme.ink
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.status
                                    font.pixelSize: 13
                                    font.family: Theme.fontFamily
                                    color: modelData.statusMuted ? Theme.mid : Theme.ink
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 6
                                radius: 3
                                color: Theme.faint

                                Rectangle {
                                    width: parent.width * modelData.progress
                                    height: parent.height
                                    radius: 3
                                    color: Theme.ink
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.line
                            visible: !itemDelegate.isLast
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        WideButton {
            label: "Start learning"
            enabled: false
        }
    }
}
