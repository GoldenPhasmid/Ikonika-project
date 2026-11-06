import QtQuick
import QtQuick.Layouts

Text {
    id: root

    property string label: "Don't know"

    text: root.label
    font.pixelSize: 14
    font.weight: Font.DemiBold
    font.family: Theme.fontFamily
    font.underline: true
    color: "#555555"
    Layout.alignment: Qt.AlignHCenter

    signal clicked()

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
