import QtQuick
import QtQuick.Layouts

BottomSheet {
    id: root

    heading: "Filter"

    property var model: []
    property var selectedKeys: []

    signal toggled(string key)
    signal cleared()

    function isSelected(key)
    {
        return root.selectedKeys.indexOf(key) >= 0
    }

    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(rows.implicitHeight, 280)

        contentWidth: width
        contentHeight: rows.implicitHeight
        clip: true

        Rectangle {
            width: parent.width
            height: rows.implicitHeight

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

                        title: optionRow.modelData.label
                        lastRow: optionRow.index === root.model.length - 1

                        onClicked: root.toggled(optionRow.modelData.key)

                        Checkbox {
                            checked: root.isSelected(optionRow.modelData.key)
                            onToggled: root.toggled(optionRow.modelData.key)
                        }
                    }
                }
            }
        }
    }

    WideButton {
        label: "Clear"
        filled: false
        enabled: root.selectedKeys.length > 0

        onClicked: root.cleared()
    }
}
