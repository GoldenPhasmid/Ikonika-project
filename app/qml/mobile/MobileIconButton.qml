import QtQuick

Item
{
    id: root

    property string iconName: ""
    property string text: ""
    property color iconColour: AppTheme.colourText
    property bool filled: false
    property bool marked: false
    property real iconSize: AppMetrics.iconSize

    signal clicked()

    implicitWidth: AppMetrics.iconTapSize
    implicitHeight: AppMetrics.iconTapSize

    Accessible.role: Accessible.Button
    Accessible.name: root.text
    Accessible.onPressAction: root.clicked()

    Rectangle
    {
        anchors.centerIn: parent
        width: Math.min(root.width, root.height)
        height: width
        radius: width / 2
        color: AppTheme.colourFaint
        opacity: tap.pressed ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
    }

    DesktopIcon
    {
        anchors.centerIn: parent
        name: root.iconName
        filled: root.filled
        width: root.iconSize
        height: root.iconSize
        color: root.iconColour

        Rectangle
        {
            visible: root.marked
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: -AppMetrics.gapTight
            width: AppMetrics.gapSmall
            height: AppMetrics.gapSmall
            radius: width / 2
            color: AppTheme.colourAccent
        }
    }

    TapHandler
    {
        id: tap

        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: root.clicked()
    }
}
