import QtQuick
import QtQuick.Controls.Basic

Button
{
    id: root

    property string iconName: ""
    property color iconColour: AppTheme.colourText
    property int iconSize: AppMetrics.iconSize
    property bool active: false
    property string shortcutKey: ""

    readonly property bool showsShortcut: root.shortcutKey.length > 0 && AppModifierKeys.controlHeld

    implicitWidth: AppMetrics.headerIconTapWidth
    implicitHeight: AppMetrics.headerIconTapWidth

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    display: AbstractButton.IconOnly
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.name: root.text

    ToolTip.visible: root.hovered && root.text.length > 0
    ToolTip.text: root.text
    ToolTip.delay: AppMetrics.tooltipDelay

    contentItem: Item
    {
        DesktopKeyHint
        {
            anchors.centerIn: parent
            visible: root.showsShortcut
            key: root.shortcutKey
            tint: root.active ? AppTheme.colourAccent : root.iconColour
        }

        DesktopIcon
        {
            anchors.centerIn: parent
            visible: !root.showsShortcut
            name: root.iconName
            width: root.iconSize
            height: root.iconSize
            color: root.active ? AppTheme.colourAccent : root.iconColour
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.down ? AppTheme.colourPressed
                         : (root.hovered || root.checked ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0))

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

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
