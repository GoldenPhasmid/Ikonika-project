import QtQuick

Rectangle {
    id: root

    property bool checked: false

    signal toggled()

    implicitWidth: 20
    implicitHeight: 20
    radius: 4
    color: root.checked ? Theme.ink : "transparent"
    border.color: root.checked ? Theme.ink : Theme.line
    border.width: 1

    Text {
        anchors.centerIn: parent
        visible: root.checked
        text: "✓"
        font.pixelSize: 12
        color: "#ffffff"
    }

    MouseArea {
        anchors.fill: parent
        anchors.margins: -8
        onClicked: root.toggled()
    }
}
