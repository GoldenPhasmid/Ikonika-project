import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    activeTab: "Words"
    headerTitle: "Add word"
    showBack: true
    showSearch: true

    readonly property var chips: [
        { label: "market", level: 4, selected: true },
        { label: "deal", level: 3, selected: false },
        { label: "marketplace", level: 2, selected: false },
        { label: "agreement", level: 1, selected: false },
        { label: "bargain", level: 1, selected: false }
    ]

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 24
        anchors.bottomMargin: 32
        spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "FRENCH WORD"
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
                color: "transparent"
                border.color: Theme.line
                border.width: 1

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    text: "le marché"
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
                text: "TRANSLATION"
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
                color: "transparent"
                border.color: Theme.line
                border.width: 1

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    text: "market"
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
                text: "PART OF SPEECH"
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
                color: "transparent"
                border.color: Theme.line
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Text {
                        text: "noun"
                        font.pixelSize: 15
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
                text: "ADD TO DECK"
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
                color: "transparent"
                border.color: Theme.line
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Text {
                        text: "Mercado y comida"
                        font.pixelSize: 15
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
            Layout.fillHeight: true
            spacing: 8

            SectionLabel {
                text: "TRANSLATIONS"
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentHeight: chipColumn.implicitHeight
                clip: true

                ColumnLayout {
                    id: chipColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 8

                    Repeater {
                        model: chips
                        delegate: TranslationChip {}
                    }
                }
            }
        }

        WideButton {
            label: "Add"
        }
    }
}
