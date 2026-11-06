import QtQuick
import QtQuick.Layouts

GridLayout
{
    id: root

    property var choices: []
    property string chosenKey: ""
    property bool selectable: false
    property string namePrefix: ""

    signal activated(string key)

    readonly property bool firstSpans: root.choices.length % root.columns !== 0

    Layout.fillWidth: true

    columns: 2
    rowSpacing: AppMetrics.gapSmall
    columnSpacing: AppMetrics.gapSmall

    Repeater
    {
        model: root.choices

        delegate: DesktopChoiceTile
        {
            required property var modelData
            required property int index

            objectName: root.namePrefix + modelData.key

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            Layout.columnSpan: index === 0 && root.firstSpans ? root.columns : 1

            text: modelData.label
            iconName: modelData.iconName || ""
            description: modelData.caption || ""
            progress: modelData.progress === undefined ? -1 : modelData.progress
            enabled: modelData.enabled === undefined || modelData.enabled
            selectable: root.selectable
            selected: root.selectable && modelData.key === root.chosenKey

            onClicked: root.activated(modelData.key)
        }
    }
}
