import QtQuick

Item {
    id: root

    required property int level
    property int bars: 4
    property color activeColor: Theme.ink
    property color inactiveColor: Theme.line

    readonly property int barWidth: 3
    readonly property int barSpacing: 2

    implicitWidth: bars * barWidth + (bars - 1) * barSpacing
    implicitHeight: 6 + (bars - 1) * 3

    Repeater {
        model: root.bars
        delegate: Rectangle {
            width: root.barWidth
            height: 6 + index * 3
            radius: 1
            x: index * (root.barWidth + root.barSpacing)
            y: root.height - height
            color: index < root.level ? root.activeColor : root.inactiveColor
        }
    }
}
