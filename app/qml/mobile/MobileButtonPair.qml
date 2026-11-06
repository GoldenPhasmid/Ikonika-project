import QtQuick
import QtQuick.Layouts

RowLayout
{
    id: root

    property string leftLabel: qsTr("Previous")
    property string rightLabel: qsTr("Next")

    property string leftArrow: ""
    property string rightArrow: ""

    property bool leftEnabled: true
    property bool rightEnabled: true

    property bool rightDestructive: false

    signal leftClicked()
    signal rightClicked()

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    MobileSessionButton
    {
        label: root.leftLabel
        leadingArrow: root.leftArrow
        filled: false
        interactive: root.leftEnabled

        onClicked: root.leftClicked()
    }

    MobileSessionButton
    {
        label: root.rightLabel
        trailingArrow: root.rightArrow
        interactive: root.rightEnabled
        destructive: root.rightDestructive

        onClicked: root.rightClicked()
    }
}
