import QtQuick
import QtQuick.Controls.Basic

TextField
{
    id: root

    property bool large: false

    implicitHeight: root.large ? AppMetrics.buttonHeightLarge : AppMetrics.buttonHeight

    leftPadding: AppMetrics.gapCompact
    rightPadding: AppMetrics.gapCompact
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter

    font.family: AppMetrics.fontFamily
    font.pixelSize: root.large ? AppMetrics.fontLabel : AppMetrics.fontBody
    color: AppTheme.colourText
    placeholderTextColor: AppTheme.colourMuted
    selectionColor: AppTheme.colourAccent
    selectedTextColor: AppTheme.colourAccentText
    selectByMouse: true
    hoverEnabled: true

    Accessible.name: root.placeholderText

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.readOnly ? AppTheme.colourSunken : AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: root.activeFocus ? AppTheme.colourAccent
                                       : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

        Behavior on border.color { ColorAnimation { duration: AppMetrics.motionFast } }
    }
}
