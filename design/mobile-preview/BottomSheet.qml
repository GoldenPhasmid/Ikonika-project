import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property alias heading: headingLabel.text

    default property alias content: body.data

    signal dismissed()

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: Theme.ink
        opacity: 0.4

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: Math.min(parent.height * 0.85, sheetColumn.implicitHeight + 48)
        radius: Theme.radiusLarge
        color: Theme.panel
        clip: true

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            id: sheetColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    id: headingLabel

                    Layout.fillWidth: true
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    color: Theme.ink
                    elide: Text.ElideRight
                }

                Text {
                    text: "✕"
                    font.pixelSize: 16
                    font.family: Theme.fontFamily
                    color: Theme.mid

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        onClicked: root.dismissed()
                    }
                }
            }

            ColumnLayout {
                id: body

                Layout.fillWidth: true
                spacing: 20
            }
        }
    }
}
