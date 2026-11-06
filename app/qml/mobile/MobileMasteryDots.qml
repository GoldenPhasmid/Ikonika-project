import QtQuick
import QtQuick.Layouts

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

    Layout.alignment: Qt.AlignHCenter
    spacing: AppMetrics.gapSmall

    Repeater
    {
        model: root.target

        delegate: Rectangle
        {
            id: dot

            required property int index

            readonly property bool isFilled: dot.index < root.filled

            onIsFilledChanged: if (dot.isFilled && root.popArmed) pop.restart()

            SequentialAnimation
            {
                id: pop

                NumberAnimation { target: dot; property: "scale"; to: 1.6; duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
                NumberAnimation { target: dot; property: "scale"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
            }

            Behavior on color { ColorAnimation { duration: AppMetrics.motionBase } }

            width: AppMetrics.masteryDotSize
            height: AppMetrics.masteryDotSize
            radius: width / 2

            color: dot.isFilled ? AppTheme.colourAccent : "transparent"
            border.color: dot.isFilled ? AppTheme.colourAccent : AppTheme.colourBorder
            border.width: AppMetrics.borderWidth
        }
    }
}
