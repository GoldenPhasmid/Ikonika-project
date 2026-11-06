import QtQuick
import QtQuick.Layouts

GridLayout {
    id: root

    required property var cardModel

    columns: 2
    rowSpacing: 12
    columnSpacing: 12

    readonly property real cellHeight:
    {
        let tallest = 42
        for (let i = 0; i < children.length; i++)
        {
            const cell = children[i]
            if (cell.textItem)
                tallest = Math.max(tallest, cell.textItem.implicitHeight + 24)
        }
        return tallest
    }

    Repeater {
        model: root.cardModel
        delegate: Rectangle {
            id: card
            property alias textItem: cardText

            Layout.fillWidth: true
            Layout.preferredHeight: root.cellHeight
            radius: Theme.radiusMedium
            color: "#f9f9f9"
            border.color: Theme.line
            border.width: 1

            Text {
                id: cardText
                anchors.centerIn: parent
                width: parent.width - 24
                text: modelData
                font.pixelSize: 13
                font.weight: Font.Medium
                font.family: Theme.fontFamily
                color: Theme.ink
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
