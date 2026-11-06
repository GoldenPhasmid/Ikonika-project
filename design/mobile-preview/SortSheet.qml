import QtQuick
import QtQuick.Layouts

BottomSheet {
    id: root

    heading: "Sort"

    property var model: []
    property string currentKey: ""
    property bool descending: false

    signal chosen(string key, bool descending)

    readonly property var activeOption:
    {
        for (const option of root.model)
        {
            if (option.key === root.currentKey)
            {
                return option
            }
        }

        return null
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: Theme.radiusSmall
            color: root.descending ? Theme.faint : Theme.ink

            Text {
                anchors.centerIn: parent
                text: root.activeOption ? root.activeOption.primary : ""
                font.pixelSize: 14
                font.weight: Font.Medium
                font.family: Theme.fontFamily
                color: root.descending ? Theme.ink : "#ffffff"
            }

            TapHandler { onTapped: root.chosen(root.currentKey, false) }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: Theme.radiusSmall
            color: root.descending ? Theme.ink : Theme.faint

            Text {
                anchors.centerIn: parent
                text: root.activeOption ? root.activeOption.inverted : ""
                font.pixelSize: 14
                font.weight: Font.Medium
                font.family: Theme.fontFamily
                color: root.descending ? "#ffffff" : Theme.ink
            }

            TapHandler { onTapped: root.chosen(root.currentKey, true) }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: rows.implicitHeight

        radius: Theme.radiusMedium
        color: Theme.panel
        border.color: Theme.line
        border.width: 1
        clip: true

        ColumnLayout {
            id: rows

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            Repeater {
                model: root.model

                delegate: SettingsRow {
                    id: optionRow

                    required property var modelData
                    required property int index

                    title: optionRow.modelData.primary
                    lastRow: optionRow.index === root.model.length - 1

                    onClicked: root.chosen(optionRow.modelData.key, false)

                    Text {
                        visible: optionRow.modelData.key === root.currentKey
                        text: "✓"
                        font.pixelSize: 16
                        font.family: Theme.fontFamily
                        color: Theme.ink
                    }
                }
            }
        }
    }
}
