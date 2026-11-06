import QtQuick

Rectangle {
    id: root

    required property string level

    color: Theme.cefrColours[root.level] || Theme.line
    radius: Theme.radiusSmall
    implicitWidth: label.implicitWidth + 16
    implicitHeight: 22

    Text {
        id: label
        anchors.centerIn: parent
        text: root.level
        font.pixelSize: 11
        font.bold: true
        font.family: Theme.fontFamily
        color: "#16181d"
    }
}
