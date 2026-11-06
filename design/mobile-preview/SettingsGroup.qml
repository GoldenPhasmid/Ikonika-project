import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property string title

    default property alias rows: card.data

    Layout.fillWidth: true
    spacing: 8

    Text {
        text: root.title
        font.pixelSize: 11
        font.weight: Font.Bold
        font.family: Theme.fontFamily
        font.letterSpacing: 1
        color: Theme.subtle
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: card.implicitHeight

        radius: Theme.radiusMedium
        color: Theme.panel
        border.color: Theme.line
        border.width: 1
        clip: true

        ColumnLayout {
            id: card

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0
        }
    }
}
