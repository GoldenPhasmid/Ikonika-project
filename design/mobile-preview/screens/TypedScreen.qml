import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    id: root
    title: "Compile word"
    progress: 0.1

    property bool revealed: false
    property bool primaryIsForeign: false

    ContentArea {
        posVisible: root.revealed
        posText: "noun · f"
        counterText: "4 / 25"

        primaryContent: Text {
            text: "anxiety or dismay at unexpected"
            font.pixelSize: root.primaryIsForeign ? Theme.fontSizeForeign : Theme.fontSizeNative
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        secondaryContent: [
            InputBox {
                text: "Type translation..."
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
