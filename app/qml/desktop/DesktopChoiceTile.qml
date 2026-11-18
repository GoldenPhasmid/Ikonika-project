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
    property bool filled: false
    property real progress: -1

    readonly property color contentColour: root.filled ? AppTheme.colourPanel : AppTheme.colourText
    readonly property color captionColour: root.filled ? AppTheme.colourPanel : (root.selected ? AppTheme.colourAccent : AppTheme.colourMuted)

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
            color: root.filled ? root.contentColour : (root.selected ? AppTheme.colourAccent : AppTheme.colourText)
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
                color: root.contentColour
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                font.pixelSize: AppMetrics.fontSmall
                color: root.captionColour
                opacity: root.filled ? AppTheme.opacitySecondary : 1.0
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
            if (root.filled)
            {
                const veil = root.down ? AppTheme.alphaStrong : (root.hovered ? AppTheme.alphaSoft : 0)

                return Qt.tint(AppTheme.colourText, AppTheme.faded(AppTheme.colourPanel, veil))
            }

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
