import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string iconName: ""
    property bool selected: false
    property bool collapsed: false
    property int count: 0
    property bool emphasiseCount: false
    property bool marked: false
    property color markColour: AppTheme.colourAccent
    property bool destination: true
    property string shortcutKey: ""

    readonly property bool showsShortcut: root.collapsed && root.shortcutKey.length > 0 && AppModifierKeys.controlHeld

    Layout.fillWidth: true
    implicitHeight: AppMetrics.navItemHeight

    leftPadding: root.collapsed ? 0 : AppMetrics.gapCompact
    rightPadding: root.collapsed ? 0 : AppMetrics.gapSmall
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: root.destination ? Accessible.PageTab : Accessible.Button
    Accessible.name: root.count > 0 ? qsTr("%1, %2").arg(root.text).arg(root.count) : root.text
    Accessible.selected: root.selected

    ToolTip.visible: root.collapsed && root.hovered
    ToolTip.text: root.text
    ToolTip.delay: AppMetrics.tooltipDelay

    contentItem: Item
    {
        RowLayout
        {
            anchors.fill: parent
            spacing: AppMetrics.gapCompact
            visible: !root.collapsed

            DesktopIcon
            {
                name: root.iconName
                color: root.selected ? AppTheme.colourText : AppTheme.colourSubtle
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.text
                font.pixelSize: AppMetrics.fontBody
                font.weight: root.selected ? Font.DemiBold : Font.Medium
                color: root.selected ? AppTheme.colourText : AppTheme.colourSubtle
                elide: Text.ElideRight
            }

            DesktopCountBadge
            {
                count: root.count
                emphasised: root.emphasiseCount
            }
        }

        DesktopKeyHint
        {
            anchors.centerIn: parent
            visible: root.showsShortcut
            key: root.shortcutKey
            tint: root.selected ? AppTheme.colourText : AppTheme.colourSubtle
        }

        DesktopIcon
        {
            anchors.centerIn: parent
            visible: root.collapsed && !root.showsShortcut
            name: root.iconName
            color: root.selected ? AppTheme.colourText : AppTheme.colourSubtle

            Rectangle
            {
                visible: (root.count > 0 && root.emphasiseCount) || root.marked
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: -AppMetrics.gapTight
                width: AppMetrics.gapSmall
                height: AppMetrics.gapSmall
                radius: width / 2
                color: root.markColour
            }
        }
    }

    background: DesktopSelectionBackground
    {
        selected: root.selected
        hovered: root.hovered
        pressed: root.down
        focused: root.visualFocus
        barAtBottom: true
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
