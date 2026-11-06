import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string placeholder: "Search words..."
    property alias text: input.text

    signal accepted()

    Layout.fillWidth: true
    Layout.preferredHeight: 43
    radius: Theme.radiusMedium
    color: Theme.panel
    border.color: Theme.line
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 8

        Text {
            text: "🔍"
            font.pixelSize: 13
            color: Theme.mid
        }

        TextInput {
            id: input
            Layout.fillWidth: true
            font.pixelSize: 14
            font.family: Theme.fontFamily
            color: Theme.ink
            clip: true

            onAccepted: root.accepted()

            Text {
                anchors.fill: parent
                visible: input.text.length === 0
                text: root.placeholder
                font: input.font
                color: Theme.mid
            }
        }
    }
}
