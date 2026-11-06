import QtQuick

Rectangle {
    id: root

    required property string label

    signal clicked()

    property int minimumWidth: 112

    implicitWidth: Math.max(root.minimumWidth,
                            value.implicitWidth + chevron.implicitWidth + 30)
    implicitHeight: 36

    radius: Theme.radiusSmall
    color: press.pressed ? Theme.line : Theme.faint

    Behavior on color { ColorAnimation { duration: 90 } }

    Text {
        id: value

        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter

        text: root.label
        font.pixelSize: 15
        font.weight: Font.Medium
        font.family: Theme.fontFamily
        color: Theme.ink
    }

    Text {
        id: chevron

        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter

        text: "▾"
        font.pixelSize: 12
        font.family: Theme.fontFamily
        color: Theme.subtle
    }

    MouseArea {
        id: press

        anchors.fill: parent
        anchors.margins: -4
        onClicked: root.clicked()
    }
}
