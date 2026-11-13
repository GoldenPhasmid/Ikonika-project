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

    property color leftMarkColour: "transparent"
    property color rightMarkColour: "transparent"

    signal leftClicked()
    signal rightClicked()

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    MobileSessionButton
    {
        label: root.leftLabel
        leadingArrow: root.leftArrow
        markColour: root.leftMarkColour
        filled: false
        interactive: root.leftEnabled

        onClicked: root.leftClicked()
    }

    MobileSessionButton
    {
        label: root.rightLabel
        trailingArrow: root.rightArrow
        markColour: root.rightMarkColour
        interactive: root.rightEnabled
        destructive: root.rightDestructive

        onClicked: root.rightClicked()
    }
}
