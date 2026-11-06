import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string posText: ""
    property bool posVisible: true
    property string counterText: ""

    Layout.fillWidth: true

    Text {
        text: root.posText
        visible: root.posVisible
        font.pixelSize: 13
        font.weight: Font.Medium
        font.family: Theme.fontFamily
        color: Theme.mid
    }

    Item { Layout.fillWidth: true }

    Text {
        text: root.counterText
        font.pixelSize: 13
        font.family: Theme.fontFamily
        color: Theme.mid
    }
}
