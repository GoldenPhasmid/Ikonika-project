import QtQuick

Rectangle {
    id: root

    required property string icon
    property bool holdToConfirm: false
    property int holdDuration: 600
    property color tint: Theme.faint
    property color iconColor: Theme.ink

    signal triggered()

    property real fillProgress: 0

    color: root.tint

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * root.fillProgress
        color: Qt.rgba(1, 1, 1, 0.28)
        visible: root.holdToConfirm
    }

    Text {
        anchors.centerIn: parent
        text: root.icon
        font.pixelSize: 18
        font.family: Theme.fontFamily
        color: root.iconColor
    }

    NumberAnimation {
        id: fillUp
        target: root
        property: "fillProgress"
        to: 1.0
        duration: root.holdDuration

        onFinished: { root.fillProgress = 0; root.triggered() }
    }

    NumberAnimation {
        id: fillDown
        target: root
        property: "fillProgress"
        to: 0.0
        duration: 120
    }

    TapHandler {
        enabled: !root.holdToConfirm
        onTapped: root.triggered()
    }

    TapHandler {
        enabled: root.holdToConfirm

        onPressedChanged:
        {
            if (pressed)
            {
                fillDown.stop()
                fillUp.start()
            }
            else
            {
                fillUp.stop()
                fillDown.start()
            }
        }
    }
}
