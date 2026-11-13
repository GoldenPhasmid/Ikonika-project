import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property string label
    required property bool active

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
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter

            radius: AppMetrics.radiusSmall
            color: root.active ? AppTheme.colourText : "transparent"
            border.color: AppTheme.colourBorder
            border.width: root.active ? 0 : AppMetrics.borderWidth

            DesktopIcon
            {
                anchors.centerIn: parent
                name: root.iconName
                width: 20
                height: width
                color: root.active ? AppTheme.colourPanel : AppTheme.colourText
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
