import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string label
    property bool filled: true

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: Theme.actionButtonHeight
    radius: Theme.radiusMedium
    color: filled ? Theme.ink : Theme.panel
    border.color: Theme.line
    border.width: filled ? 0 : 1
    opacity: root.enabled ? 1.0 : 0.4

    Text {
        anchors.centerIn: parent
        text: root.label
        font.pixelSize: root.filled ? 15 : 14
        font.weight: Font.DemiBold
        font.family: Theme.fontFamily
        color: root.filled ? "#ffffff" : Theme.ink
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
