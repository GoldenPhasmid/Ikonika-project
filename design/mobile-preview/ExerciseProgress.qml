import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property real progress: 0

    Layout.fillWidth: true
    Layout.preferredHeight: 4

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: Theme.faint
    }

    Rectangle {
        width: parent.width * Math.max(0, Math.min(1, root.progress))
        height: parent.height
        radius: 2
        color: Theme.ink
    }
}
