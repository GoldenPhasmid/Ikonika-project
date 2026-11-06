import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property string title
    property string description: ""
    property bool lastRow: false
    property bool destructive: false

    default property alias control: controlHolder.data

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: Math.max(60, label.implicitHeight + 28)

    ColumnLayout {
        id: label

        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: controlHolder.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: root.title
            font.pixelSize: 15
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: root.destructive ? Theme.wrong : Theme.ink
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: root.description.length > 0
            text: root.description
            font.pixelSize: 12
            font.family: Theme.fontFamily
            color: Theme.subtle
            wrapMode: Text.WordWrap
        }
    }

    Row {
        id: controlHolder

        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        width: parent.width - 16
        height: 1
        visible: !root.lastRow
        color: Theme.line
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.faint
        opacity: rowPress.pressed ? 1.0 : 0.0
        z: -2

        Behavior on opacity { NumberAnimation { duration: 90 } }
    }

    MouseArea {
        id: rowPress

        anchors.fill: parent
        z: -1
        onClicked: root.clicked()
    }
}
