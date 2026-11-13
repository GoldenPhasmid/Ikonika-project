import QtQuick

Rectangle
{
    id: root

    property bool checked: false

    signal toggled()

    implicitWidth: 20
    implicitHeight: 20
    radius: 4
    color: root.checked ? AppTheme.colourText : "transparent"
    border.color: root.checked ? AppTheme.colourText : AppTheme.colourBorder
    border.width: AppMetrics.borderWidth

    DesktopIcon
    {
        anchors.centerIn: parent
        visible: root.checked
        name: "check"
        width: AppMetrics.fontMeta
        height: width
        color: AppTheme.colourPanel
    }

    MouseArea
    {
        anchors.fill: parent
        anchors.margins: -8
        onClicked: root.toggled()
    }
}
