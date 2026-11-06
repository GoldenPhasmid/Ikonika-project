import QtQuick
import QtQuick.Layouts
import "../"

Item {
    SettingsScreen {
        anchors.fill: parent
    }

    BottomSheet {
        heading: "Delete everything"

        Text {
            Layout.fillWidth: true
            text: "Deletes both local and cloud user information. Do you wish to proceed?"
            font.pixelSize: 14
            font.family: Theme.fontFamily
            color: Theme.subtle
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "TYPE DELETE TO CONFIRM"
                font.pixelSize: 12
                font.weight: Font.Bold
                font.family: Theme.fontFamily
                color: Theme.mid
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: Theme.radiusMedium
                color: "transparent"
                border.color: Theme.line
                border.width: 1

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    text: "DELETE"
                    font.pixelSize: 15
                    font.family: Theme.fontFamily
                    color: Theme.mid
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: Theme.radiusMedium
                color: "transparent"
                border.color: Theme.line
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Cancel"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    color: Theme.ink
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: Theme.radiusMedium
                color: Theme.wrong
                opacity: 0.4

                Text {
                    anchors.centerIn: parent
                    text: "DELETE"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    color: "#ffffff"
                }
            }
        }
    }
}
