import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    id: root
    title: "Correct / Incorrect"
    progress: 0

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
            text: "the house"
            font.pixelSize: root.primaryIsForeign ? Theme.fontSizeNative : Theme.fontSizeForeign
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    actions: ButtonPair {
        leftLabel: "Incorrect"
        rightLabel: "Correct"
    }
}
