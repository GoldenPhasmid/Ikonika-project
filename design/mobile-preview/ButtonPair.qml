import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string leftLabel: "Previous"
    property string rightLabel: "Next"

    signal leftClicked()
    signal rightClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: Theme.actionButtonHeight
    spacing: 8

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.actionButtonHeight
        radius: Theme.radiusSmall
        color: "transparent"
        border.color: Theme.line
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: root.leftLabel
            font.pixelSize: 14
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            color: Theme.ink
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.leftClicked()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.actionButtonHeight
        radius: Theme.radiusSmall
        color: Theme.ink

        Text {
            anchors.centerIn: parent
            text: root.rightLabel
            font.pixelSize: 14
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            color: "#ffffff"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.rightClicked()
        }
    }
}
