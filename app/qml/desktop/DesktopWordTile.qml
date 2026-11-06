import QtQuick
import QtQuick.Controls.Basic

Button
{
    id: root

    property bool used: false

    implicitHeight: AppMetrics.buttonHeight
    leftPadding: AppMetrics.gapCompact
    rightPadding: AppMetrics.gapCompact
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    enabled: !root.used
    opacity: root.used ? AppTheme.opacityDisabled : 1.0

    Accessible.name: root.text

    contentItem: DesktopLabel
    {
        text: root.text
        font.pixelSize: AppMetrics.fontLabel
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.down ? AppTheme.colourFaint : (root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel)
        border.width: AppMetrics.borderWidth
        border.color: root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
    }

    HoverHandler
    {
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
    }
}
