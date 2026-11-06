import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    title: "New words"
    progress: 1 / 7

    readonly property var chips: [
        { label: "mussel", level: 4, selected: true },
        { label: "shell", level: 2, selected: false },
        { label: "mould", level: 2, selected: false },
        { label: "cast", level: 1, selected: false },
        { label: "tin", level: 1, selected: false },
        { label: "form", level: 1, selected: false }
    ]

    ContentArea {
        posText: "noun · f"
        counterText: "14 / 25"

        primaryContent: ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "la moule"
                font.pixelSize: 40
                font.weight: Font.Medium
                font.family: Theme.fontFamily
                color: Theme.ink
                Layout.alignment: Qt.AlignHCenter
            }

            InputBox {
                text: "mussel"
                filled: true
            }
        }

        secondaryContent: ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: chips
                delegate: TranslationChip {}
            }
        }
    }

    actions: WideButtonRow {
        WideButton { label: "← I know it"; filled: false }
        WideButton { label: "↓ Skip"; filled: false }
        WideButton { label: "Learn it →" }
    }
}
