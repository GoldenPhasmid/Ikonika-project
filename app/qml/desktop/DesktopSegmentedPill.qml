import QtQuick
import QtQuick.Controls.Basic

Rectangle
{
    id: root

    property var options: []
    property int currentIndex: 0
    property string accessibleName: ""

    signal chosen(int index)

    readonly property Item currentSegment: segments.count > root.currentIndex ? segments.itemAt(root.currentIndex) : null
    readonly property real segmentWidth:
    {
        let Widest = 0

        for (let Index = 0; Index < segments.count; ++Index)
        {
            const Segment = segments.itemAt(Index)

            if (Segment !== null)
            {
                Widest = Math.max(Widest, Segment.labelWidth)
            }
        }

        return Widest + AppMetrics.gapMedium * 2
    }

    implicitWidth: root.segmentWidth * segments.count + AppMetrics.gapTight * 2
    implicitHeight: AppMetrics.chipHeight + AppMetrics.gapTight

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourFaint
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: Accessible.PageTabList
    Accessible.name: root.accessibleName

    Rectangle
    {
        visible: root.currentSegment !== null
        x: root.currentSegment !== null ? segmentRow.x + root.currentSegment.x : 0
        y: segmentRow.y
        width: root.currentSegment !== null ? root.currentSegment.width : 0
        height: segmentRow.height
        radius: AppMetrics.radiusSmall
        color: AppTheme.colourText

        Behavior on x { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }
        Behavior on width { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }
    }

    Row
    {
        id: segmentRow

        anchors.centerIn: parent

        Repeater
        {
            id: segments

            model: root.options

            AbstractButton
            {
                id: segment

                required property string modelData
                required property int index

                readonly property bool selected: segment.index === root.currentIndex
                readonly property real labelWidth: segmentLabel.implicitWidth

                implicitWidth: root.segmentWidth
                width: segments.count > 0 ? (root.width - AppMetrics.gapTight * 2) / segments.count : 0
                implicitHeight: root.height - AppMetrics.gapTight * 2

                hoverEnabled: true
                focusPolicy: Qt.StrongFocus

                Accessible.role: Accessible.PageTab
                Accessible.name: segment.modelData
                Accessible.selected: segment.selected

                contentItem: DesktopLabel
                {
                    id: segmentLabel

                    text: segment.modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Medium
                    color: segment.selected ? AppTheme.colourPanel : (segment.hovered ? AppTheme.colourText : AppTheme.colourMuted)

                    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
                }

                background: DesktopFocusRing
                {
                    visible: segment.visualFocus
                    baseRadius: AppMetrics.radiusSmall
                }

                onClicked:
                {
                    if (segment.selected)
                    {
                        return
                    }

                    root.chosen(segment.index)
                }

                HoverHandler
                {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }
}
