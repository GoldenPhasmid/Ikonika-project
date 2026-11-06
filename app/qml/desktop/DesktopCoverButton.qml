import QtQuick
import QtQuick.Controls.Basic

Button
{
    id: root

    property string iconName: ""
    property color iconColour: AppTheme.colourPanel
    property bool showLabel: false

    implicitWidth: root.showLabel ? labelRow.implicitWidth + AppMetrics.gapSmall * 2 : AppMetrics.buttonHeight
    implicitHeight: AppMetrics.buttonHeight

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    display: AbstractButton.IconOnly

    Accessible.name: root.text

    ToolTip.visible: root.hovered && root.text.length > 0 && !root.showLabel
    ToolTip.text: root.text
    ToolTip.delay: AppMetrics.tooltipDelay

    contentItem: Item
    {
        DesktopIcon
        {
            anchors.centerIn: parent
            visible: !root.showLabel
            name: root.iconName
            color: root.iconColour
        }

        Row
        {
            id: labelRow

            anchors.centerIn: parent
            visible: root.showLabel
            spacing: AppMetrics.gapTight

            DesktopIcon
            {
                anchors.verticalCenter: parent.verticalCenter
                name: root.iconName
                color: root.iconColour
            }

            DesktopLabel
            {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                font.pixelSize: AppMetrics.fontSmall
                font.weight: Font.DemiBold
                color: root.iconColour
            }
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusSmall
        color: root.down ? AppTheme.faded(AppTheme.colourPanel, AppTheme.alphaStrong)
                         : (root.hovered ? AppTheme.faded(AppTheme.colourPanel, AppTheme.alphaSoft) : "transparent")

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
