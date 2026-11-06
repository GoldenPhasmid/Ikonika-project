import QtQuick

Item
{
    id: root

    property int level: 0
    property int bars: 4
    property color activeColour: AppTheme.colourText
    property color inactiveColour: AppTheme.colourBorder

    readonly property int barWidth: AppMetrics.progressRingStroke
    readonly property int barSpacing: AppMetrics.borderWidth * 2
    readonly property int baseHeight: AppMetrics.gapTight + AppMetrics.borderWidth * 2

    implicitWidth: root.bars * root.barWidth + (root.bars - 1) * root.barSpacing
    implicitHeight: root.baseHeight + (root.bars - 1) * root.barWidth

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Confidence %1 of %2").arg(root.level).arg(root.bars)

    Repeater
    {
        model: root.bars

        delegate: Rectangle
        {
            required property int index

            width: root.barWidth
            height: root.baseHeight + index * root.barWidth
            radius: AppMetrics.borderWidth
            x: index * (root.barWidth + root.barSpacing)
            y: root.height - height
            color: index < root.level ? root.activeColour : root.inactiveColour
        }
    }
}
