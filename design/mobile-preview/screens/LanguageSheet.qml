import QtQuick
import QtQuick.Layouts
import "../"

Item {
    id: root

    property real italianProgress: 0

    SettingsScreen {
        anchors.fill: parent
    }

    SequentialAnimation {
        running: true
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "italianProgress"
            from: 0
            to: 1
            duration: 3000
        }

        PauseAnimation {
            duration: 600
        }

        NumberAnimation {
            target: root
            property: "italianProgress"
            to: 0
            duration: 0
        }
    }

    BottomSheet {
        heading: "Languages"

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: rows.implicitHeight
                radius: Theme.radiusMedium
                color: Theme.panel
                border.color: Theme.line
                border.width: 1
                clip: true

                ColumnLayout {
                    id: rows

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: 0

                    SettingsRow {
                        title: "French"

                        Text {
                            text: "✓"
                            font.pixelSize: 16
                            font.family: Theme.fontFamily
                            color: Theme.ink
                        }
                    }

                    SettingsRow {
                        title: "Spanish"

                        Rectangle {
                            width: spanishLabel.implicitWidth + 24
                            height: 32
                            radius: Theme.radiusSmall
                            color: "transparent"
                            border.color: Theme.line
                            border.width: 1

                            Text {
                                id: spanishLabel
                                anchors.centerIn: parent
                                text: "Download 64 MB"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                color: Theme.ink
                            }
                        }
                    }

                    SettingsRow {
                        title: "Italian"

                        ColumnLayout {
                            width: 96
                            spacing: 4

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 6
                                radius: 3
                                color: Theme.faint

                                Rectangle {
                                    width: parent.width * italianProgress
                                    height: parent.height
                                    radius: 3
                                    color: Theme.accent
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: Math.round(italianProgress * 100) + "%"
                                font.pixelSize: 11
                                font.family: Theme.fontFamily
                                color: Theme.mid
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    SettingsRow {
                        title: "German"
                        description: "needs a newer Ikonika"
                        lastRow: true
                    }
                }
            }
        }
    }
}
