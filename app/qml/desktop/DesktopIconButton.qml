import QtQuick
import QtQuick.Controls.Basic

Button
{
    id: root

    property string iconName: ""
    property url imageSource: ""
    property url hoveredImageSource: ""
    property bool onCover: false
    property bool destructive: false
    property color iconColour: root.destructive ? (root.onCover ? AppTheme.colourCoverWrong : AppTheme.colourWrong)
                                                : (root.onCover ? AppTheme.colourCoverInk : AppTheme.colourText)
    property bool iconFilled: false
    property int iconSize: AppMetrics.iconSize
    property bool active: false
    property string shortcutKey: ""

    readonly property bool showsShortcut: root.shortcutKey.length > 0 && AppModifierKeys.controlHeld
    readonly property bool showsImage: root.imageSource.toString().length > 0
    readonly property bool backed: root.onCover && root.destructive
    readonly property color restColour: root.backed ? AppTheme.colourCoverPaper : AppTheme.faded(root.hoverColour, 0)
    readonly property color hoverColour: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaSoft) : AppTheme.colourHover
    readonly property color pressedColour: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong) : AppTheme.colourPressed

    implicitWidth: AppMetrics.iconTapSize
    implicitHeight: AppMetrics.iconTapSize

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

        Image
        {
            anchors.fill: parent
            visible: !root.showsShortcut && root.showsImage
            source: root.hovered && root.hoveredImageSource.toString().length > 0 ? root.hoveredImageSource : root.imageSource
            fillMode: Image.PreserveAspectFit
            mipmap: true
        }

        DesktopIcon
        {
            anchors.centerIn: parent
            visible: !root.showsShortcut && !root.showsImage
            name: root.iconName
            filled: root.iconFilled
            width: root.iconSize
            height: root.iconSize
            color: root.active ? AppTheme.colourAccent : root.iconColour
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.showsImage ? AppTheme.faded(root.hoverColour, 0)
             : root.backed ? Qt.tint(root.restColour, root.down ? root.pressedColour : (root.hovered ? root.hoverColour : AppTheme.faded(root.hoverColour, 0)))
             : root.down ? root.pressedColour
             : (root.hovered || root.checked ? root.hoverColour : root.restColour)

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            colour: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourAccent
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
