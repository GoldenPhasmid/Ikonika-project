import QtQuick
import "MobileIcons.js" as Icons

MobileVectorIcon
{
    id: root

    property string direction: "right"

    readonly property var icon: root.direction === "left" ? Icons.Icons.arrow_left
        : root.direction === "down" ? Icons.Icons.arrow_down
        : Icons.Icons.arrow_right

    sourceWidth: root.icon.width
    sourceHeight: root.icon.height
    strokeWidth: 2

    svgPath: root.icon.path
}
