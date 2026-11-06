import QtQuick

TextInput {
    id: root

    property string placeholderText: "Search"

    font.pixelSize: 17
    font.weight: Font.DemiBold
    font.family: Theme.fontFamily
    color: Theme.ink
    verticalAlignment: TextInput.AlignVCenter
    clip: true

    Text {
        anchors.fill: parent
        visible: root.text.length === 0
        text: root.placeholderText
        font: root.font
        color: Theme.mid
        verticalAlignment: Text.AlignVCenter
    }
}
