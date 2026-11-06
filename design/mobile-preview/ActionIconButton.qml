import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string icon

    signal clicked()

    Layout.preferredWidth: 32
    Layout.preferredHeight: 32
    radius: 6
    color: Theme.faint

    Text {
        anchors.centerIn: parent
        text: root.icon
        font.pixelSize: 14
        color: Theme.ink
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
