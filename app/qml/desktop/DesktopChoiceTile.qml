import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string description: ""
    property string iconName: ""
    property bool selectable: false
    property bool selected: false
    property real progress: -1

    implicitHeight: Math.max(AppMetrics.buttonHeightLarge, contentRow.implicitHeight + root.topPadding + root.bottomPadding)

    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapMedium
    topPadding: AppMetrics.gapCompact
    bottomPadding: AppMetrics.gapCompact

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: Accessible.Button
    Accessible.name: root.text
    Accessible.description: root.description
    Accessible.checkable: root.selectable
    Accessible.checked: root.selected

    contentItem: RowLayout
    {
        id: contentRow

        spacing: AppMetrics.gapCompact

        DesktopIcon
        {
            visible: root.iconName.length > 0
            name: root.iconName
            color: root.selected ? AppTheme.colourAccent : AppTheme.colourText
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.text
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                font.pixelSize: AppMetrics.fontSmall
                color: root.selected ? AppTheme.colourAccent : AppTheme.colourMuted
                wrapMode: Text.WordWrap
            }

            DesktopProgressBar
            {
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapTight
                visible: root.progress >= 0
                progress: root.progress
                fill: AppTheme.colourAccent
                animated: false
            }
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium

        color:
        {
            if (root.selected)
            {
                return Qt.tint(AppTheme.colourWell, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft))
            }

            return root.down ? AppTheme.colourPage : AppTheme.colourWell
        }

        border.width: root.selected ? AppMetrics.focusRingWidth : AppMetrics.borderWidth
        border.color: root.selected ? AppTheme.colourAccent : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusMedium
        }
    }

    HoverHandler
    {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
