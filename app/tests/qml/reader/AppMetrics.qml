pragma Singleton
import QtQuick

QtObject
{
    readonly property int contentPadding: 16
    readonly property int gapSmall: 8
    readonly property int fontLabel: 16
    readonly property int fontBody: 16
    readonly property string fontFamily: "Arial"
    readonly property int borderWidth: 1
    readonly property int radiusMedium: 8
    readonly property int fontLead: 20
    readonly property int motionBase: 200
    readonly property int scrollBarWidth: 6
    readonly property int readerWheelStep: 60
    readonly property int readerAutoscrollDeadZone: 12
    readonly property real readerAutoscrollSpeed: 0.1
    readonly property int readerAutoscrollAnchorSize: 24
    readonly property real readerImageHeightShare: 0.8
    readonly property real readerEndSpaceShare: 0.3
}
