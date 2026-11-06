import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string label
    required property int level
    required property bool selected

    Layout.fillWidth: true
    Layout.preferredHeight: 44
    radius: Theme.radiusMedium
    color: root.selected ? Theme.ink : Theme.panel

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10

        Text {
            text: root.label
            font.pixelSize: 14
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: root.selected ? "#ffffff" : Theme.ink
            Layout.fillWidth: true
        }

        SignalBars {
            level: root.level
            activeColor: root.selected ? "#ffffff" : Theme.ink
            inactiveColor: root.selected ? "#555555" : Theme.line
        }
    }
}
