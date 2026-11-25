import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property string label
    required property bool active
    property bool marked: false

    required property string iconName

    signal activated()

    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout
    {
        anchors.centerIn: parent
        spacing: 6

        Rectangle
        {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter

            radius: AppMetrics.radiusMedium
            color: root.active ? AppTheme.colourFaint : "transparent"

            DesktopIcon
            {
                anchors.centerIn: parent
                name: root.iconName
                width: 22
                height: width
                color: root.active ? AppTheme.colourText : AppTheme.colourSubtle
            }

            Rectangle
            {
                visible: root.active
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: 24
                height: 3
                radius: height / 2
                color: AppTheme.colourAccent
            }

            Rectangle
            {
                visible: root.marked
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 10
                anchors.topMargin: 4
                width: 8
                height: 8
                radius: width / 2
                color: AppTheme.colourAccent
            }
        }

        Text
        {
            Layout.alignment: Qt.AlignHCenter

            text: root.label.toUpperCase()
            font.pixelSize: 10
            font.weight: root.active ? Font.DemiBold : Font.Medium
            font.family: AppMetrics.fontFamily
            font.letterSpacing: 0.4
            color: root.active ? AppTheme.colourText : AppTheme.colourMuted
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.activated()
    }
}
