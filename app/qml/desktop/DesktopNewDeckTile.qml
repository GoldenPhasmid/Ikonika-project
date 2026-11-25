import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    implicitHeight: Math.round(width / AppMetrics.readerTileAspect)
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    text: qsTr("New deck")

    Accessible.role: Accessible.Button
    Accessible.name: root.text

    contentItem: Item
    {
        DesktopIcon
        {
            anchors.centerIn: parent
            name: "plus"
            width: AppMetrics.newDeckIconSize
            height: AppMetrics.newDeckIconSize
            color: AppTheme.colourPanel
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.coverRadius
        color: Qt.tint(AppTheme.colourText, AppTheme.faded(AppTheme.colourPanel, root.down ? AppTheme.alphaStrong : (root.hovered ? AppTheme.alphaSoft : 0)))

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.coverRadius
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
