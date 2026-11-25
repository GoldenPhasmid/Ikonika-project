pragma Singleton
import QtQuick

QtObject
{
    readonly property color colourBorder: "grey"
    readonly property color colourAccent: "blue"
    readonly property color colourPanel: "grey"
    readonly property color colourText: "black"
    readonly property color colourMuted: "grey"
    readonly property color colourOutline: "grey"
    readonly property color colourCard: "white"
    readonly property color colourAccentText: "white"
    readonly property color colourReaderMarkDark: "orange"
    readonly property real alphaSoft: 0.2
    readonly property real alphaHalf: 0.5

    function faded(colour, alpha)
    {
        return Qt.rgba(colour.r, colour.g, colour.b, alpha)
    }
}
