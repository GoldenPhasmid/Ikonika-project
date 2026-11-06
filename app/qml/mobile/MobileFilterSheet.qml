import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    heading: qsTr("Filter")

    property var model: []
    property var selectedKeys: []

    signal toggled(string key)
    signal cleared()

    function isSelected(key)
    {
        return root.selectedKeys.indexOf(key) >= 0
    }

    Flickable
    {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(panel.implicitHeight, 280)

        contentWidth: width
        contentHeight: panel.implicitHeight
        clip: true

        MobileOptionListPanel
        {
            id: panel

            width: parent.width

            Repeater
            {
                model: root.model

                delegate: MobileSettingsRow
                {
                    id: optionRow

                    required property var modelData
                    required property int index

                    title: optionRow.modelData.label
                    lastRow: optionRow.index === root.model.length - 1

                    onClicked: root.toggled(optionRow.modelData.key)

                    MobileCheckbox
                    {
                        checked: root.isSelected(optionRow.modelData.key)
                        onToggled: root.toggled(optionRow.modelData.key)
                    }
                }
            }
        }
    }

    MobileWideButton
    {
        label: qsTr("Clear")
        filled: false
        interactive: root.selectedKeys.length > 0

        onClicked: root.cleared()
    }
}
