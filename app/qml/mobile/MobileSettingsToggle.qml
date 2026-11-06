import QtQuick

Item
{
    id: root

    property bool checked: false
    property bool interactive: true

    signal toggled()

    implicitWidth: 46
    implicitHeight: 44

    opacity: root.interactive ? 1.0 : 0.5

    Rectangle
    {
        id: track

        anchors.centerIn: parent
        width: 46
        height: 28
        radius: height / 2

        color: root.checked ? AppTheme.colourText : AppTheme.colourFaint
        border.color: root.checked ? AppTheme.colourText : AppTheme.colourOutline
        border.width: 2

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        Rectangle
        {
            width: 20
            height: 20
            radius: height / 2
            y: 4
            x: root.checked ? track.width - width - 4 : 4

            color: AppTheme.colourPanel
            border.color: root.checked ? AppTheme.colourPanel : AppTheme.colourOutline
            border.width: root.checked ? 0 : 2

            Behavior on x { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }
        }
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: root.interactive
        onClicked: root.toggled()
    }
}
