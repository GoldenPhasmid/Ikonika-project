import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton
{
    id: root

    property bool selected: false
    property int stage: -1
    property bool completed: false

    implicitWidth: contentRow.implicitWidth
    implicitHeight: AppMetrics.tabHeight

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: root.text
    Accessible.selected: root.selected

    contentItem: Item
    {
        RowLayout
        {
            id: contentRow

            anchors.centerIn: parent
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                text: root.text
                font.pixelSize: AppMetrics.fontTab
                font.weight: root.selected ? Font.DemiBold : Font.Medium
                color: root.selected || root.hovered ? AppTheme.colourText : AppTheme.colourMuted
            }

            DesktopLearnedMark
            {
                stage: root.stage
                completed: root.completed
            }
        }
    }

    background: Item
    {
        Rectangle
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: AppMetrics.tabIndicatorHeight
            radius: height / 2
            color: root.selected ? AppTheme.colourAccent : "transparent"

            Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusSmall
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
