import QtQuick

Row
{
    id: root

    property int target: 0
    property int filled: 0
    property bool popArmed: false

    function armPop()
    {
        root.popArmed = true

        Qt.callLater(function() { root.popArmed = false })
    }

    spacing: AppMetrics.gapSmall

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("%1 of %2 correct").arg(root.filled).arg(root.target)

    Repeater
    {
        model: root.target

        delegate: Rectangle
        {
            id: dot

            required property int index

            readonly property bool isFilled: dot.index < root.filled

            onIsFilledChanged: if (dot.isFilled && root.popArmed) pop.restart()

            width: AppMetrics.masteryDotSize
            height: AppMetrics.masteryDotSize
            radius: width / 2

            color: dot.isFilled ? AppTheme.colourAccent : "transparent"
            border.color: dot.isFilled ? AppTheme.colourAccent : AppTheme.colourOutline
            border.width: AppMetrics.borderWidth

            Behavior on color { ColorAnimation { duration: AppMetrics.motionBase } }

            SequentialAnimation
            {
                id: pop

                NumberAnimation { target: dot; property: "scale"; to: 1.6; duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
                NumberAnimation { target: dot; property: "scale"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
            }
        }
    }
}
