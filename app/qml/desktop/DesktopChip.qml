import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string count: ""
    property color dotColour: "transparent"
    property bool selected: false
    property bool toggle: false

    readonly property bool filled: root.selected && !root.toggle
    readonly property bool switchedOn: root.selected && root.toggle

    implicitHeight: AppMetrics.chipHeight
    implicitWidth: contentRow.implicitWidth + AppMetrics.gapCompact * 2

    padding: 0
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.name: root.count.length > 0 ? qsTr("%1, %2").arg(root.text).arg(root.count) : root.text
    Accessible.checkable: true
    Accessible.checked: root.selected

    contentItem: Item
    {
        RowLayout
        {
            id: contentRow

            anchors.centerIn: parent
            spacing: AppMetrics.gapSmall

            CategoryDot
            {
                visible: root.dotColour.a > 0
                colour: root.dotColour
            }

            DesktopLabel
            {
                text: root.text
                font.pixelSize: AppMetrics.fontSmall
                font.weight: Font.Medium
                color: root.filled ? AppTheme.colourPanel : (root.switchedOn ? AppTheme.colourAccent : AppTheme.colourText)
            }

            DesktopLabel
            {
                visible: root.count.length > 0
                text: root.count
                font.pixelSize: AppMetrics.fontSmall
                color: root.filled ? AppTheme.colourPanel : (root.switchedOn ? AppTheme.colourAccent : AppTheme.colourMuted)
                opacity: root.filled ? AppTheme.opacitySecondary : 1.0
            }
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.filled ? AppTheme.colourText
             : root.switchedOn ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaMedium))
             : (root.hovered ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0))
        border.width: root.filled ? 0 : AppMetrics.borderWidth
        border.color: root.switchedOn ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf) : AppTheme.colourBorder

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.chipHeight / 2
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
