pragma Singleton
import QtQuick

QtObject {
    readonly property color ink: "#1a1a1a"
    readonly property color mid: "#999999"
    readonly property color subtle: "#555555"
    readonly property color outline: "#767676"
    readonly property color line: "#dddddd"
    readonly property color faint: "#f0f0f0"
    readonly property color panel: "#ffffff"

    readonly property color accent: "#2f6fe0"
    readonly property color accentText: "#ffffff"
    readonly property color correct: "#2f9563"
    readonly property color wrong: "#b03a3a"

    readonly property var cefrColours: ({
        "A1": "#4caf7d",
        "A2": "#7fb356",
        "B1": "#c9a53f",
        "B2": "#c98f45",
        "C1": "#c9703f",
        "C2": "#c25a5a"
    })

    readonly property var deckColours: ["#5a7ca8", "#6b917c", "#8f7396", "#a37c5e"]

    readonly property var frequencyColours: ({
        "TOP 100": "#4caf7d",
        "TOP 500": "#7fb356",
        "TOP 1000": "#a9a94a",
        "TOP 2000": "#c98f45",
        "TOP 5000": "#c9703f",
        "TOP 10000": "#c25a5a"
    })

    function frequencyColour(bucket) {
        return frequencyColours[bucket] || line
    }

    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 20

    readonly property int fontSizeForeign: 48
    readonly property int fontSizeHeadword: 32
    readonly property int fontSizeNative: 24
    readonly property int fontSizeEyebrow: 11

    readonly property int trackingWide: 1

    readonly property int actionButtonHeight: 48

    readonly property int exerciseContentMargin: 16
    readonly property int exerciseContentBottomMargin: 24
    readonly property int exerciseContentSpacing: 16

    readonly property string fontFamily: "Inter"
}
