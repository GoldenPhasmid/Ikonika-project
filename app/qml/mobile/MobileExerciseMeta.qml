import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string posText: ""
    property bool posVisible: true
    property string counterText: ""
    property int masteryTarget: 0
    property int masteryFilled: 0

    function armMasteryPop()
    {
        dots.armPop()
    }

    Layout.fillWidth: true
    implicitHeight: Math.max(posLabel.implicitHeight, dots.implicitHeight, counterLabel.implicitHeight)

    Text
    {
        id: posLabel

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter

        text: root.posText
        visible: root.posVisible
        font.pixelSize: AppMetrics.fontSmall
        font.weight: Font.Medium
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourMuted
    }

    MobileMasteryDots
    {
        id: dots

        anchors.centerIn: parent

        visible: root.masteryTarget > 0

        target: root.masteryTarget
        filled: root.masteryFilled
    }

    Text
    {
        id: counterLabel

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        text: root.counterText
        font.pixelSize: AppMetrics.fontSmall
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourMuted
    }
}
