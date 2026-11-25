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
    readonly property real shelfRecedeScale: 0.96
    readonly property int motionEasing: Easing.OutCubic
    readonly property int motionSlow: 400
    readonly property bool touchInput: false
    readonly property int scrollBarWidth: 6
    readonly property int readerWheelStep: 60
    readonly property int readerAutoscrollDeadZone: 12
    readonly property real readerAutoscrollSpeed: 6
    readonly property int readerAutoscrollAnchorSize: 24
    readonly property int gapLarge: 24
    readonly property int readerPageMargin: 24
    readonly property int readerPageWidth: 880
    readonly property int readerCropPadding: 8
    readonly property real readerZoomStep: 1.2
    readonly property real readerMinimumZoom: 0.25
    readonly property real readerMaximumZoom: 8
    readonly property real readerImageHeightShare: 0.8
    readonly property int readerEndSpace: 80
}
