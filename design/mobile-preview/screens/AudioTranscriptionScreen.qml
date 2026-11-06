import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    id: root
    title: "Compile word audio"
    progress: 0.1

    property bool revealed: false
    property bool primaryIsForeign: true

    ContentArea {
        posVisible: root.revealed
        posText: "noun · f"
        counterText: "4 / 25"

        primaryContent: [
            Rectangle {
                visible: !root.revealed
                Layout.alignment: Qt.AlignHCenter
                width: 80
                height: 80
                radius: 40
                color: Theme.ink

                Text {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 2
                    text: "▶"
                    font.pixelSize: 56
                    color: "#ffffff"
                }
            },
            ColumnLayout {
                visible: root.revealed
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "la maison"
                    font.pixelSize: root.primaryIsForeign ? Theme.fontSizeForeign : Theme.fontSizeNative
                    font.weight: Font.Medium
                    font.family: Theme.fontFamily
                    color: Theme.ink
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    text: "the house"
                    font.pixelSize: root.primaryIsForeign ? Theme.fontSizeNative : Theme.fontSizeForeign
                    font.weight: Font.Medium
                    font.family: Theme.fontFamily
                    color: Theme.ink
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        ]

        secondaryContent: [
            InputBox {
                text: "Type what you hear..."
            },
            GhostLink {
                onClicked: root.revealed = true
            }
        ]
    }

    actions: WideButton {
        label: "Continue"
    }
}
