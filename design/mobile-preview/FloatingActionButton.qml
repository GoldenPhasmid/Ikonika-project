import QtQuick

Rectangle {
    id: root

    property string icon: "+"

    signal clicked()

    width: 56
    height: 56
    radius: width / 2

    color: Theme.ink

    Text {
        anchors.centerIn: parent
        text: root.icon
        font.pixelSize: 26
        font.family: Theme.fontFamily
        color: "#ffffff"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
