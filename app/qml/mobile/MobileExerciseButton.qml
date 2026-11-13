import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string label
    property string subtitle: ""
    property string iconSvg: ""

    signal activated()

    Layout.fillWidth: true
    Layout.preferredHeight: 58
    radius: AppMetrics.radiusMedium
    color: AppTheme.colourFaint

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        Rectangle
        {
            visible: root.iconSvg.length > 0
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            radius: AppMetrics.radiusSmall
            color: AppTheme.colourPanel

            MobileVectorIcon
            {
                anchors.centerIn: parent
                visible: root.iconSvg.length > 0
                width: 18
                height: 18
                svgPath: root.iconSvg
                color: AppTheme.colourText
            }
        }

        Text
        {
            Layout.fillWidth: true
            Layout.minimumWidth: 0

            elide: Text.ElideRight
            text: root.label
            font.pixelSize: 18
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.activated()
    }
}
