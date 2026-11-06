import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopDialog
{
    id: root

    property var choices: []
    property string chosenKey: ""
    property string startLabel: qsTr("Start")
    property bool canStart: true

    property alias settings: settingsColumn.data

    signal choiceActivated(string key)
    signal startRequested()

    DesktopChoiceGrid
    {
        choices: root.choices
        chosenKey: root.chosenKey
        selectable: true
        namePrefix: "start_"

        onActivated: function(key) { root.choiceActivated(key) }
    }

    DesktopDivider
    {
        visible: settingsColumn.children.length > 0
    }

    ColumnLayout
    {
        id: settingsColumn

        Layout.fillWidth: true
        visible: children.length > 0
        spacing: 0
    }

    footer: [
        DesktopButton
        {
            large: true
            tone: "ghost"
            text: qsTr("Cancel")

            onClicked: root.close()
        },
        DesktopButton
        {
            objectName: "start_confirm"

            large: true
            tone: "primary"
            text: root.startLabel
            enabled: root.canStart

            onClicked: root.startRequested()
        }
    ]
}
