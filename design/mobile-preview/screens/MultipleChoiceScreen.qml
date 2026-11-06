import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    title: "Choose a translation"
    progress: 0
    hasActions: false

    property bool primaryIsForeign: true

    readonly property var options: [
        "swindle or cheat out money",
        "spoken defamation",
        "deliberate act to deceive or trick people",
        "lacking thought or intelligence",
        "(noun) courage and determination in diff situations",
        "spread or expand something outward"
    ]

    ContentArea {
        posText: "noun · f"
        counterText: "4 / 25"
        reserveActionsSpace: true

        primaryContent: [
            Text {
                text: "vacuous"
                font.pixelSize: primaryIsForeign ? Theme.fontSizeForeign : Theme.fontSizeNative
                font.weight: Font.Medium
                font.family: Theme.fontFamily
                color: Theme.ink
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        ]

        secondaryContent: [
            UniformCardGrid {
                Layout.fillWidth: true
                rowSpacing: 8
                columnSpacing: 8
                cardModel: options
            },
            GhostLink {}
        ]
    }
}
