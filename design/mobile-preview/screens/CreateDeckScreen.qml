import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    activeTab: "Decks"
    headerTitle: "Create deck"
    showBack: true
    trailingIcon: ""
    showSearch: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "DECK NAME"
                    font.pixelSize: 13
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
                        text: "e.g. Market and food"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.mid
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "DESCRIPTION (OPTIONAL)"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    font.family: Theme.fontFamily
                    color: Theme.mid
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    radius: Theme.radiusMedium
                    color: "transparent"
                    border.color: Theme.line
                    border.width: 1

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        text: "What is this deck about?"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.mid
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        WideButton {
            label: "Create"
        }
    }
}
