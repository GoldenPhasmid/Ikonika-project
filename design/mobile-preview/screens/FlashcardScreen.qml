import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    title: "Flashcard"
    progress: 0.7

    readonly property bool primaryIsForeign: true

    ContentArea {
        posText: "noun · m"
        counterText: "4 / 25"

        primaryContent: Text {
            text: "le chien"
            font.pixelSize: primaryIsForeign ? Theme.fontSizeForeign : Theme.fontSizeNative
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        secondaryContent: [
            Text {
                text: "the dog"
                font.pixelSize: primaryIsForeign ? Theme.fontSizeNative : Theme.fontSizeForeign
                font.weight: Font.Medium
                font.family: Theme.fontFamily
                color: Theme.ink
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            },
            Text {
                text: "animal domestique de la famille des canidés"
                font.pixelSize: 15
                font.family: Theme.fontFamily
                color: Theme.mid
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        ]
    }

    actions: ButtonPair {
        leftLabel: "← Previous"
        rightLabel: "Next →"
    }
}
