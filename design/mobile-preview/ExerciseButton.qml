import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string icon
    required property string label
    required property string subtitle
    required property int count

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 58
    radius: 8
    color: "#f5f5f5"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            radius: 6
            color: Theme.panel

            Text {
                anchors.centerIn: parent
                text: root.icon
                font.pixelSize: 16
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: root.label
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.family: Theme.fontFamily
                color: Theme.ink
            }

            Text {
                text: root.subtitle
                font.pixelSize: 12
                font.family: Theme.fontFamily
                color: Theme.mid
            }
        }

        Text {
            text: root.count
            font.pixelSize: 14
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            color: Theme.ink
            horizontalAlignment: Text.AlignRight
            Layout.alignment: Qt.AlignRight
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
