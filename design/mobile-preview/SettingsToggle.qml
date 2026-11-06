import QtQuick

Item {
    id: root

    property bool checked: false

    signal toggled()

    implicitWidth: 46
    implicitHeight: 44

    Rectangle {
        id: track

        anchors.centerIn: parent
        width: 46
        height: 28
        radius: height / 2

        color: root.checked ? Theme.ink : Theme.faint
        border.color: root.checked ? Theme.ink : Theme.outline
        border.width: 2

        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            width: 20
            height: 20
            radius: height / 2
            y: 4
            x: root.checked ? track.width - width - 4 : 4

            color: Theme.panel
            border.color: root.checked ? Theme.panel : Theme.outline
            border.width: root.checked ? 0 : 2

            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.toggled()
    }
}
