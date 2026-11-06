import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string label
    property string icon: ""
    property string trailing: ""

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 56
    radius: Theme.radiusMedium
    color: Theme.ink

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16

        Text {
            text: root.icon
            visible: text.length > 0
            font.pixelSize: 16
        }

        Text {
            text: root.label
            font.pixelSize: 17
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: "#ffffff"
            Layout.leftMargin: root.icon.length > 0 ? 8 : 0
            Layout.fillWidth: true
        }

        Text {
            text: root.trailing
            visible: text.length > 0
            font.pixelSize: 17
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: "#ffffff"
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
