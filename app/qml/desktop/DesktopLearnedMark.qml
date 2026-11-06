import QtQuick
import QtQuick.Controls.Basic

Rectangle
{
    id: root

    property int stage: -1
    property bool completed: false

    readonly property bool learning: root.stage > 0 || root.completed

    implicitWidth: root.learning ? AppMetrics.gapSmall : 0
    implicitHeight: root.learning ? AppMetrics.gapSmall : 0

    visible: root.learning
    radius: width / 2
    color: root.completed ? AppTheme.colourCorrect : AppTheme.colourAccent

    Accessible.role: Accessible.Indicator
    Accessible.name: hint.text

    HoverHandler
    {
        id: hover
    }

    ToolTip
    {
        id: hint

        visible: hover.hovered && root.learning
        delay: AppMetrics.motionSlow
        text: root.completed ? qsTr("Learned") : qsTr("Learning, at interval %1").arg(root.stage)
    }
}
