import QtQuick

Rectangle
{
    id: root

    property bool on: false
    property bool onCover: false
    property color coverTint: AppTheme.colourCoverMuted

    readonly property color trackOnColour: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourAccent
    readonly property color trackOffColour: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, 0) : AppTheme.colourFaint
    readonly property color knobOnColour: root.onCover ? root.coverTint : AppTheme.colourAccentText
    readonly property color knobOffColour: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourMuted

    implicitWidth: AppMetrics.chipHeight + AppMetrics.gapCompact
    implicitHeight: AppMetrics.badgeHeight

    radius: height / 2
    color: root.on ? root.trackOnColour : root.trackOffColour
    border.width: root.on ? 0 : AppMetrics.borderWidth
    border.color: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaHalf) : AppTheme.colourBorder

    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

    Rectangle
    {
        width: parent.height - AppMetrics.gapTight * 2
        height: width
        radius: width / 2
        anchors.verticalCenter: parent.verticalCenter
        x: root.on ? parent.width - width - AppMetrics.gapTight : AppMetrics.gapTight
        color: root.on ? root.knobOnColour : root.knobOffColour

        Behavior on x { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }
    }
}
