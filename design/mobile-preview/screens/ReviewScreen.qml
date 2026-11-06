import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    id: root
    title: "Review"
    progress: 0

    property bool revealed: false
    property bool primaryIsForeign: true

    ContentArea {
        posText: "noun · f"
        counterText: "4 / 25"

        primaryContent: Text {
            text: "la maison"
            font.pixelSize: root.primaryIsForeign ? Theme.fontSizeForeign : Theme.fontSizeNative
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        secondaryContent: Text {
            text: root.revealed ? "the house" : "?"
            font.pixelSize: root.primaryIsForeign ? Theme.fontSizeNative : Theme.fontSizeForeign
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: root.revealed ? Theme.ink : Theme.mid
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    actions: [
        WideButton {
            label: "Reveal"
            visible: !root.revealed
            onClicked: root.revealed = true
        },
        ButtonPair {
            visible: root.revealed
            leftLabel: "Incorrect"
            rightLabel: "Correct"
            onLeftClicked: root.revealed = false
            onRightClicked: root.revealed = false
        }
    ]
}
