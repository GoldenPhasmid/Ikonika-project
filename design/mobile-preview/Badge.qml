import QtQuick

Rectangle {
    id: root

    required property string text

    implicitWidth: label.implicitWidth + 12
    implicitHeight: 17
    radius: 4
    color: Theme.faint

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: 11
        font.family: Theme.fontFamily
        color: "#555555"
    }
}
