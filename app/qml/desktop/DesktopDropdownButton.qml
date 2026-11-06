import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property Popup popover: null

    readonly property bool open: !!root.popover && root.popover.visible

    Layout.fillWidth: true
    implicitHeight: AppMetrics.buttonHeight
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    leftPadding: AppMetrics.gapCompact
    rightPadding: AppMetrics.gapCompact
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.ComboBox

    onClicked:
    {
        if (!root.popover)
        {
            return
        }

        if (root.popover.visible)
        {
            root.popover.close()

            return
        }

        root.popover.open()
    }

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.text
            elide: Text.ElideRight
        }

        DesktopIcon
        {
            name: "chevron_down"
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: AppTheme.colourMuted
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: root.open ? AppTheme.colourAccent : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

        DesktopFocusRing
        {
            visible: root.visualFocus
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
