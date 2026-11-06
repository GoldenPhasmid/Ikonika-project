import QtQuick
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    property string iconSvg: Icons.Icons.plus.path

    signal clicked()

    width: 56
    height: 56
    radius: width / 2

    color: AppTheme.colourText

    MobileVectorIcon
    {
        anchors.centerIn: parent

        width: 24
        height: 24
        strokeWidth: 2

        svgPath: root.iconSvg
        color: AppTheme.colourPanel
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
