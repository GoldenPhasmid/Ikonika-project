import QtQuick

Rectangle
{
    id: root

    property var options: []
    property int currentIndex: 0

    signal chosen(int index)

    readonly property Item currentSegment: segments.count > root.currentIndex ? segments.itemAt(root.currentIndex) : null
    readonly property real segmentWidth: segments.count > 0 ? (root.width - AppMetrics.gapTight * 2) / segments.count : 0

    implicitWidth: 240
    implicitHeight: 36

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourFaint

    Accessible.role: Accessible.PageTabList

    Rectangle
    {
        visible: root.currentSegment !== null
        x: root.currentSegment !== null ? segmentRow.x + root.currentSegment.x : 0
        y: segmentRow.y
        width: root.segmentWidth
        height: segmentRow.height
        radius: AppMetrics.radiusSmall
        color: AppTheme.colourText

        Behavior on x { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }
    }

    Row
    {
        id: segmentRow

        anchors.fill: parent
        anchors.margins: AppMetrics.gapTight

        Repeater
        {
            id: segments

            model: root.options

            Item
            {
                id: segment

                required property string modelData
                required property int index

                readonly property bool selected: segment.index === root.currentIndex

                width: root.segmentWidth
                height: segmentRow.height

                Accessible.role: Accessible.PageTab
                Accessible.name: segment.modelData
                Accessible.selected: segment.selected

                Text
                {
                    anchors.fill: parent
                    anchors.leftMargin: AppMetrics.gapTight
                    anchors.rightMargin: AppMetrics.gapTight

                    text: segment.modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: AppMetrics.fontMeta
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: segment.selected ? AppTheme.colourPanel : AppTheme.colourSubtle

                    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
                }

                MouseArea
                {
                    anchors.fill: parent

                    onClicked:
                    {
                        if (segment.selected)
                        {
                            return
                        }

                        root.chosen(segment.index)
                    }
                }
            }
        }
    }
}
