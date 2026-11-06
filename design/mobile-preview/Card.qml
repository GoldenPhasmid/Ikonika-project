import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    default property alias content: contentColumn.data
    property alias spacing: contentColumn.spacing

    Layout.fillWidth: true
    implicitHeight: contentColumn.implicitHeight + 32
    height: implicitHeight

    radius: Theme.radiusMedium
    color: Theme.panel
    border.color: Theme.line
    border.width: 1

    ColumnLayout {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8
    }
}
