import QtQuick

Item {
    id: root

    property int value: 0
    property int from: 0
    property int to: 100
    property int step: 1
    property string suffix: ""

    signal edited(int newValue)

    implicitWidth: row.implicitWidth
    implicitHeight: 44

    Row {
        id: row

        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        spacing: 8

        Rectangle {
            id: minus

            width: 44
            height: 44
            radius: 8
            color: minusPress.pressed ? Theme.line : Theme.faint
            opacity: root.value > root.from ? 1.0 : 0.4

            Text {
                anchors.centerIn: parent
                text: "−"
                font.pixelSize: 16
                font.family: Theme.fontFamily
                color: Theme.ink
            }

            MouseArea {
                id: minusPress

                anchors.fill: parent
                enabled: root.value > root.from
                onClicked: root.edited(Math.max(root.from, root.value - root.step))
            }
        }

        Text {
            width: 68
            height: 44
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            text: root.suffix.length > 0 ? root.value + " " + root.suffix : root.value
            font.pixelSize: 15
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
        }

        Rectangle {
            id: plus

            width: 44
            height: 44
            radius: 8
            color: plusPress.pressed ? Theme.line : Theme.faint
            opacity: root.value < root.to ? 1.0 : 0.4

            Text {
                anchors.centerIn: parent
                text: "+"
                font.pixelSize: 16
                font.family: Theme.fontFamily
                color: Theme.ink
            }

            MouseArea {
                id: plusPress

                anchors.fill: parent
                enabled: root.value < root.to
                onClicked: root.edited(Math.min(root.to, root.value + root.step))
            }
        }
    }
}
