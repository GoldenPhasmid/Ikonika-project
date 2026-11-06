import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string text: ""
    property bool filled: false

    Layout.fillWidth: true
    Layout.preferredHeight: 37
    radius: Theme.radiusSmall
    color: Theme.faint
    border.color: Theme.line
    border.width: 1

    Text {
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: 15
        font.weight: root.filled ? Font.Medium : Font.Normal
        font.family: Theme.fontFamily
        color: root.filled ? Theme.ink : Theme.mid
    }
}
